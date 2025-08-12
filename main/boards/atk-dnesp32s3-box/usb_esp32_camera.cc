#include "usb_esp32_camera.h"
#include "mcp_server.h"
#include "display.h"
#include "board.h"
#include "system_info.h"
#include "nvs_flash.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <img_converters.h>
#include "usb_stream.h"
#include <cstring>

#define TAG "USB_Esp32Camera"

static camera_resolution_info_t camera_resolution_info = {0};
static uint8_t *jpg_frame_buf1                         = nullptr;
static uint8_t *jpg_frame_buf2                         = nullptr;
static uint8_t *xfer_buffer_a                          = nullptr;
static uint8_t *xfer_buffer_b                          = nullptr;
static uint8_t *frame_buffer                           = nullptr;
static PingPongBuffer_t *ppbuffer_handle               = nullptr;
static uint16_t current_width                          = 0;
static uint16_t current_height                         = 0;
static bool if_ppbuffer_init                           = false;
jpeg_dec_io_t *jpeg_io = NULL;
jpeg_dec_header_info_t *out_info = NULL;
jpeg_dec_handle_t jpeg_dec = NULL;
int out_len = 0;
uint8_t *decode_frame_buffer = nullptr; /* 视频缓冲区 */
JpegData jpeg_data_;

jpeg_error_t esp_jpeg_decode_one_picture(uint8_t *input_buf, int len, uint8_t **output_buf, int *out_len)
{
    jpeg_error_t ret = JPEG_ERR_OK;
    // Generate default configuration
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    config.rotate = JPEG_ROTATE_0D;

    // Create jpeg_dec handle
    
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK) {
        return ret;
    }

    // Create io_callback handle
    jpeg_io = (jpeg_dec_io_t *)heap_caps_aligned_alloc(16, sizeof(jpeg_dec_io_t),MALLOC_CAP_SPIRAM);
    if (jpeg_io == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_dec_failed;
    }

    // Create out_info handle
    out_info = (jpeg_dec_header_info_t *)heap_caps_aligned_alloc(16, sizeof(jpeg_dec_header_info_t),MALLOC_CAP_SPIRAM);
    if (out_info == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_dec_failed;
    }

    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret != JPEG_ERR_OK) {
        goto jpeg_dec_failed;
    }

    jpeg_io->outbuf = *output_buf;

    // Start decode jpeg
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret != JPEG_ERR_OK) {
        goto jpeg_dec_failed;
    }

    // Decoder deinitialize
jpeg_dec_failed:
    jpeg_dec_close(jpeg_dec);
    jpeg_dec = nullptr;
    if (jpeg_io) {
        heap_caps_aligned_free(jpeg_io);
        jpeg_io = nullptr;
    }
    if (out_info) {
        heap_caps_aligned_free(out_info);
        out_info = nullptr;
    }

    return ret;
}

/**
 * @brief       摄像头回调函数
 * @param       frame       :从UVC设备接收到的图像帧
 * @param       ptr         :转入参数（未使用）
 * @retval      无
 */
static void camera_frame_cb(uvc_frame_t *frame, void *ptr)
{
    jpeg_data_.fb_buf = (uint8_t *)frame->data;    /* 获取图像数据 */
    jpeg_data_.fb_buf_size =  frame->data_bytes;   /* 计算缓冲区大小 */
    esp_jpeg_decode_one_picture((uint8_t *)frame->data, frame->data_bytes, &decode_frame_buffer, &out_len);   /* 解码一帧 */
    vTaskDelay(pdMS_TO_TICKS(1));
}

/**
 * @brief       在nvs分区获取数值
 * @param       key     :名称
 * @param       value   :数据
 * @param       size    :大小
 * @retval      无
 */
void usb_get_value_from_nvs(char *key, void *value, size_t *size)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("memory", NVS_READWRITE, &my_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        err = nvs_get_blob(my_handle, key, value, size);
        switch (err)
        {
            case ESP_OK:
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG, "%s is not initialized yet!", key);
                break;
            default :
                ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
        }

        nvs_close(my_handle);
    }
}

/**
 * @brief       在nvs分区保存数值
 * @param       key     :名称
 * @param       value   :数据
 * @param       size    :大小
 * @retval      ESP_OK：设置成功；其他表示获取失败
 */
esp_err_t usb_set_value_to_nvs(char *key, void *value, size_t size)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("memory", NVS_READWRITE, &my_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    }
    else
    {
        err = nvs_set_blob(my_handle, key, value, size);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "NVS set failed %s", esp_err_to_name(err));
        }

        err = nvs_commit(my_handle);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "NVS commit failed");
        }

        nvs_close(my_handle);
    }

    return err;
}

