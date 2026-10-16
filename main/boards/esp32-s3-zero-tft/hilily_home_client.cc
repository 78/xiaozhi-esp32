#include "hilily_home_client.h"
#include "hilily_display.h"
#include "hilily_music_player.h"
#include "application.h"
#include "config.h"
#include "settings.h"

#include <esp_mac.h>
#include <esp_system.h>

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

// Runtime identity persists in NVS; no device key is compiled into the
// distributable N4R2 image. Each physical ESP32 owns a fresh 256-bit secret.
struct PairState {
    std::string hardware_id, secret, code, device_id;
    bool started = false;
    bool paired = false;
    int64_t next_request_us = 0;
    int64_t next_display_us = 0;
};
PairState pair;

std::string RandomHex32() {
    uint8_t bytes[32];
    esp_fill_random(bytes, sizeof(bytes));
    char hex[65];
    for (int i=0;i<32;i++) snprintf(hex+i*2, 3, "%02x", bytes[i]);
    return std::string(hex,64);
}

void NewPairCode() {
    char pin[9];
    snprintf(pin,sizeof(pin),"%08u",static_cast<unsigned>(esp_random()%100000000U));
    pair.code=pin;
    Settings nvs("morning_pair",true);
    nvs.SetString("pin",pair.code);
}

void InitializePairIdentity() {
    uint8_t mac[6]={};
    if (esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK) {
        ESP_LOGE(TAG,"MAC address unavailable; morning pairing disabled");
        return;
    }
    char hw[16]={};
    snprintf(hw,sizeof(hw),"s3_%02x%02x%02x%02x%02x%02x",
             mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    pair.hardware_id=hw;
    Settings nvs("morning_pair",true);
    pair.secret=nvs.GetString("secret","");
    if(pair.secret.size()!=64) {
        pair.secret=RandomHex32();
        nvs.SetString("secret",pair.secret);
    }
    pair.code=nvs.GetString("pin","");
    if(pair.code.size()!=8) NewPairCode();
    ESP_LOGI(TAG,"Morning pairing hardware ID: %s",pair.hardware_id.c_str());
}

void ShowPairingCode(HiLilyDisplay* display) {
    if(!display||pair.paired||pair.hardware_id.empty())return;
    const std::string notice=std::string("GHEP LOA tren Dashboard")
        +"\nID: "+pair.hardware_id+"\nMA: "+pair.code;
    Application::GetInstance().Schedule([display,notice](){
        if(Application::GetInstance().GetDeviceState()==kDeviceStateIdle)
            display->SetChatMessage("system",notice.c_str());
    });
    ESP_LOGI(TAG,"Pairing pending; enter the 8-digit PIN displayed on the speaker");
}
std::mutex event_mutex;
std::deque<MorningEvent> event_queue;

bool DeviceHttp(const char* endpoint, bool post, const std::string& body,
                int& status, std::string* response_text=nullptr) {
    if(pair.hardware_id.empty()||pair.secret.size()!=64)return false;
    RequestContext response;
    esp_http_client_config_t cfg={};
    cfg.url=endpoint;
    cfg.method=post?HTTP_METHOD_POST:HTTP_METHOD_GET;
    cfg.timeout_ms=8000;
    cfg.crt_bundle_attach=esp_crt_bundle_attach;
    cfg.event_handler=OnHttpEvent;
    cfg.user_data=&response;
    cfg.buffer_size=1024;
    esp_http_client_handle_t client=esp_http_client_init(&cfg);
    if(!client)return false;
    const std::string bearer=std::string("Bearer ")+pair.secret;
    esp_http_client_set_header(client,"Authorization",bearer.c_str());
    esp_http_client_set_header(client,"X-Xiaozhi-Hardware-ID",pair.hardware_id.c_str());
    if(post) {
        esp_http_client_set_header(client,"Content-Type","application/json");
        esp_http_client_set_post_field(client,body.c_str(),body.size());
    }
    const esp_err_t err=esp_http_client_perform(client);
    status=esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if(response_text)*response_text=std::move(response.payload);
    if(err!=ESP_OK||response.overflow) {
        ESP_LOGW(TAG,"Device API transport failed: %s",esp_err_to_name(err));
        return false;
    }
    return true;
}

bool PairRequest(bool start, int& status, std::string& text) {
    cJSON* root=cJSON_CreateObject();
    if(!root)return false;
    cJSON_AddStringToObject(root,"hardware_id",pair.hardware_id.c_str());
    cJSON_AddStringToObject(root,"secret",pair.secret.c_str());
    if(start)cJSON_AddStringToObject(root,"code",pair.code.c_str());
    char* raw=cJSON_PrintUnformatted(root);
    const std::string body=raw?raw:"";
    if(raw)cJSON_free(raw);
    cJSON_Delete(root);
    if(body.empty())return false;
    return DeviceHttp(start?HILILY_WORKER_BASE_URL "/api/device/pair/start":
                            HILILY_WORKER_BASE_URL "/api/device/pair/poll",
                      true,body,status,&text);
}

bool ProcessPairing(HiLilyDisplay* display) {
    if(pair.hardware_id.empty())return false;
    if(pair.paired)return true;
    if(esp_timer_get_time()<pair.next_request_us)return false;
    pair.next_request_us=esp_timer_get_time()+15LL*1000000;
    int status=0;
    std::string result;
    if(!PairRequest(!pair.started,status,result))return false;
    cJSON* body=cJSON_ParseWithLength(result.data(),result.size());
    if(body) {
        const char* state=JsonString(body,"status");
        if(status==200&&state&&strcmp(state,"paired")==0) {
            const char* assigned=JsonString(body,"device_id");
            if(assigned&&strlen(assigned)>=2&&strlen(assigned)<=64) {
                pair.device_id=assigned;
                pair.paired=true;
                ESP_LOGI(TAG,"Morning schedule paired to Family device %s",assigned);
                Application::GetInstance().Schedule([display,assigned=pair.device_id](){
                    display->SetChatMessage("system",("Da ghep loa: "+assigned).c_str());
                });
            } else {
                // A successful /start response only confirms the credential.
                // The /poll response supplies the bound Family device ID.
                pair.started=true;
            }
        } else if((status==202||status==200)&&state&&strcmp(state,"pending")==0) {
            pair.started=true;
        } else if(status==200&&state&&strcmp(state,"expired")==0) {
            NewPairCode();
            pair.started=false;
            pair.next_request_us=0;
        }
        cJSON_Delete(body);
    }
    if(!pair.paired && esp_timer_get_time()>=pair.next_display_us) {
        ShowPairingCode(display);
        pair.next_display_us=esp_timer_get_time()+20LL*1000000;
    }
    if(status==401 && pair.started)pair.started=false;
    return pair.paired;
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
        device && device==pair.device_id &&
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
        (pair.device_id + "-" + ev.day).c_str());
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
    InitializePairIdentity();
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
        if (ProcessPairing(args->display)) {
            if (esp_timer_get_time()>=next_schedule_us) {
                config.valid=false; // Fail closed if a refresh fails.
                const bool ok=FetchMorning(config);
                // Fail closed on transient HTTP errors without forgetting the
                // pairing: only explicitly revoked credentials require re-pairing.
                if(!ok) ESP_LOGW(TAG,"Morning schedule refresh failed; skipping until retry");
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
