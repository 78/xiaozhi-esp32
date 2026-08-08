#include "preview_camera.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>

#include <algorithm>

#include "board.h"
#include "lvgl_display.h"
#include "mcp_server.h"

#define TAG "PreviewCamera"

namespace {
constexpr int kPreviewIntervalMs = 100;
constexpr int kPreviewTaskStackSize = 8192;
constexpr UBaseType_t kPreviewTaskPriority = 1;
constexpr int kPreviewDefaultSeconds = 30;
constexpr int kPreviewMinSeconds = 5;
constexpr int kPreviewMaxSeconds = 120;
constexpr int kPreviewExitTimeoutMs = 3000;
}  // namespace

PreviewCamera::PreviewCamera(const esp_video_init_config_t& config) : EspVideo(config) {
    capture_mutex_ = xSemaphoreCreateMutex();
    preview_exited_ = xSemaphoreCreateBinary();
}

PreviewCamera::~PreviewCamera() {
    StopPreview();
    vSemaphoreDelete(capture_mutex_);
    vSemaphoreDelete(preview_exited_);
}

bool PreviewCamera::Capture() {
    // Taking a photo ends the viewfinder; both share one V4L2 device.
    StopPreview();

    xSemaphoreTake(capture_mutex_, portMAX_DELAY);
    bool ok = EspVideo::Capture();
    xSemaphoreGive(capture_mutex_);
    return ok;
}

void PreviewCamera::StartPreview(int duration_seconds) {
    duration_seconds = std::clamp(duration_seconds, kPreviewMinSeconds, kPreviewMaxSeconds);
    preview_deadline_us_ = esp_timer_get_time() + static_cast<int64_t>(duration_seconds) * 1000000;

    if (preview_running_.exchange(true)) {
        ESP_LOGI(TAG, "Viewfinder extended to %d s", duration_seconds);
        return;
    }

    xSemaphoreTake(preview_exited_, 0);
    auto entry = [](void* arg) {
        static_cast<PreviewCamera*>(arg)->PreviewLoop();
        vTaskDelete(NULL);
    };
    if (xTaskCreate(entry, "cam_preview", kPreviewTaskStackSize, this, kPreviewTaskPriority,
                    nullptr) != pdPASS) {
        preview_running_ = false;
        ESP_LOGE(TAG, "Failed to create viewfinder task");
    }
}

void PreviewCamera::StopPreview() {
    if (!preview_running_.exchange(false)) {
        return;
    }
    if (xSemaphoreTake(preview_exited_, pdMS_TO_TICKS(kPreviewExitTimeoutMs)) != pdTRUE) {
        ESP_LOGW(TAG, "Viewfinder task did not exit in time");
    }
}

void PreviewCamera::PreviewLoop() {
    ESP_LOGI(TAG, "Viewfinder started");

    while (preview_running_.load() && esp_timer_get_time() < preview_deadline_us_) {
        xSemaphoreTake(capture_mutex_, portMAX_DELAY);
        bool ok = EspVideo::Capture();
        xSemaphoreGive(capture_mutex_);

        if (!ok) {
            ESP_LOGW(TAG, "Frame capture failed, stopping viewfinder");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(kPreviewIntervalMs));
    }
    preview_running_ = false;

    // Drop the last frame now instead of waiting out the display's preview timer.
    auto display = dynamic_cast<LvglDisplay*>(Board::GetInstance().GetDisplay());
    if (display != nullptr) {
        display->SetPreviewImage(nullptr);
    }

    ESP_LOGI(TAG, "Viewfinder stopped");
    xSemaphoreGive(preview_exited_);
}

void PreviewCamera::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddTool(
        "self.camera.start_preview",
        "Show a live camera viewfinder on the screen so the user can frame a shot before you take "
        "a photo. Use this when the user says they are getting ready to take a photo, or asks to "
        "see what the camera sees. The viewfinder turns off by itself once a photo is taken or "
        "the duration elapses.\n"
        "Args:\n"
        "  `duration`: How many seconds to keep the viewfinder on.",
        PropertyList({Property("duration", kPropertyTypeInteger, kPreviewDefaultSeconds,
                               kPreviewMinSeconds, kPreviewMaxSeconds)}),
        [this](const PropertyList& properties) -> ReturnValue {
            StartPreview(properties["duration"].value<int>());
            return true;
        });

    mcp_server.AddTool("self.camera.stop_preview",
                       "Turn off the camera viewfinder and go back to the normal screen.",
                       PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                           StopPreview();
                           return true;
                       });
}
