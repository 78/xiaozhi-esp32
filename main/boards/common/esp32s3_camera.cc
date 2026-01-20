#include "sdkconfig.h"

// esp32s3_camera (使用 esp_camera 组件) 仅用于 ESP32-S3 且选择使用 esp_camera 时
#if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(CONFIG_XIAOZHI_USE_ESP_CAMERA)

#include <esp_heap_caps.h>
#include <cstdio>
#include <cstring>
#include <esp_log.h>

#include "esp32s3_camera.h"
#include "board.h"
#include "display.h"
#include "lvgl_display.h"
#include "mcp_server.h"
#include "system_info.h"
#include "jpg/image_to_jpeg.h"

#define TAG "Esp32S3Camera"

// V4L2 兼容的格式定义
#define V4L2_PIX_FMT_RGB565 0x50424752 // 'RGBP'
#define V4L2_PIX_FMT_YUYV 0x56595559   // 'YUYV'
#define V4L2_PIX_FMT_JPEG 0x4745504A   // 'JPEG'
#define V4L2_PIX_FMT_RGB24 0x33424752  // 'RGB3'
#define V4L2_PIX_FMT_GREY 0x59455247   // 'GREY'

static uint32_t pixformat_to_v4l2(pixformat_t fmt)
{
    switch (fmt)
    {
    case PIXFORMAT_RGB565:
        return V4L2_PIX_FMT_RGB565;
    case PIXFORMAT_YUV422:
        return V4L2_PIX_FMT_YUYV;
    case PIXFORMAT_JPEG:
        return V4L2_PIX_FMT_JPEG;
    case PIXFORMAT_RGB888:
        return V4L2_PIX_FMT_RGB24;
    case PIXFORMAT_GRAYSCALE:
        return V4L2_PIX_FMT_GREY;
    default:
        return 0;
    }
}

Esp32S3Camera::Esp32S3Camera(const camera_config_t &config)
{
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_camera_init failed with error 0x%x", err);
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s)
    {
        frame_.width = config.frame_size == FRAMESIZE_QVGA ? 320 : config.frame_size == FRAMESIZE_VGA ? 640
                                                               : config.frame_size == FRAMESIZE_SVGA  ? 800
                                                               : config.frame_size == FRAMESIZE_XGA   ? 1024
                                                               : config.frame_size == FRAMESIZE_HD    ? 1280
                                                               : config.frame_size == FRAMESIZE_SXGA  ? 1280
                                                               : config.frame_size == FRAMESIZE_UXGA  ? 1600
                                                                                                      : 320;
        frame_.height = config.frame_size == FRAMESIZE_QVGA ? 240 : config.frame_size == FRAMESIZE_VGA ? 480
                                                                : config.frame_size == FRAMESIZE_SVGA  ? 600
                                                                : config.frame_size == FRAMESIZE_XGA   ? 768
                                                                : config.frame_size == FRAMESIZE_HD    ? 720
                                                                : config.frame_size == FRAMESIZE_SXGA  ? 1024
                                                                : config.frame_size == FRAMESIZE_UXGA  ? 1200
                                                                                                       : 240;
        frame_.format = config.pixel_format;
        ESP_LOGI(TAG, "Camera initialized: %dx%d, format=%d", frame_.width, frame_.height, config.pixel_format);
    }

    streaming_on_ = true;
    ESP_LOGI(TAG, "ESP32-S3 Camera init success");
}

Esp32S3Camera::~Esp32S3Camera()
{
    if (streaming_on_)
    {
        if (current_fb_)
        {
            esp_camera_fb_return(current_fb_);
            current_fb_ = nullptr;
        }
        esp_camera_deinit();
        streaming_on_ = false;
    }
    if (frame_.data)
    {
        heap_caps_free(frame_.data);
        frame_.data = nullptr;
    }
}

void Esp32S3Camera::SetExplainUrl(const std::string &url, const std::string &token)
{
    explain_url_ = url;
    explain_token_ = token;
}

