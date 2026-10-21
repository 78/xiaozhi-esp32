#pragma once
#include "sdkconfig.h"

#include <lvgl.h>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "camera.h"
#include "esp_video_init.h"
#include "jpg/image_to_jpeg.h"

struct JpegChunk {
    uint8_t* data;
    size_t len;
};

class D1001Camera : public Camera {
private:
    struct FrameBuffer {
        uint8_t* data = nullptr;
        size_t len = 0;
        uint16_t width = 0;
        uint16_t height = 0;
        v4l2_pix_fmt_t format = 0;
    } frame_;
    v4l2_pix_fmt_t sensor_format_ = 0;
#ifdef CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
    uint16_t sensor_width_ = 0;
    uint16_t sensor_height_ = 0;
#endif  // CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
    int video_fd_ = -1;
    bool streaming_on_ = false;
    bool preview_enabled_ = true;
    std::recursive_mutex capture_mutex_;
    struct MmapBuffer {
        void* start = nullptr;
        size_t length = 0;
    };
    std::vector<MmapBuffer> mmap_buffers_;
    std::string explain_url_;
    std::string explain_token_;
    std::thread encoder_thread_;

public:
    D1001Camera(const esp_video_init_config_t& config);
    ~D1001Camera() override;

    virtual void SetExplainUrl(const std::string& url, const std::string& token);
    virtual bool Capture();
    // Suppresses the on-screen preview during background captures (face
    // service); take_photo keeps the preview.
    void SetPreviewEnabled(bool enabled) { preview_enabled_ = enabled; }

    // Silent capture exposing the raw frame for on-device inference. The
    // pointers stay valid until the next capture.
    struct RawFrame {
        const uint8_t* data;
        uint16_t width;
        uint16_t height;
        uint32_t v4l2_format;
    };
    bool CaptureRaw(RawFrame& out);
    // Silent capture that copies the frame out under the capture mutex, for
    // debug export. Returns false on failure.
    bool CaptureRawCopy(std::vector<uint8_t>& bytes, RawFrame& info);
    // Serializes camera access between the face service, take_photo and the
    // HTTP snapshot endpoint (concurrent captures corrupt frames).
    std::recursive_mutex& capture_mutex() { return capture_mutex_; }
    // Captures one frame and encodes it to JPEG into `out` (synchronous).
    virtual bool CaptureToJpeg(std::vector<uint8_t>& out);
    // 翻转控制函数
    virtual bool SetHMirror(bool enabled) override;
    virtual bool SetVFlip(bool enabled) override;
    virtual std::expected<std::string, std::string> Explain(const std::string& question) override;
};
