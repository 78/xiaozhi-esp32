#include "hilily_home_client.h"
#include "hilily_display.h"
#include "hilily_music_player.h"
#include "application.h"
#include "config.h"
#include "settings.h"

#include <esp_mac.h>
#include <esp_timer.h>

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>

#include <cmath>
#include <cstring>
#include <string>
#include <ctime>
#include <cstdio>
#include <cstdint>

namespace {
constexpr const char* TAG = "HiLilyHome";
constexpr size_t MAX_JSON_BYTES = 4096;

struct RequestContext {
    std::string payload;
    bool overflow = false;
};

esp_err_t OnHttpEvent(esp_http_client_event_t* event) {
    auto* context = static_cast<RequestContext*>(event->user_data);
    if (!context || event->event_id != HTTP_EVENT_ON_DATA || !event->data || event->data_len <= 0)
        return ESP_OK;
    if (context->payload.size() + (size_t)event->data_len > MAX_JSON_BYTES) {
        context->overflow = true;
        return ESP_FAIL;
    }
    context->payload.append(static_cast<const char*>(event->data), event->data_len);
    return ESP_OK;
}

const char* JsonString(cJSON* parent, const char* name) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(parent, name);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : nullptr;
}

bool FetchHome(HiLilyDisplay* display, const char* url) {
    RequestContext response;
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 9000;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = OnHttpEvent;
    config.user_data = &response;
    config.buffer_size = 1024;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;

    const esp_err_t error = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (error != ESP_OK || status != 200 || response.overflow) {
        ESP_LOGW(TAG, "Home API failed: %s HTTP %d", esp_err_to_name(error), status);
        return false;
    }
    cJSON* root = cJSON_ParseWithLength(response.payload.data(), response.payload.size());
    if (!root) return false;
    cJSON* weather = cJSON_GetObjectItemCaseSensitive(root, "weather");
    cJSON* lunar = cJSON_GetObjectItemCaseSensitive(root, "lunar");
    cJSON* temp = cJSON_GetObjectItemCaseSensitive(weather, "temperature_c");
    const char* schema = JsonString(root, "schema");
    const char* location = JsonString(root, "location");
    const char* condition = JsonString(weather, "description");
    const char* lunar_date = JsonString(lunar, "display");
    bool valid = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "ok")) &&
                 schema && strcmp(schema, "hilily.display.home.v1") == 0 &&
                 location && condition && lunar_date &&
                 cJSON_IsNumber(temp) && std::isfinite(temp->valuedouble) &&
                 temp->valuedouble >= -60 && temp->valuedouble <= 65 &&
                 strlen(location) <= 128 && strlen(condition) <= 128 &&
                 strlen(lunar_date) <= 48;
    if (valid) {
        display->SetHomeData(location, condition, lunar_date, temp->valuedouble);
    }
    cJSON_Delete(root);
    if (!valid) ESP_LOGW(TAG, "Home API returned invalid/incompatible JSON");
    return valid;
}


// Simple public read-only schedule. The server never returns family/member data.
// Do not persist server credentials on this device or allocate another TLS task.
struct MorningSchedule {
    bool valid = false;
    bool enabled = false;
    uint16_t minute_of_day = 0;
    uint8_t days = 0;
    int64_t server_unix = 0;
    int64_t fetched_us = 0;
};

std::string DeviceId() {
    uint8_t mac[6] = {};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) return {};
    char id[16] = {};
    snprintf(id, sizeof(id), "s3_%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return id;
}

bool FetchMorningSchedule(const std::string& device_id, MorningSchedule& out) {
    // Device ID contains only fixed prefix + 12 hex bytes; safe in query.
    std::string url = std::string(HILILY_WORKER_BASE_URL) +
        "/api/device/morning/config?device_id=" + device_id;
    RequestContext response;
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 8000;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = OnHttpEvent;
    config.user_data = &response;
    config.buffer_size = 1024;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;
    const esp_err_t error = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (error != ESP_OK || status != 200 || response.overflow) {
        ESP_LOGW(TAG, "Schedule API unavailable: HTTP %d", status);
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(response.payload.data(), response.payload.size());
    if (!root) return false;
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "config");
    cJSON* days = cJSON_GetObjectItemCaseSensitive(data, "active_days");
    cJSON* enabled = cJSON_GetObjectItemCaseSensitive(data, "enabled");
    cJSON* now = cJSON_GetObjectItemCaseSensitive(root, "server_unix");
    const char* schema = JsonString(root, "schema");
    const char* id = JsonString(root, "device_id");
    const char* time = JsonString(data, "wake_time");
    const char* zone = JsonString(data, "timezone");
    int hh = -1, mm = -1;
    const bool time_ok = time && strlen(time) == 5 && time[2] == ':' &&
        sscanf(time, "%2d:%2d", &hh, &mm) == 2 &&
        hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59;
    bool valid =
        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "ok")) &&
        schema && strcmp(schema, "hilily.morning.schedule.v1") == 0 &&
        id && device_id == id && time_ok &&
        zone && strcmp(zone, "Asia/Ho_Chi_Minh") == 0 &&
        cJSON_IsBool(enabled) && cJSON_IsArray(days) &&
        cJSON_GetArraySize(days) <= 7 && cJSON_IsNumber(now) &&
        now->valuedouble > 1700000000.0 && now->valuedouble < 4102444800.0;
    uint8_t mask = 0;
    if (valid) {
        cJSON* d = nullptr;
        cJSON_ArrayForEach(d, days) {
            if (!cJSON_IsNumber(d) || d->valuedouble != d->valueint ||
                d->valueint < 0 || d->valueint > 6) {
                valid = false;
                break;
            }
            mask |= 1U << d->valueint;
        }
    }
    if (valid) {
        out.enabled = cJSON_IsTrue(enabled);
        out.minute_of_day = static_cast<uint16_t>(hh * 60 + mm);
        out.days = mask;
        out.server_unix = static_cast<int64_t>(now->valuedouble);
        out.fetched_us = esp_timer_get_time();
        out.valid = true;
    }
    cJSON_Delete(root);
    return valid;
}

