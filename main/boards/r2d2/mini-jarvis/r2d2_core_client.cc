#include "r2d2_core_client.h"

#include "application.h"
#include "audio/audio_codec.h"
#include "backlight.h"
#include "board.h"
#include "device_state.h"
#include "display/lvgl_display/jpg/jpeg_to_image.h"
#include "r2d2_display.h"
#include "settings.h"
#include "system_info.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_app_desc.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <wifi_manager.h>
#include <sys/time.h>

#define TAG "R2d2Core"

namespace r2d2 {

namespace {

constexpr int64_t kStatusPeriodUs = 60LL * 1000 * 1000;  // also the telemetry period
constexpr int64_t kRetryPeriodUs = 30LL * 1000 * 1000;
constexpr int kQueueDepth = 8;

}  // namespace

CoreClient::CoreClient(R2d2Display* display) : display_(display) {}

void CoreClient::Start() {
    queue_ = xQueueCreate(kQueueDepth, sizeof(Job*));
    xTaskCreate(Run, "r2d2_core", 8192, this, 2, &task_);
}

void CoreClient::Enqueue(Job::Kind kind, const std::string& arg, uint32_t seq) {
    auto* job = new Job{kind, arg, seq};
    if (xQueueSend(queue_, &job, 0) != pdTRUE) {
        ESP_LOGW(TAG, "queue full, dropping job %d", (int)kind);
        delete job;
    }
}

void CoreClient::FetchNav() { Enqueue(Job::kNav, "", 0); }
void CoreClient::FetchScreen(const std::string& id, uint32_t seq) { Enqueue(Job::kScreen, id, seq); }
void CoreClient::FetchImage(const std::string& path, uint32_t seq) { Enqueue(Job::kImage, path, seq); }
void CoreClient::Action(const std::string& json_body, uint32_t seq) { Enqueue(Job::kAction, json_body, seq); }

void CoreClient::Run(void* arg) {
    auto* self = static_cast<CoreClient*>(arg);
    int64_t next_status_us = 0;
    while (true) {
        Job* job = nullptr;
        int64_t now = esp_timer_get_time();
        TickType_t wait = next_status_us > now ? pdMS_TO_TICKS((next_status_us - now) / 1000) : 0;
        if (xQueueReceive(self->queue_, &job, wait) == pdTRUE) {
            self->Handle(*job);
            delete job;
            continue;
        }
        // Idle only: no HTTP alongside a conversation's audio, and no token
        // before the device has been through activation.
        bool idle = Application::GetInstance().GetDeviceState() == kDeviceStateIdle;
        bool ok = idle && self->PollStatus();
        if (ok && !self->nav_loaded_) self->Handle(Job{Job::kNav, "", 0});
        next_status_us = esp_timer_get_time() + (ok ? kStatusPeriodUs : kRetryPeriodUs);
    }
}

std::string CoreClient::BaseUrl() {
    Settings wifi("wifi", false);
    std::string url = wifi.GetString("ota_url");
    if (url.empty()) url = CONFIG_OTA_URL;
    auto pos = url.find("/satellite/");
    return pos == std::string::npos ? "" : url.substr(0, pos);
}

bool CoreClient::Request(const char* method, const std::string& path, const std::string& content,
                         std::string* body, int* code) {
    *code = -1;
    std::string base = BaseUrl();
    Settings ws("websocket", false);
    std::string token = ws.GetString("token");
    if (base.empty() || token.empty()) return false;

    auto& board = Board::GetInstance();
    auto http = board.GetNetwork()->CreateHttp(0);
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", board.GetUuid());
    http->SetHeader("Authorization", "Bearer " + token);
    if (!content.empty()) {
        http->SetHeader("Content-Type", "application/json");
        http->SetContent(std::string(content));
    }
    if (auto opened = http->Open(method, base + path); !opened) {
        ESP_LOGW(TAG, "%s %s failed: %s", method, path.c_str(), opened.error().ToString().c_str());
        online_ = false;
        return false;
    }
    auto status = http->GetStatusCode();
    *code = status ? *status : -1;
    online_ = status.has_value();
    if (*code != 200) {
        ESP_LOGW(TAG, "%s %s: HTTP %d", method, path.c_str(), *code);
        http->Close();
        return false;
    }
    *body = http->ReadAll();
    http->Close();
    return true;
}

void CoreClient::Handle(const Job& job) {
    std::string body;
    int code;
    switch (job.kind) {
        case Job::kNav: {
            if (!Request("GET", "/satellite/nav", "", &body, &code)) return;
            cJSON* root = cJSON_Parse(body.c_str());
            if (root == nullptr) return;
            nav_loaded_ = display_->OnNav(root);  // takes ownership
            break;
        }
        case Job::kScreen: {
            cJSON* root = nullptr;
            if (Request("GET", "/satellite/screen/" + job.arg, "", &body, &code)) root = cJSON_Parse(body.c_str());
            display_->OnScreen(job.seq, job.arg, root);  // nullptr: failed
            break;
        }
        case Job::kAction: {
            bool ok = Request("POST", "/satellite/action", job.arg, &body, &code);
            if (ok) {
                cJSON* root = cJSON_Parse(body.c_str());
                ok = root && cJSON_IsTrue(cJSON_GetObjectItem(root, "ok"));
                cJSON_Delete(root);
            }
            display_->OnAction(job.seq, ok);
            break;
        }
        case Job::kImage: {
            if (!Request("GET", job.arg, "", &body, &code)) {
                display_->OnImage(job.seq, nullptr, 0, 0, 0, 0);
                break;
            }
            uint8_t* pixels = nullptr;
            size_t size = 0, w = 0, h = 0, stride = 0;
            if (jpeg_to_image((const uint8_t*)body.data(), body.size(), &pixels, &size, &w, &h, &stride) != ESP_OK) {
                ESP_LOGW(TAG, "JPEG decode failed (%u bytes)", (unsigned)body.size());
                pixels = nullptr;
            }
            display_->OnImage(job.seq, pixels, size, w, h, stride);  // takes ownership of pixels
            break;
        }
    }
}

// The device's own numbers, as query parameters of the status poll: the Core
// turns them into r2d2_satellite_* Prometheus gauges (r2d2 repo,
// agent_core/channels/satellite/metrics.py).
std::string CoreClient::Telemetry() {
    auto& board = Board::GetInstance();
    auto& wifi = WifiManager::GetInstance();
    int level = 0;
    bool charging = false, discharging = false;
    bool has_battery = board.GetBatteryLevel(level, charging, discharging);
    auto codec = board.GetAudioCodec();
    auto backlight = board.GetBacklight();
    char query[320];
    int n = snprintf(query, sizeof(query), "?rssi=%d&uptime=%lld&heap=%u&psram=%u&volume=%d&mic_gain=%.0f&fw=%s",
                     wifi.IsConnected() ? wifi.GetRssi() : 0, esp_timer_get_time() / 1000000,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), codec->output_volume(),
                     codec->input_gain(), esp_app_get_description()->version);
    if (has_battery && n > 0 && n < (int)sizeof(query)) {
        n += snprintf(query + n, sizeof(query) - n, "&battery=%d&charging=%d", level, charging ? 1 : 0);
    }
    if (backlight && n > 0 && n < (int)sizeof(query)) {
        snprintf(query + n, sizeof(query) - n, "&brightness=%d", backlight->brightness());
    }
    return query;
}

