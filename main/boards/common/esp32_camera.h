#pragma once
#include "sdkconfig.h"

#include <lvgl.h>
#include <thread>
#include <memory>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "camera.h"
#include "esp_camera.h"

struct JpegChunk
{
    uint8_t *data;
    size_t len;
};

class Esp32Camera : public Camera
{
private:
    struct FrameBuffer
    {
        uint8_t *data = nullptr;
        size_t len = 0;
        uint16_t width = 0;
        uint16_t height = 0;
        pixformat_t format = PIXFORMAT_RGB565;
    } frame_;

    bool streaming_on_ = false;
    std::string explain_url_;
    std::string explain_token_;
    std::thread encoder_thread_;
    camera_fb_t *current_fb_ = nullptr;

public:
    Esp32Camera(const camera_config_t &config);
    ~Esp32Camera();

    virtual void SetExplainUrl(const std::string &url, const std::string &token) override;
    virtual bool Capture() override;
    virtual bool SetHMirror(bool enabled) override;
    virtual bool SetVFlip(bool enabled) override;
    virtual std::string Explain(const std::string &question) override;
};
