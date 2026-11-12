#include "sdkconfig.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <img_converters.h>
#include <cstdio>
#include <cstring>

#include "board.h"
#include "display.h"
#include "esp32_camera.h"
#include "esp_timer.h"
#include "jpg/image_to_jpeg.h"
#include "lvgl_display.h"
#include "mcp_server.h"
#include "system_info.h"

#define TAG "Esp32Camera"

#if CONFIG_XIAOZHI_CAMERA_MIRROR_CONFIGURED
#if CONFIG_XIAOZHI_CAMERA_HMIRROR
static constexpr bool kConfiguredHMirror = true;
#else
static constexpr bool kConfiguredHMirror = false;
#endif
#if CONFIG_XIAOZHI_CAMERA_VFLIP
static constexpr bool kConfiguredVFlip = true;
#else
static constexpr bool kConfiguredVFlip = false;
#endif
#endif

Esp32Camera::Esp32Camera(const camera_config_t& config) {
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed with error 0x%x", err);
        return;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        if (s->id.PID == GC0308_PID) {
            s->set_hmirror(s, 0);  // Control camera mirror: 1 for mirror, 0 for normal
        }
#if CONFIG_XIAOZHI_CAMERA_MIRROR_CONFIGURED
        s->set_hmirror(s, kConfiguredHMirror ? 1 : 0);
        s->set_vflip(s, kConfiguredVFlip ? 1 : 0);
#endif
        ESP_LOGI(TAG, "Camera initialized: format=%d", config.pixel_format);
    }

    streaming_on_ = true;
}

Esp32Camera::~Esp32Camera() {
    if (streaming_on_) {
        if (current_fb_) {
            esp_camera_fb_return(current_fb_);
            current_fb_ = nullptr;
        }
        if (encode_buf_) {
            heap_caps_free(encode_buf_);
            encode_buf_ = nullptr;
            encode_buf_size_ = 0;
        }
        esp_camera_deinit();
        streaming_on_ = false;
    }
}

void Esp32Camera::SetExplainUrl(const std::string& url, const std::string& token) {
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

    // Get the latest frame, discard old frames for real-time performance
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

    // Prepare encode buffer for RGB565 format (with optional byte swapping)
    if (current_fb_->format == PIXFORMAT_RGB565) {
        size_t pixel_count = current_fb_->width * current_fb_->height;
        size_t data_size = pixel_count * 2;

        // Allocate or reallocate encode buffer if needed
        if (encode_buf_size_ < data_size) {
            if (encode_buf_) {
                heap_caps_free(encode_buf_);
            }
            encode_buf_ =
                (uint8_t*)heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (encode_buf_ == nullptr) {
                ESP_LOGE(TAG, "Failed to allocate memory for encode buffer");
                encode_buf_size_ = 0;
                return false;
            }
            encode_buf_size_ = data_size;
        }

        // Copy data to encode buffer with optional byte swapping
        uint16_t* src = (uint16_t*)current_fb_->buf;
        uint16_t* dst = (uint16_t*)encode_buf_;
        if (swap_bytes_enabled_) {
            for (size_t i = 0; i < pixel_count; i++) {
                dst[i] = __builtin_bswap16(src[i]);
            }
        } else {
            memcpy(encode_buf_, current_fb_->buf, data_size);
        }

        // Allocate separate buffer for preview display
        uint8_t* preview_data =
            (uint8_t*)heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (preview_data != nullptr) {
            memcpy(preview_data, encode_buf_, data_size);
            auto display = Board::GetInstance().GetDisplay();
            if (display != nullptr) {
                display->SetPreviewImage(std::make_unique<LvglAllocatedImage>(
                    preview_data, data_size, current_fb_->width, current_fb_->height,
                    current_fb_->width * 2, LV_COLOR_FORMAT_RGB565));
            } else {
                heap_caps_free(preview_data);
            }
        }
    } else if (current_fb_->format == PIXFORMAT_JPEG) {
        // JPEG format preview usually requires decoding, skip preview display for now, just log
        ESP_LOGW(TAG, "JPEG capture success, len=%zu, but not supported for preview",
                 current_fb_->len);
    }

    ESP_LOGI(TAG, "Captured frame: %dx%d, len=%zu, format=%d", current_fb_->width,
             current_fb_->height, current_fb_->len, current_fb_->format);

    return true;
}

bool Esp32Camera::SetHMirror(bool enabled) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) {
        return false;
    }
    s->set_hmirror(s, enabled ? 1 : 0);
    return true;
}

bool Esp32Camera::SetVFlip(bool enabled) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) {
        return false;
    }
    s->set_vflip(s, enabled ? 1 : 0);
    return true;
}

bool Esp32Camera::SetSwapBytes(bool enabled) {
    swap_bytes_enabled_ = enabled;
    return true;
}

