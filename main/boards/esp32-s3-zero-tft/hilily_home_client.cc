#include "hilily_home_client.h"
#include "hilily_display.h"
#include "hilily_music_player.h"
#include "application.h"
#include "config.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>

#include <cmath>
#include <cstring>
#include <string>
#include <deque>
#include <mutex>
#include <ctime>
#include <cstdio>

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


// Reuse the existing home-HTTP task instead of allocating another TLS task
// and stack on the ESP32-S3 N4R2 (4 MB flash, 2 MB PSRAM).
struct MorningConfig {
    bool valid = false;
    bool enabled = false;
    int minute_of_day = 0;
    uint8_t days_mask = 0;
    int64_t server_unix = 0;
    int64_t fetched_us = 0;
};
struct MorningEvent {
    std::string action, day, reason;
    int completed = -1;
};
std::mutex event_mutex;
std::deque<MorningEvent> event_queue;

bool DeviceHttp(const char* endpoint, bool post, const std::string& body,
                int& status, std::string* response_text = nullptr) {
#if CONFIG_BOARD_TYPE_ESP32_S3_ZERO_TFT
    const char* id = CONFIG_HILILY_MORNING_DEVICE_ID;
    const char* token = CONFIG_HILILY_MORNING_DEVICE_TOKEN;
    if (!id[0] || strlen(token) < 32) return false;
    RequestContext response;
    esp_http_client_config_t cfg = {};
    cfg.url = endpoint;
    cfg.method = post ? HTTP_METHOD_POST : HTTP_METHOD_GET;
    cfg.timeout_ms = 8000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.event_handler = OnHttpEvent;
    cfg.user_data = &response;
    cfg.buffer_size = 1024;
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    const std::string authorization = std::string("Bearer ") + token;
    esp_http_client_set_header(client, "Authorization", authorization.c_str());
    esp_http_client_set_header(client, "X-Xiaozhi-Device-ID", id);
    if (post) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body.c_str(), body.size());
    }
    const esp_err_t err = esp_http_client_perform(client);
    status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (response_text) *response_text = std::move(response.payload);
    if (err != ESP_OK || response.overflow) {
        ESP_LOGW(TAG, "Morning device API transport error: %s", esp_err_to_name(err));
        return false;
    }
    return true;
#else
    (void)endpoint; (void)post; (void)body; (void)status; (void)response_text;
    return false;
#endif
}

bool FetchMorning(MorningConfig& cfg) {
    int status = 0;
    std::string payload;
    if (!DeviceHttp(HILILY_WORKER_BASE_URL "/api/device/morning/config",
                    false, "", status, &payload) || status != 200) {
        ESP_LOGW(TAG, "Morning config unavailable: HTTP %d", status);
        return false;
    }
    cJSON* root = cJSON_ParseWithLength(payload.data(), payload.size());
    if (!root) return false;
    const cJSON* config = cJSON_GetObjectItemCaseSensitive(root, "config");
    const cJSON* sec = cJSON_GetObjectItemCaseSensitive(root, "server_unix");
    const cJSON* days = cJSON_GetObjectItemCaseSensitive(config, "active_days");
    const cJSON* enabled = cJSON_GetObjectItemCaseSensitive(config, "enabled");
    const char* time = JsonString(const_cast<cJSON*>(config), "wake_time");
    const char* zone = JsonString(const_cast<cJSON*>(config), "timezone");
    const char* device = JsonString(root, "device_id");
    int hh=-1, mm=-1;
    const bool has_time = time && strlen(time)==5 && sscanf(time, "%2d:%2d", &hh, &mm)==2 &&
                          hh>=0 && hh<=23 && mm>=0 && mm<=59 && time[2]==':';
    bool valid = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"ok")) &&
        device && strcmp(device,CONFIG_HILILY_MORNING_DEVICE_ID)==0 &&
        zone && strcmp(zone,"Asia/Ho_Chi_Minh")==0 &&
        cJSON_IsBool(enabled) && cJSON_IsArray(days) && cJSON_GetArraySize(days)>=1 &&
        cJSON_GetArraySize(days)<=7 && cJSON_IsNumber(sec) &&
        sec->valuedouble > 1700000000 && sec->valuedouble < 4102444800.0 && has_time;
    uint8_t mask=0;
    if (valid) {
        const cJSON* day=nullptr;
        cJSON_ArrayForEach(day, days) {
            if (!cJSON_IsNumber(day) || day->valuedouble<0 || day->valuedouble>6 ||
                (int)day->valuedouble != day->valuedouble) { valid=false; break; }
            mask |= static_cast<uint8_t>(1U << day->valueint);
        }
    }
    if (valid) {
        cfg.enabled = cJSON_IsTrue(enabled);
        cfg.minute_of_day = hh*60+mm;
        cfg.days_mask = mask;
        cfg.server_unix = static_cast<int64_t>(sec->valuedouble);
        cfg.fetched_us = esp_timer_get_time();
        cfg.valid = true;
    }
    cJSON_Delete(root);
    return valid;
}