/**
 * @brief       USB数据流初始化
 * @param       无
 * @retval      ESP_OK：成功初始化；其他表示初始化失败
 */
esp_err_t usb_stream_init(void)
{
    uvc_config_t uvc_config = {};
    uvc_config.frame_interval = FRAME_INTERVAL_FPS_5;
    uvc_config.xfer_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE;
    uvc_config.xfer_buffer_a = xfer_buffer_a;
    uvc_config.xfer_buffer_b = xfer_buffer_b;
    uvc_config.frame_buffer_size = DEMO_UVC_XFER_BUFFER_SIZE;
    uvc_config.frame_buffer = frame_buffer;
    uvc_config.frame_cb = &camera_frame_cb;
    uvc_config.frame_cb_arg = NULL;
    uvc_config.frame_width = FRAME_RESOLUTION_ANY;
    uvc_config.frame_height = FRAME_RESOLUTION_ANY;
    uvc_config.flags = FLAG_UVC_SUSPEND_AFTER_START;

    esp_err_t ret = uvc_streaming_config(&uvc_config);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "uvc streaming config failed");
    }
    return ret;
}

/**
 * @brief       查找USB摄像头当前的分辨率
 * @param       camera_frame_size :结构体
 * @retval      返回分辨率
 */
size_t usb_camera_find_current_resolution(camera_frame_size_t *camera_frame_size)
{
    if (camera_resolution_info.camera_frame_list == NULL)
    {
        return -1;
    }

    size_t i = 0;
    while (i < camera_resolution_info.camera_frame_list_num)
    {
        if (camera_frame_size->width >= camera_resolution_info.camera_frame_list[i].width && camera_frame_size->height >= camera_resolution_info.camera_frame_list[i].height)
        {
            /* 查找下一个分辨率
               如果当前的分辨率最小，则切换到大的分辨率*/
            camera_frame_size->width = camera_resolution_info.camera_frame_list[i].width;
            camera_frame_size->height = camera_resolution_info.camera_frame_list[i].height;
            break;
        }
        else if (i == camera_resolution_info.camera_frame_list_num - 1)
        {
            camera_frame_size->width = camera_resolution_info.camera_frame_list[i].width;
            camera_frame_size->height = camera_resolution_info.camera_frame_list[i].height;
            break;
        }
        i++;
    }
    /* 打印当前分辨率 */
    ESP_LOGI(TAG, "Current resolution is %dx%d", camera_frame_size->width, camera_frame_size->height);
    return i;
}

/**
 * @brief       usb数据流回调函数
 * @param       event   : 事件
 * @param       arg     : 参数（未使用）
 * @retval      无
 */