std::expected<std::string, std::string> Esp32Camera::Explain(const std::string& question) {
    if (explain_url_.empty()) {
        return std::unexpected("Image explain URL or token is not set");
    }

    if (current_fb_ == nullptr) {
        return std::unexpected("No camera frame captured");
    }

    // Encode synchronously and take ownership of the whole JPEG buffer. The
    // multipart body is streamed with an explicit Content-Length instead of
    // being assembled into one std::string: the server (ALB) rejects chunked
    // uploads, and a std::string that fails to grow aborts the firmware because
    // C++ exceptions are disabled.
    const uint16_t w = current_fb_->width;
    const uint16_t h = current_fb_->height;
    v4l2_pix_fmt_t enc_fmt;
    switch (current_fb_->format) {
        case PIXFORMAT_RGB565:
            enc_fmt = V4L2_PIX_FMT_RGB565;
            break;
        case PIXFORMAT_YUV422:
            enc_fmt = V4L2_PIX_FMT_YUYV;  // YUV422 is actually YUYV format
            break;
        case PIXFORMAT_YUV420:
            enc_fmt = V4L2_PIX_FMT_YUV420;
            break;
        case PIXFORMAT_GRAYSCALE:
            enc_fmt = V4L2_PIX_FMT_GREY;
            break;
        case PIXFORMAT_JPEG:
            enc_fmt = V4L2_PIX_FMT_JPEG;
            break;
        case PIXFORMAT_RGB888:
            enc_fmt = V4L2_PIX_FMT_RGB24;
            break;
        default:
            ESP_LOGE(TAG, "Unsupported pixel format: %d", current_fb_->format);
            return std::unexpected("Unsupported pixel format");
    }

    // Use encode buffer for RGB565, otherwise use original frame buffer
    uint8_t* jpeg_src_buf = current_fb_->buf;
    size_t jpeg_src_len = current_fb_->len;
    if (current_fb_->format == PIXFORMAT_RGB565 && encode_buf_ != nullptr) {
        jpeg_src_buf = encode_buf_;
        jpeg_src_len = encode_buf_size_;
    }

    int64_t start_time = esp_timer_get_time();
    uint8_t* jpeg_data = nullptr;
    size_t jpeg_size = 0;
    bool ok = image_to_jpeg(jpeg_src_buf, jpeg_src_len, w, h, enc_fmt, 80, &jpeg_data, &jpeg_size);
    ESP_LOGI(TAG, "JPEG encoding time: %ld ms", int((esp_timer_get_time() - start_time) / 1000));
    if (!ok || jpeg_data == nullptr || jpeg_size == 0) {
        free(jpeg_data);
        ESP_LOGE(TAG, "JPEG encoder failed or produced empty output");
        return std::unexpected("Failed to encode image to JPEG");
    }

    const std::string boundary = "----ESP32_CAMERA_BOUNDARY";
    std::string head;
    head += "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"question\"\r\n\r\n";
    head += question + "\r\n";
    head += "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"file\"; filename=\"camera.jpg\"\r\n";
    head += "Content-Type: image/jpeg\r\n\r\n";
    const std::string tail = "\r\n--" + boundary + "--\r\n";

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(3);
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
    if (!explain_token_.empty()) {
        http->SetHeader("Authorization", "Bearer " + explain_token_);
    }
    http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    // An explicit Content-Length makes Http::Write() send raw bytes instead of
    // chunk framing, so only the JPEG is buffered.
    http->SetHeader("Content-Length",
                    std::to_string(head.size() + jpeg_size + tail.size()));
    if (auto opened = http->Open("POST", explain_url_); !opened) {
        free(jpeg_data);
        ESP_LOGE(TAG, "Failed to upload photo: %s", opened.error().ToString().c_str());
        http->Close();
        return std::unexpected("Failed to upload photo");
    }

    auto write_all = [&http](const void* data, size_t size) -> bool {
        const char* ptr = static_cast<const char*>(data);
        while (size > 0) {
            auto written = http->Write(ptr, size);
            if (!written || *written <= 0 || static_cast<size_t>(*written) > size) {
                return false;
            }
            ptr += *written;
            size -= static_cast<size_t>(*written);
        }
        return true;
    };

    bool uploaded = write_all(head.data(), head.size()) &&
                    write_all(jpeg_data, jpeg_size) &&
                    write_all(tail.data(), tail.size());
    free(jpeg_data);
    if (!uploaded) {
        ESP_LOGE(TAG, "Failed to upload photo body");
        http->Close();
        return std::unexpected("Failed to upload photo");
    }
    ESP_LOGI(TAG, "Explain upload done: jpeg=%u bytes, content-length=%u",
             (unsigned)jpeg_size, (unsigned)(head.size() + jpeg_size + tail.size()));

    auto status_code = http->GetStatusCode();
    if (!status_code) {
        ESP_LOGE(TAG, "Failed to read HTTP status: %s", status_code.error().ToString().c_str());
        http->Close();
        return std::unexpected("Failed to upload photo");
    }
    if (*status_code != 200) {
        ESP_LOGE(TAG, "Failed to upload photo, status code: %d", *status_code);
        http->Close();
        return std::unexpected("Failed to upload photo");
    }

    std::string result = http->ReadAll();
    http->Close();

    size_t remain_stack_size = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG,
             "Explain image size=%dx%d, compressed size=%d, remain stack size=%d, question=%s\n%s",
             current_fb_->width, current_fb_->height, (int)jpeg_size, (int)remain_stack_size,
             question.c_str(), result.c_str());
    return result;
}
