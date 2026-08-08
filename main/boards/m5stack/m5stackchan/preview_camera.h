#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>

#include "esp_video.h"

// Adds a live viewfinder on top of EspVideo's single-shot Capture().
class PreviewCamera : public EspVideo {
public:
    explicit PreviewCamera(const esp_video_init_config_t& config);
    ~PreviewCamera() override;

    bool Capture() override;

    void StartPreview(int duration_seconds);
    void StopPreview();

    void RegisterMcpTools();

private:
    void PreviewLoop();

    std::atomic<bool> preview_running_{false};
    int64_t preview_deadline_us_ = 0;
    SemaphoreHandle_t capture_mutex_ = nullptr;
    SemaphoreHandle_t preview_exited_ = nullptr;
};
