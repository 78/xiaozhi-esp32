#include "r2d2_status.h"

#include "application.h"
#include "board.h"
#include "device_state.h"
#include "r2d2_display.h"
#include "settings.h"
#include "system_info.h"

#include <cJSON.h>
#include <esp_log.h>
#include <sys/time.h>

#include <string>

#define TAG "R2d2Status"

namespace r2d2 {

namespace {

constexpr int kPollPeriodS = 300;
constexpr int kRetryPeriodS = 30;

std::string StatusUrl() {
    Settings wifi("wifi", false);
    std::string url = wifi.GetString("ota_url");
    if (url.empty()) url = CONFIG_OTA_URL;
    auto pos = url.rfind("ota/");
    if (pos == std::string::npos) return "";
    return url.substr(0, pos) + "status";
}

}  // namespace

StatusPoller::StatusPoller(R2d2Display* display) : display_(display) {}

void StatusPoller::Start() {
    xTaskCreate(Run, "r2d2_status", 6144, this, 2, &task_);
}

void StatusPoller::Run(void* arg) {
    auto* self = static_cast<StatusPoller*>(arg);
    while (true) {
        auto state = Application::GetInstance().GetDeviceState();
        // Only while idle: no HTTP alongside a conversation's audio, and no
        // token before the device has been through activation.
        bool ok = state == kDeviceStateIdle && self->PollOnce();
        vTaskDelay(pdMS_TO_TICKS((ok ? kPollPeriodS : kRetryPeriodS) * 1000));
    }
}

bool StatusPoller::PollOnce() {
    std::string url = StatusUrl();
    Settings ws("websocket", false);
    std::string token = ws.GetString("token");
    if (url.empty() || token.empty()) return false;

    auto& board = Board::GetInstance();
    auto http = board.GetNetwork()->CreateHttp(0);
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", board.GetUuid());
    http->SetHeader("Authorization", ("Bearer " + token).c_str());
    if (auto opened = http->Open("GET", url); !opened) {
        ESP_LOGW(TAG, "GET %s failed: %s", url.c_str(), opened.error().ToString().c_str());
        return false;
    }
    auto status = http->GetStatusCode();
    if (!status || *status != 200) {
        ESP_LOGW(TAG, "GET %s: HTTP %d", url.c_str(), status ? *status : -1);
        http->Close();
        return false;
    }
    std::string body = http->ReadAll();
    http->Close();

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        ESP_LOGW(TAG, "bad JSON from %s", url.c_str());
        return false;
    }
    // Local time kept as UTC, the same way the firmware applies the OTA server_time.
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
        cJSON* up = cJSON_GetObjectItem(w, "up");
        cJSON* temp = cJSON_GetObjectItem(w, "temp_c");
        cJSON* condition = cJSON_GetObjectItem(w, "condition");
        cJSON* icon = cJSON_GetObjectItem(w, "icon");
        cJSON* rain = cJSON_GetObjectItem(w, "rain_pct");
        weather.up = cJSON_IsTrue(up) && cJSON_IsNumber(temp);
        if (cJSON_IsNumber(temp)) weather.temp_c = temp->valuedouble;
        if (cJSON_IsString(condition)) weather.condition = condition->valuestring;
        if (cJSON_IsString(icon)) weather.icon = icon->valuestring;
        weather.rain_pct = cJSON_IsNumber(rain) ? rain->valueint : -1;
    }
    cJSON_Delete(root);
    display_->SetWeather(weather);
    display_->UpdateStatusBar(true);  // clock may have just been set
    ESP_LOGI(TAG, "status ok: %.1f°C %s", weather.temp_c, weather.condition.c_str());
    return true;
}

}  // namespace r2d2