static void usb_stream_state_changed_cd(usb_stream_state_t event,void *arg)
{
    switch(event)
    {
        /* 连接状态 */
        case STREAM_CONNECTED:
        {
            /* 获取相机分辨率，并存储至nvs分区 */
            size_t size = sizeof(camera_frame_size_t);
            usb_get_value_from_nvs(DEMO_KEY_RESOLUTION, &camera_resolution_info.camera_frame_size, &size);
            size_t frame_index = 0;
            uvc_frame_size_list_get(NULL, &camera_resolution_info.camera_frame_list_num, NULL);

            if (camera_resolution_info.camera_frame_list_num)
            {
                ESP_LOGI(TAG, "UVC: get frame list size = %u, current = %u", camera_resolution_info.camera_frame_list_num, frame_index);
                uvc_frame_size_t *_frame_list = (uvc_frame_size_t *)heap_caps_aligned_alloc(16, camera_resolution_info.camera_frame_list_num * sizeof(uvc_frame_size_t),MALLOC_CAP_SPIRAM);

                camera_resolution_info.camera_frame_list = (uvc_frame_size_t *)heap_caps_realloc(camera_resolution_info.camera_frame_list, camera_resolution_info.camera_frame_list_num * sizeof(uvc_frame_size_t),MALLOC_CAP_SPIRAM);
                
                if (NULL == camera_resolution_info.camera_frame_list)
                {
                    ESP_LOGE(TAG, "camera_resolution_info.camera_frame_list");
                }

                uvc_frame_size_list_get(_frame_list, NULL, NULL);

                for (size_t i = 0; i < camera_resolution_info.camera_frame_list_num; i++)
                {
                    if (_frame_list[i].width <= 480 && _frame_list[i].height <= 320)    /* 设置USB摄像头分辨率 */
                    {
                        camera_resolution_info.camera_frame_list[frame_index++] = _frame_list[i];
                        ESP_LOGI(TAG, "\tpick frame[%u] = %ux%u", i, _frame_list[i].width, _frame_list[i].height);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "\tdrop frame[%u] = %ux%u", i, _frame_list[i].width, _frame_list[i].height);
                    }
                }
                camera_resolution_info.camera_frame_list_num = frame_index;

                if(camera_resolution_info.camera_frame_size.width != 0 && camera_resolution_info.camera_frame_size.height != 0) {
                    camera_resolution_info.camera_currect_frame_index = usb_camera_find_current_resolution(&camera_resolution_info.camera_frame_size);
                }
                else
                {
                    camera_resolution_info.camera_currect_frame_index = 0;
                }

                if (-1 == camera_resolution_info.camera_currect_frame_index)
                {
                    ESP_LOGE(TAG, "fine current resolution fail");
                    break;
                }
                ESP_ERROR_CHECK(uvc_frame_size_reset(camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].width,
                                                    camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].height, FPS2INTERVAL(30)));
                camera_frame_size_t camera_frame_size = {
                    .width = camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].width,
                    .height = camera_resolution_info.camera_frame_list[camera_resolution_info.camera_currect_frame_index].height,
                };

                ESP_ERROR_CHECK(usb_set_value_to_nvs(DEMO_KEY_RESOLUTION, &camera_frame_size, sizeof(camera_frame_size_t)));

                if (_frame_list != NULL)
                {
                    heap_caps_aligned_free(_frame_list);
                }
                /* 等待USB摄像头连接 */
                usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL);
            }
            else
            {
                ESP_LOGW(TAG, "UVC: get frame list size = %u", camera_resolution_info.camera_frame_list_num);
            }
            /* 设备连接成功 */
            ESP_LOGI(TAG, "Device connected");
            break;
        }
        /* 关闭连接 */
        case STREAM_DISCONNECTED:
        {
                /* 设备断开 */
                ESP_LOGI(TAG, "Device disconnected");
            break;
        }
    }
}