bool VietnamClock(const MorningSchedule& schedule, std::tm& clock,
                  std::string& day) {
    if (!schedule.valid) return false;
    const int64_t elapsed = (esp_timer_get_time() - schedule.fetched_us) / 1000000;
    if (elapsed < 0 || elapsed > 300) return false; // Stale schedule: fail closed.
    // Worker returns Unix UTC time. Vietnam has fixed UTC+7 (no DST).
    const time_t vn_time = static_cast<time_t>(schedule.server_unix + elapsed + 7*3600);
    if (!gmtime_r(&vn_time, &clock)) return false;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
             clock.tm_year + 1900, clock.tm_mon + 1, clock.tm_mday);
    day = buffer;
    return true;
}

struct HomeTaskArgs { HiLilyDisplay* display; std::string url; };

void HomeTask(void* parameter) {
    auto* args = static_cast<HomeTaskArgs*>(parameter);
    vTaskDelay(pdMS_TO_TICKS(20000));  // Allow Wi-Fi / TLS to initialize.
    const std::string device_id = DeviceId();
    if (device_id.empty()) {
        ESP_LOGE(TAG, "Cannot determine hardware ID; schedule disabled");
    } else {
        ESP_LOGI(TAG, "Dashboard morning schedule device ID: %s", device_id.c_str());
        Application::GetInstance().Schedule([display=args->display, device_id]() {
            if (Application::GetInstance().GetDeviceState() == kDeviceStateIdle)
                display->SetChatMessage("system", ("ID lich: " + device_id).c_str());
        });
    }
    // This NVS key survives ordinary app-only reflashing and prevents replays
    // on a reboot or network reconnection within the same Vietnam calendar day.
    Settings daily("morning_run", true);
    std::string last_run = daily.GetString("last_day", "");
    MorningSchedule schedule;
    int64_t next_home = 0, next_schedule = 0;
    while (true) {
        const int64_t now = esp_timer_get_time();
        if (now >= next_home) {
            const bool ok = FetchHome(args->display, args->url.c_str());
            next_home = esp_timer_get_time() + (ok ? 20LL*60 : 60LL)*1000000;
        }
        if (!device_id.empty() && esp_timer_get_time() >= next_schedule) {
            MorningSchedule updated;
            const bool ok = FetchMorningSchedule(device_id, updated);
            schedule = ok ? updated : MorningSchedule{}; // No stale execution.
            next_schedule = esp_timer_get_time() + (ok ? 120LL : 60LL)*1000000;
        }
        std::tm vietnam = {};
        std::string day;
        if (VietnamClock(schedule, vietnam, day) && schedule.enabled &&
            (schedule.days & (1U << vietnam.tm_wday)) && last_run != day) {
            const int minute = vietnam.tm_hour * 60 + vietnam.tm_min;
            // Wait for idle up to 10 minutes, never interrupt active voice/music.
            if (minute >= schedule.minute_of_day &&
                minute < schedule.minute_of_day + 10 &&
                Application::GetInstance().CanStartScheduledBriefing() &&
                !HiLilyMusicPlayer::GetInstance().IsBusy()) {
                // Mark before scheduling. If app becomes busy in the meantime,
                // skip today's run rather than speak over the user.
                daily.SetString("last_day", day);
                last_run = day;
                ESP_LOGI(TAG, "Morning schedule due (%s, UTC+7)", day.c_str());
                Application::GetInstance().StartScheduledMorningBriefing(day);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
}

void StartHiLilyHomeClient(HiLilyDisplay* display, const char* url) {
    if (!display || !url || strncmp(url, "https://", 8) != 0) {
        ESP_LOGE(TAG, "Worker Home URL must be HTTPS");
        return;
    }
    auto* args = new HomeTaskArgs{display, url};
    const BaseType_t created = xTaskCreate(HomeTask, "lily_home", 6144, args, 2, nullptr);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Unable to start Home API task");
        delete args;
    }
}
