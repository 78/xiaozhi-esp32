#include "hilily_home_client.h"
#include "hilily_display.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>

#include <cmath>
#include <cstring>
#include <string>

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

struct HomeTaskArgs { HiLilyDisplay* display; std::string url; };

void HomeTask(void* parameter) {
    auto* args = static_cast<HomeTaskArgs*>(parameter);
    vTaskDelay(pdMS_TO_TICKS(20000));  // Allow Wi-Fi setup and SNTP/TLS time sync.
    while (true) {
        const bool ok = FetchHome(args->display, args->url.c_str());
        vTaskDelay(pdMS_TO_TICKS(ok ? 20 * 60 * 1000 : 60 * 1000));
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