bool Esp32S3Camera::Capture()
{
    if (encoder_thread_.joinable())
    {
        encoder_thread_.join();
    }

    if (!streaming_on_)
    {
        return false;
    }

    // 释放之前的帧
    if (current_fb_)
    {
        esp_camera_fb_return(current_fb_);
        current_fb_ = nullptr;
    }

    // 丢弃前两帧，获取最新帧
    for (int i = 0; i < 3; i++)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            return false;
        }
        if (i < 2)
        {
            esp_camera_fb_return(fb);
        }
        else
        {
            current_fb_ = fb;
        }
    }

    if (!current_fb_)
    {
        ESP_LOGE(TAG, "Failed to get frame buffer");
        return false;
    }

    // 保存帧副本到 PSRAM
    if (frame_.data)
    {
        heap_caps_free(frame_.data);
        frame_.data = nullptr;
    }

    frame_.len = current_fb_->len;
    frame_.width = current_fb_->width;
    frame_.height = current_fb_->height;
    frame_.format = current_fb_->format;

    frame_.data = (uint8_t *)heap_caps_malloc(frame_.len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!frame_.data)
    {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for frame copy", frame_.len);
        esp_camera_fb_return(current_fb_);
        current_fb_ = nullptr;
        return false;
    }
    memcpy(frame_.data, current_fb_->buf, frame_.len);

    // 释放原始帧
    esp_camera_fb_return(current_fb_);
    current_fb_ = nullptr;

    // 对 RGB565 格式进行字节交换 (Big Endian <-> Little Endian)
    // 这样 frame_.data 就是已交换的数据，显示和上传都使用相同的数据
    if (frame_.format == PIXFORMAT_RGB565)
    {
        uint8_t *data = frame_.data;
        size_t pixel_count = frame_.width * frame_.height;
        for (size_t i = 0; i < pixel_count; i++)
        {
            uint8_t temp = data[2 * i];
            data[2 * i] = data[2 * i + 1];
            data[2 * i + 1] = temp;
        }
    }

    ESP_LOGD(TAG, "Captured frame: %dx%d, len=%zu, format=%d",
             frame_.width, frame_.height, frame_.len, frame_.format);

    // 显示预览图片
    auto display = dynamic_cast<LvglDisplay *>(Board::GetInstance().GetDisplay());
    if (display != nullptr)
    {
        if (!frame_.data)
        {
            ESP_LOGE(TAG, "frame.data is null");
            return false;
        }

        uint16_t w = frame_.width;
        uint16_t h = frame_.height;
        size_t lvgl_image_size = frame_.len;
        size_t stride = ((w * 2) + 3) & ~3; // 4字节对齐
        lv_color_format_t color_format = LV_COLOR_FORMAT_RGB565;
        uint8_t *data = nullptr;

        switch (frame_.format)
        {
        case PIXFORMAT_RGB565:
            // frame_.data 已经在捕获阶段完成了字节交换，直接复制即可
            data = (uint8_t *)heap_caps_malloc(w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (data == nullptr)
            {
                ESP_LOGE(TAG, "Failed to allocate memory for preview image");
                return false;
            }
            memcpy(data, frame_.data, frame_.len);
            lvgl_image_size = frame_.len;
            break;

        case PIXFORMAT_JPEG:
            // JPEG 格式需要解码 - 跳过预览显示
            ESP_LOGD(TAG, "JPEG format preview not supported, skipping display");
            return true;

        default:
            ESP_LOGE(TAG, "Unsupported frame format for preview: %d", frame_.format);
            return true; // 仍然返回 true，因为捕获成功
        }

        if (data)
        {
            auto image = std::make_unique<LvglAllocatedImage>(data, lvgl_image_size, w, h, stride, color_format);
            display->SetPreviewImage(std::move(image));
        }
    }
    return true;
}

bool Esp32S3Camera::SetHMirror(bool enabled)
{
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        return false;
    }
    s->set_hmirror(s, enabled ? 1 : 0);
    return true;
}

bool Esp32S3Camera::SetVFlip(bool enabled)
{
    sensor_t *s = esp_camera_sensor_get();
    if (!s)
    {
        return false;
    }
    s->set_vflip(s, enabled ? 1 : 0);
    return true;
}