bool CoreClient::PollStatus() {
    std::string body;
    int code;
    if (!Request("GET", "/satellite/status" + Telemetry(), "", &body, &code)) return false;
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) return false;
    // Local time kept as UTC, the way the firmware applies the OTA server_time.
    cJSON* time_ms = cJSON_GetObjectItem(root, "time_ms");
    cJSON* tz = cJSON_GetObjectItem(root, "tz_offset_min");
    if (cJSON_IsNumber(time_ms)) {
        double ms = time_ms->valuedouble + (cJSON_IsNumber(tz) ? tz->valueint * 60.0 * 1000 : 0);
        struct timeval tv = {(time_t)(ms / 1000), (suseconds_t)(((long long)ms % 1000) * 1000)};
        settimeofday(&tv, nullptr);
    }
    Weather weather;
    cJSON* w = cJSON_GetObjectItem(root, "weather");
    if (cJSON_IsObject(w)) {
        cJSON* temp = cJSON_GetObjectItem(w, "temp_c");
        cJSON* condition = cJSON_GetObjectItem(w, "condition");
        cJSON* icon = cJSON_GetObjectItem(w, "icon");
        cJSON* rain = cJSON_GetObjectItem(w, "rain_pct");
        weather.up = cJSON_IsTrue(cJSON_GetObjectItem(w, "up")) && cJSON_IsNumber(temp);
        if (cJSON_IsNumber(temp)) weather.temp_c = temp->valuedouble;
        if (cJSON_IsString(condition)) weather.condition = condition->valuestring;
        if (cJSON_IsString(icon)) weather.icon = icon->valuestring;
        weather.rain_pct = cJSON_IsNumber(rain) ? rain->valueint : -1;
    }
    cJSON_Delete(root);
    display_->SetWeather(weather);
    display_->UpdateStatusBar(true);  // the clock may have just been set
    ESP_LOGI(TAG, "status ok: %.1f°C %s", weather.temp_c, weather.condition.c_str());
    return true;
}

}  // namespace r2d2
