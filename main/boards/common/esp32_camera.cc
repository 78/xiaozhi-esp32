#include "sdkconfig.h"

#include <esp_heap_caps.h>
#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <img_converters.h>

#include "esp32_camera.h"
#include "board.h"
#include "display.h"
#include "lvgl_display.h"
#include "mcp_server.h"
#include "system_info.h"
#include "jpg/image_to_jpeg.h"

#define TAG "Esp32Camera"

Esp32Camera::Esp32Camera(const camera_config_t &config) {
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed with error 0x%x", err);
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        if (s->id.PID == GC0308_PID) {
            s->set_hmirror(s, 0); // 控制摄像头镜像 写1镜像 写0不镜像
        }

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

        // 预分配帧缓存
        size_t pixel_size = (frame_.format == PIXFORMAT_RGB565 || frame_.format == PIXFORMAT_YUV422) ? 2 : 3;
        frame_.len = frame_.width * frame_.height * pixel_size;
        frame_.data = (uint8_t *)heap_caps_malloc(frame_.len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (frame_.data == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate memory for frame buffer");
        }
    }

    streaming_on_ = true;
    ESP_LOGI(TAG, "ESP32-S3 Camera init success");
}

Esp32Camera::~Esp32Camera() {
    if (streaming_on_) {
        if (current_fb_) {
            esp_camera_fb_return(current_fb_);
            current_fb_ = nullptr;
        }
        esp_camera_deinit();
        streaming_on_ = false;
    }
    if (frame_.data) {
        heap_caps_free(frame_.data);
        frame_.data = nullptr;
    }
}

void Esp32Camera::SetExplainUrl(const std::string &url, const std::string &token) {
    explain_url_ = url;
    explain_token_ = token;
}

bool Esp32Camera::Capture() {
    if (encoder_thread_.joinable()) {
        encoder_thread_.join();
    }

    if (!streaming_on_) {
        return false;
    }

    // 获取最新帧，丢弃旧帧以保证实时性
    for (int i = 0; i < 2; i++) {
        if (current_fb_) {
            esp_camera_fb_return(current_fb_);
        }
        current_fb_ = esp_camera_fb_get();
        if (!current_fb_) {
            ESP_LOGE(TAG, "Camera capture failed");
            return false;
        }
    }

    // 检查预分配缓存
    if (frame_.data == nullptr) {
        ESP_LOGE(TAG, "Camera buffers not initialized");
        return false;
    }

    // 备份当前帧参数
    frame_.width = current_fb_->width;
    frame_.height = current_fb_->height;
    frame_.format = current_fb_->format;
    size_t actual_len = current_fb_->len;

    // 确保备份缓存足够大，如果当前帧比预分配的还大（如 JPEG 质量突变），则重新分配
    if (actual_len > frame_.len) {
        ESP_LOGW(TAG, "Frame size %zu exceeds pre-allocated buffer %zu, re-allocating", actual_len, frame_.len);
        heap_caps_free(frame_.data);
        frame_.data = (uint8_t *)heap_caps_malloc(actual_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (frame_.data == nullptr) {
            ESP_LOGE(TAG, "Failed to re-allocate frame buffer");
            return false;
        }
        frame_.len = actual_len;
    }
    memcpy(frame_.data, current_fb_->buf, actual_len);

    // 对 RGB565 格式进行字节交换并准备预览图
    if (frame_.format == PIXFORMAT_RGB565) {
        uint16_t *src = (uint16_t *)frame_.data;
        size_t pixel_count = frame_.width * frame_.height;
        size_t data_size = pixel_count * 2;

        uint8_t *data = (uint8_t *)heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (data == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate memory for preview image copy");
            return false;
        }

        uint16_t *dst = (uint16_t *)data;
        for (size_t i = 0; i < pixel_count; i++) {
            // 在拷贝到预览缓存的同时完成字节交换
            dst[i] = __builtin_bswap16(src[i]);
            // 同时更新 frame_.data 以便 Explain 使用
            src[i] = dst[i];
        }

        // 显示预览图片
        auto display = dynamic_cast<LvglDisplay *>(Board::GetInstance().GetDisplay());
        if (display != nullptr) {
            display->SetPreviewImage(std::make_unique<LvglAllocatedImage>(data, data_size, frame_.width, frame_.height, frame_.width * 2, LV_COLOR_FORMAT_RGB565));
        } else {
            heap_caps_free(data);
        }
    } else if (frame_.format == PIXFORMAT_JPEG) {
        // JPEG 格式预览通常需要解码，此处暂不处理预览显示，仅日志记录
        ESP_LOGD(TAG, "JPEG capture success, len=%zu", actual_len);
    }

    ESP_LOGI(TAG, "Captured frame: %dx%d, len=%zu, format=%d",
             frame_.width, frame_.height, actual_len, frame_.format);

    return true;
}

bool Esp32Camera::SetHMirror(bool enabled) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        return false;
    }
    s->set_hmirror(s, enabled ? 1 : 0);
    return true;
}