std::string Esp32S3Camera::Explain(const std::string &question)
{
    if (explain_url_.empty())
    {
        throw std::runtime_error("Image explain URL or token is not set");
    }

    // 创建局部的 JPEG 队列
    QueueHandle_t jpeg_queue = xQueueCreate(40, sizeof(JpegChunk));
    if (jpeg_queue == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create JPEG queue");
        throw std::runtime_error("Failed to create JPEG queue");
    }

    // 转换格式为 v4l2 兼容格式
    uint32_t v4l2_format = pixformat_to_v4l2(frame_.format);

    // 启动编码线程
    encoder_thread_ = std::thread([this, jpeg_queue, v4l2_format]()
                                  {
        uint16_t w = frame_.width ? frame_.width : 320;
        uint16_t h = frame_.height ? frame_.height : 240;
        bool ok = image_to_jpeg_cb(
            frame_.data, frame_.len, w, h, static_cast<v4l2_pix_fmt_t>(v4l2_format), 80,
            [](void* arg, size_t index, const void* data, size_t len) -> size_t {
                auto jpeg_queue = static_cast<QueueHandle_t>(arg);
                JpegChunk chunk = {.data = nullptr, .len = len};
                if (index == 0 && data != nullptr && len > 0) {
                    chunk.data = (uint8_t*)heap_caps_aligned_alloc(16, len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                    if (chunk.data == nullptr) {
                        ESP_LOGE(TAG, "Failed to allocate %zu bytes for JPEG chunk", len);
                        chunk.len = 0;
                    } else {
                        memcpy(chunk.data, data, len);
                    }
                } else {
                    chunk.len = 0;
                }
                xQueueSend(jpeg_queue, &chunk, portMAX_DELAY);
                return len;
            },
            jpeg_queue);

        if (!ok) {
            JpegChunk chunk = {.data = nullptr, .len = 0};
            xQueueSend(jpeg_queue, &chunk, portMAX_DELAY);
        } });

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(3);
    std::string boundary = "----ESP32_CAMERA_BOUNDARY";

    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
    if (!explain_token_.empty())
    {
        http->SetHeader("Authorization", "Bearer " + explain_token_);
    }
    http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http->SetHeader("Transfer-Encoding", "chunked");
    if (!http->Open("POST", explain_url_))
    {
        ESP_LOGE(TAG, "Failed to connect to explain URL");
        encoder_thread_.join();
        JpegChunk chunk;
        while (xQueueReceive(jpeg_queue, &chunk, portMAX_DELAY) == pdPASS)
        {
            if (chunk.data != nullptr)
            {
                heap_caps_free(chunk.data);
            }
            else
            {
                break;
            }
        }
        vQueueDelete(jpeg_queue);
        throw std::runtime_error("Failed to connect to explain URL");
    }

    {
        std::string question_field;
        question_field += "--" + boundary + "\r\n";
        question_field += "Content-Disposition: form-data; name=\"question\"\r\n";
        question_field += "\r\n";
        question_field += question + "\r\n";
        http->Write(question_field.c_str(), question_field.size());
    }
    {
        std::string file_header;
        file_header += "--" + boundary + "\r\n";
        file_header += "Content-Disposition: form-data; name=\"file\"; filename=\"camera.jpg\"\r\n";
        file_header += "Content-Type: image/jpeg\r\n";
        file_header += "\r\n";
        http->Write(file_header.c_str(), file_header.size());
    }

    size_t total_sent = 0;
    bool saw_terminator = false;
    while (true)
    {
        JpegChunk chunk;
        if (xQueueReceive(jpeg_queue, &chunk, portMAX_DELAY) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to receive JPEG chunk");
            break;
        }
        if (chunk.data == nullptr)
        {
            saw_terminator = true;
            break;
        }
        http->Write((const char *)chunk.data, chunk.len);
        total_sent += chunk.len;
        heap_caps_free(chunk.data);
    }
    encoder_thread_.join();
    vQueueDelete(jpeg_queue);

    if (!saw_terminator || total_sent == 0)
    {
        ESP_LOGE(TAG, "JPEG encoder failed or produced empty output");
        throw std::runtime_error("Failed to encode image to JPEG");
    }

    {
        std::string multipart_footer;
        multipart_footer += "\r\n--" + boundary + "--\r\n";
        http->Write(multipart_footer.c_str(), multipart_footer.size());
    }
    http->Write("", 0);

    if (http->GetStatusCode() != 200)
    {
        ESP_LOGE(TAG, "Failed to upload photo, status code: %d", http->GetStatusCode());
        throw std::runtime_error("Failed to upload photo");
    }

    std::string result = http->ReadAll();
    http->Close();

    size_t remain_stack_size = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG, "Explain image size=%d bytes, compressed size=%d, remain stack size=%d, question=%s\n%s",
             (int)frame_.len, (int)total_sent, (int)remain_stack_size, question.c_str(), result.c_str());
    return result;
}

#endif // CONFIG_IDF_TARGET_ESP32S3 && CONFIG_XIAOZHI_USE_ESP_CAMERA