bool PostMorningEvent(const MorningEvent& ev, int& status) {
    // All strings originate on the device; escape them before serializing JSON.
    cJSON* obj=cJSON_CreateObject();
    if (!obj) return false;
    cJSON_AddStringToObject(obj,"day",ev.day.c_str());
    cJSON_AddStringToObject(obj,"run_id",
        (std::string(CONFIG_HILILY_MORNING_DEVICE_ID) + "-" + ev.day).c_str());
    cJSON_AddStringToObject(obj,"action",ev.action.c_str());
    cJSON_AddNumberToObject(obj,"last_completed_segment",ev.completed);
    if (!ev.reason.empty()) cJSON_AddStringToObject(obj,"reason",ev.reason.c_str());
    char* raw=cJSON_PrintUnformatted(obj);
    std::string body=raw?raw:"";
    if (raw) cJSON_free(raw);
    cJSON_Delete(obj);
    if (body.empty()) return false;
    return DeviceHttp(HILILY_WORKER_BASE_URL "/api/device/morning/event",
                      true, body, status);
}

bool CurrentVietnamTime(const MorningConfig& cfg, std::tm& tm, std::string& day) {
    if (!cfg.valid || !cfg.fetched_us) return false;
    const int64_t elapsed = (esp_timer_get_time()-cfg.fetched_us)/1000000;
    if (elapsed<0 || elapsed>900) return false; // Never schedule from stale config.
    const time_t local = static_cast<time_t>(cfg.server_unix+elapsed+7*3600);
    if (!gmtime_r(&local, &tm)) return false;
    char buf[12];
    snprintf(buf,sizeof(buf),"%04d-%02d-%02d",tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday);
    day=buf;
    return true;
}

void ProcessPendingEvents() {
    MorningEvent ev;
    {
        std::lock_guard<std::mutex> lock(event_mutex);
        if (event_queue.empty()) return;
        ev = event_queue.front();
    }
    int status=0;
    if (PostMorningEvent(ev,status) && (status>=200 && status<300 || status==404 || status==409)) {
        std::lock_guard<std::mutex> lock(event_mutex);
        if (!event_queue.empty()) event_queue.pop_front();
    }
}

struct HomeTaskArgs { HiLilyDisplay* display; std::string url; };

void HomeTask(void* parameter) {
    auto* args = static_cast<HomeTaskArgs*>(parameter);
    vTaskDelay(pdMS_TO_TICKS(20000)); // Allow Wi-Fi and TLS to initialize.
    MorningConfig config;
    std::string claimed_day;
    int64_t next_home_us=0, next_schedule_us=0, next_event_us=0;
    while (true) {
        const int64_t now_us=esp_timer_get_time();
        if (now_us>=next_home_us) {
            const bool ok=FetchHome(args->display,args->url.c_str());
            next_home_us=esp_timer_get_time()+(ok?20LL*60:60)*1000000;
        }
#if CONFIG_BOARD_TYPE_ESP32_S3_ZERO_TFT
        if (CONFIG_HILILY_MORNING_DEVICE_ID[0] &&
            strlen(CONFIG_HILILY_MORNING_DEVICE_TOKEN)>=32) {
            if (esp_timer_get_time()>=next_schedule_us) {
                config.valid=false; // Fail closed if a refresh fails.
                const bool ok=FetchMorning(config);
                next_schedule_us=esp_timer_get_time()+(ok?120LL:60LL)*1000000;
            }
            std::tm vn={};
            std::string day;
            if (CurrentVietnamTime(config,vn,day) && config.enabled &&
                (config.days_mask & (1U << vn.tm_wday)) && claimed_day!=day) {
                const int minutes=vn.tm_hour*60+vn.tm_min;
                // Ten-minute postponement maximum. The schedule is local UTC+7.
                if (minutes>=config.minute_of_day &&
                    minutes<config.minute_of_day+10 &&
                    Application::GetInstance().CanStartScheduledBriefing() &&
                    !HiLilyMusicPlayer::GetInstance().IsBusy()) {
                    int status=0;
                    const MorningEvent start={"started",day,"",-1};
                    const bool sent=PostMorningEvent(start,status);
                    if (sent && (status==200 || status==201)) {
                        claimed_day=day;
                        Application::GetInstance().StartScheduledMorningBriefing(day);
                    } else if (sent && status==409) {
                        // Admin D1 has already recorded today's run, including reboots.
                        claimed_day=day;
                    }
                }
            }
            if (esp_timer_get_time()>=next_event_us) {
                ProcessPendingEvents();
                next_event_us=esp_timer_get_time()+30LL*1000000;
            }
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
}

void QueueHiLilyMorningEvent(const std::string& action, const std::string& day,
                            int completed, const char* reason) {
    if (day.empty()) return;
    std::lock_guard<std::mutex> lock(event_mutex);
    if (event_queue.size() >= 8) event_queue.pop_front(); // Bound memory on N4R2.
    event_queue.push_back({action,day,reason?reason:"",completed});
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