bool Esp32Camera::SetVFlip(bool enabled) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        return false;
    }
    s->set_vflip(s, enabled ? 1 : 0);
    return true;
}

std::string Esp32Camera::Explain(const std::string &question) {
    if (explain_url_.empty()) {
        throw std::runtime_error("Image explain URL or token is not set");
    }

    if (current_fb_ == nullptr) {
        throw std::runtime_error("No camera frame captured");
    }

    // 创建局部的 JPEG 队列
    QueueHandle_t jpeg_queue = xQueueCreate(40, sizeof(JpegChunk));
    if (jpeg_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create JPEG queue");
        throw std::runtime_error("Failed to create JPEG queue");
    }

    // 启动编码线程
    encoder_thread_ = std::thread([this, jpeg_queue]() {
        bool ok = frame2jpg_cb(current_fb_, 80, [](void* arg, size_t index, const void* data, size_t len) -> size_t {
            auto jpeg_queue = static_cast<QueueHandle_t>(arg);
            if (data == nullptr || len == 0) {
                return 0;
            }
            
            JpegChunk chunk;
            chunk.data = (uint8_t*)heap_caps_aligned_alloc(16, len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (chunk.data == nullptr) {
                ESP_LOGE(TAG, "Failed to allocate %zu bytes for JPEG chunk", len);
                chunk.len = 0;
            } else {
                memcpy(chunk.data, data, len);
                chunk.len = len;
            }
            xQueueSend(jpeg_queue, &chunk, portMAX_DELAY);
            return len;
        }, jpeg_queue);

        // 发送结束标识
        JpegChunk terminator = {.data = nullptr, .len = 0};
        xQueueSend(jpeg_queue, &terminator, portMAX_DELAY);

        if (!ok) {
            ESP_LOGE(TAG, "JPEG encoding failed");
        }
    });

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(3);
    std::string boundary = "----ESP32_CAMERA_BOUNDARY";

    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
    if (!explain_token_.empty()) {
        http->SetHeader("Authorization", "Bearer " + explain_token_);
    }
    http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http->SetHeader("Transfer-Encoding", "chunked");
    if (!http->Open("POST", explain_url_)) {
        ESP_LOGE(TAG, "Failed to connect to explain URL");
        encoder_thread_.join();
        JpegChunk chunk;
        while (xQueueReceive(jpeg_queue, &chunk, portMAX_DELAY) == pdPASS) {
            if (chunk.data != nullptr) {
                heap_caps_free(chunk.data);
            } else {
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
    while (true) {
        JpegChunk chunk;
        if (xQueueReceive(jpeg_queue, &chunk, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Failed to receive JPEG chunk");
            break;
        }
        if (chunk.data == nullptr) {
            saw_terminator = true;
            break;
        }
        http->Write((const char *)chunk.data, chunk.len);
        total_sent += chunk.len;
        heap_caps_free(chunk.data);
    }
    encoder_thread_.join();
    vQueueDelete(jpeg_queue);

    if (!saw_terminator || total_sent == 0) {
        ESP_LOGE(TAG, "JPEG encoder failed or produced empty output");
        throw std::runtime_error("Failed to encode image to JPEG");
    }

    {
        std::string multipart_footer;
        multipart_footer += "\r\n--" + boundary + "--\r\n";
        http->Write(multipart_footer.c_str(), multipart_footer.size());
    }
    http->Write("", 0);

    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Failed to upload photo, status code: %d", http->GetStatusCode());
        throw std::runtime_error("Failed to upload photo");
    }

    std::string result = http->ReadAll();
    http->Close();

    size_t remain_stack_size = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG, "Explain image size=%dx%d, compressed size=%d, remain stack size=%d, question=%s\n%s",
             current_fb_->width, current_fb_->height, (int)total_sent, (int)remain_stack_size, question.c_str(), result.c_str());
    return result;
}