USB_Esp32Camera::USB_Esp32Camera() {
    
    xfer_buffer_a = (uint8_t *)heap_caps_aligned_alloc(16, DEMO_UVC_XFER_BUFFER_SIZE,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(xfer_buffer_a != NULL);
    xfer_buffer_b = (uint8_t *)heap_caps_aligned_alloc(16, DEMO_UVC_XFER_BUFFER_SIZE,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(xfer_buffer_b != NULL);
    frame_buffer = (uint8_t *)heap_caps_aligned_alloc(16, DEMO_UVC_XFER_BUFFER_SIZE,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(frame_buffer != NULL);
    decode_frame_buffer = (uint8_t *)heap_caps_aligned_alloc(16,240 * 320 * 2,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(decode_frame_buffer != NULL);
    
    /* USB数据流初始化 */
    ESP_ERROR_CHECK(usb_stream_init());
    /* 注册回调函数 */
    ESP_ERROR_CHECK(usb_streaming_state_register(&usb_stream_state_changed_cd, NULL));
    /* 开启USB数据流转输  */
    ESP_ERROR_CHECK(usb_streaming_start());
    /* 等待连接 */
    // ESP_ERROR_CHECK(usb_streaming_connect_wait(portMAX_DELAY));

    memset(&preview_image_, 0, sizeof(preview_image_));
    preview_image_.header.magic = LV_IMAGE_HEADER_MAGIC;
    preview_image_.header.cf = LV_COLOR_FORMAT_RGB565;
    preview_image_.header.flags = LV_IMAGE_FLAGS_ALLOCATED | LV_IMAGE_FLAGS_MODIFIABLE;
    preview_image_.header.w = 480;
    preview_image_.header.h = 320;
    preview_image_.header.stride = preview_image_.header.w * 2;
    preview_image_.data_size = preview_image_.header.w * preview_image_.header.h * 2;
    preview_image_.data = (uint8_t*)heap_caps_malloc(preview_image_.data_size, MALLOC_CAP_SPIRAM);
    if (preview_image_.data == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for preview image");
        return;
    }
}

USB_Esp32Camera::~USB_Esp32Camera() {
    usb_streaming_stop();
    if (xfer_buffer_a) {
        heap_caps_aligned_free(xfer_buffer_a);
        xfer_buffer_a = nullptr;
    }
    if (xfer_buffer_b) {
        heap_caps_aligned_free(xfer_buffer_b);
        xfer_buffer_b = nullptr;
    }
    if (frame_buffer) {
        heap_caps_aligned_free(frame_buffer);
        frame_buffer = nullptr;
    }
    if (decode_frame_buffer) {
        heap_caps_aligned_free(decode_frame_buffer);
        decode_frame_buffer = nullptr;
    }
}

void USB_Esp32Camera::SetExplainUrl(const std::string& url, const std::string& token) {
    explain_url_ = url;
    explain_token_ = token;
}

bool USB_Esp32Camera::Capture() {
    if (encoder_thread_.joinable()) {
        encoder_thread_.join();
    }

    // 显示预览图片
    auto display = Board::GetInstance().GetDisplay();
    if (display != nullptr) {
        preview_image_.data = (uint8_t *)decode_frame_buffer;
        display->SetPreviewImage(&preview_image_);
    }
    return true;
}

bool USB_Esp32Camera::SetHMirror(bool enabled) {
    return false;
}

bool USB_Esp32Camera::SetVFlip(bool enabled) {
    return false;
}
/**
 * @brief 将摄像头捕获的图像发送到远程服务器进行AI分析和解释
 * 
 * 该函数将当前摄像头缓冲区中的图像编码为JPEG格式，并通过HTTP POST请求
 * 以multipart/form-data的形式发送到指定的解释服务器。服务器将根据提供的
 * 问题对图像进行AI分析并返回结果。
 * 
 * 实现特点：
 * - 使用独立线程编码JPEG，与主线程分离
 * - 采用分块传输编码(chunked transfer encoding)优化内存使用
 * - 通过队列机制实现编码线程和发送线程的数据同步
 * - 支持设备ID、客户端ID和认证令牌的HTTP头部配置
 * 
 * @param question 要向AI提出的关于图像的问题，将作为表单字段发送
 * @return std::string 服务器返回的JSON格式响应字符串
 *         成功时包含AI分析结果，失败时包含错误信息
 *         格式示例：{"success": true, "result": "分析结果"}
 *                  {"success": false, "message": "错误信息"}
 * 
 * @note 调用此函数前必须先调用SetExplainUrl()设置服务器URL
 * @note 函数会等待之前的编码线程完成后再开始新的处理
 * @warning 如果摄像头缓冲区为空或网络连接失败，将返回错误信息
 */
std::string USB_Esp32Camera::Explain(const std::string& question) {
    if (explain_url_.empty()) {
        return "{\"success\": false, \"message\": \"Image explain URL or token is not set\"}";
    }

    auto http = Board::GetInstance().CreateHttp();
    // 构造multipart/form-data请求体
    std::string boundary = "----ESP32_CAMERA_BOUNDARY";
    
    // 构造question字段
    std::string question_field;
    question_field += "--" + boundary + "\r\n";
    question_field += "Content-Disposition: form-data; name=\"question\"\r\n";
    question_field += "\r\n";
    question_field += question + "\r\n";
    
    // 构造文件字段头部
    std::string file_header;
    file_header += "--" + boundary + "\r\n";
    file_header += "Content-Disposition: form-data; name=\"file\"; filename=\"camera.jpg\"\r\n";
    file_header += "Content-Type: image/jpeg\r\n";
    file_header += "\r\n";
    
    // 构造尾部
    std::string multipart_footer;
    multipart_footer += "\r\n--" + boundary + "--\r\n";

    // 配置HTTP客户端，使用分块传输编码
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
    if (!explain_token_.empty()) {
        http->SetHeader("Authorization", "Bearer " + explain_token_);
    }
    http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http->SetHeader("Transfer-Encoding", "chunked");
    if (!http->Open("POST", explain_url_)) {
        ESP_LOGE(TAG, "Failed to connect to explain URL");
        return "{\"success\": false, \"message\": \"Failed to connect to explain URL\"}";
    }
    
    // 第一块：question字段
    http->Write(question_field.c_str(), question_field.size());
    
    // 第二块：文件字段头部
    http->Write(file_header.c_str(), file_header.size());
    
    // 第三块：JPEG数据
    http->Write((const char*)jpeg_data_.fb_buf, jpeg_data_.fb_buf_size);

    // 第四块：multipart尾部
    http->Write(multipart_footer.c_str(), multipart_footer.size());
    
    // 结束块
    http->Write("", 0);

    if (http->GetStatusCode() != 200) {
        return "{\"success\": false, \"message\": \"Failed to upload photo\"}";
    }

    std::string result = http->ReadAll();
    http->Close();

    return result;
}
