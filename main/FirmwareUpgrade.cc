#include "FirmwareUpgrade.h"
#include "SystemInfo.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_partition.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"


#define TAG "FirmwareUpgrade"


FirmwareUpgrade::FirmwareUpgrade() {
}

FirmwareUpgrade::~FirmwareUpgrade() {
}

void FirmwareUpgrade::CheckVersion() {
    std::string current_version = esp_app_get_description()->version;
    ESP_LOGI(TAG, "Current version: %s", current_version.c_str());

    // Get device info and request the latest firmware from the server
    std::string device_info = SystemInfo::GetJsonString();

    esp_http_client_config_t config = {};
    config.url = CONFIG_OTA_UPGRADE_URL;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Device-Id", SystemInfo::GetMacAddress().c_str());
    esp_err_t err = esp_http_client_open(client, device_info.length());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to perform HTTP request: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }
    auto written = esp_http_client_write(client, device_info.data(), device_info.length());
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write request body: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "Failed to fetch headers");
        esp_http_client_cleanup(client);
        return;
    }

    std::string response;
    response.resize(content_length);
    int ret = esp_http_client_read_response(client, (char*)response.data(), content_length);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to read response content_length=%d", content_length);
        esp_http_client_cleanup(client);
        return;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL
    
    cJSON *root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return;
    }
    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (firmware == NULL) {
        ESP_LOGE(TAG, "Failed to get firmware object");
        cJSON_Delete(root);
        return;
    }
    cJSON *version = cJSON_GetObjectItem(firmware, "version");
    if (version == NULL) {
        ESP_LOGE(TAG, "Failed to get version object");
        cJSON_Delete(root);
        return;
    }
    cJSON *url = cJSON_GetObjectItem(firmware, "url");
    if (url == NULL) {
        ESP_LOGE(TAG, "Failed to get url object");
        cJSON_Delete(root);
        return;
    }

    new_version_ = version->valuestring;
    new_version_url_ = url->valuestring;
    cJSON_Delete(root);

    has_new_version_ = new_version_ != current_version;
    if (has_new_version_) {
        ESP_LOGI(TAG, "New version available: %s", new_version_.c_str());
    } else {
        ESP_LOGI(TAG, "No new version available");
    }
    return;
}

void FirmwareUpgrade::MarkValid() {
    auto partition = esp_ota_get_running_partition();
    if (strcmp(partition->label, "factory") == 0) {
        ESP_LOGI(TAG, "Running from factory partition, skipping");
        return;
    }

    ESP_LOGI(TAG, "Running partition: %s", partition->label);
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(partition, &state) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get state of partition");
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Marking firmware as valid");
        esp_ota_mark_app_valid_cancel_rollback();
    }
}

void FirmwareUpgrade::Upgrade(std::string firmware_url) {
    ESP_LOGI(TAG, "Upgrading firmware from %s", firmware_url.c_str());

    esp_http_client_config_t config = {};
    config.url = firmware_url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = [](esp_http_client_event_t *evt) {
        switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
        }
        return ESP_OK;
    };
    config.keep_alive_enable = true;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;
    esp_err_t err = esp_https_ota(&ota_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to upgrade firmware: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Firmware upgrade successful, rebooting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}
