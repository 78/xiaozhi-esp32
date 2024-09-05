#include "WifiStation.h"
#include <cstring>

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_system.h"


#define TAG "wifi"
#define WIFI_EVENT_CONNECTED BIT0
#define WIFI_EVENT_FAILED BIT1
#define MAX_RECONNECT_COUNT 5

WifiStation::WifiStation() {
    // Get ssid and password from NVS
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READONLY, &nvs_handle));
    char ssid[32], password[64];
    size_t length = sizeof(ssid);
    ESP_ERROR_CHECK(nvs_get_str(nvs_handle, "ssid", ssid, &length));
    length = sizeof(password);
    ESP_ERROR_CHECK(nvs_get_str(nvs_handle, "password", password, &length));
    nvs_close(nvs_handle);

    ssid_ = std::string(ssid);
    password_ = std::string(password);

    // Create the event group
    event_group_ = xEventGroupCreate();

    // Register event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        [](void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
        auto this_ = static_cast<WifiStation*>(event_handler_arg);
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupClearBits(this_->event_group_, WIFI_EVENT_CONNECTED);
            if (this_->reconnect_count_ < MAX_RECONNECT_COUNT) {
                esp_wifi_connect();
                this_->reconnect_count_++;
                ESP_LOGI(TAG, "Reconnecting to WiFi (attempt %d)", this_->reconnect_count_);
            } else {
                xEventGroupSetBits(this_->event_group_, WIFI_EVENT_FAILED);
                ESP_LOGI(TAG, "Failed to connect to WiFi");
            }
        }
    }, this));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        [](void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
        auto this_ = static_cast<WifiStation*>(event_handler_arg);
        auto event = static_cast<ip_event_got_ip_t*>(event_data);

        char ip_address[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_address, sizeof(ip_address));
        this_->ip_address_ = ip_address;
        ESP_LOGI(TAG, "Got IP: %s", this_->ip_address_.c_str());
        xEventGroupSetBits(this_->event_group_, WIFI_EVENT_CONNECTED);
    }, this));
}


void WifiStation::Start() {
    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    esp_netif_create_default_wifi_sta();

    // Initialize the WiFi stack in station mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_LOGI(TAG, "Connecting to WiFi ssid=%s password=%s", ssid_.c_str(), password_.c_str());
    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, ssid_.c_str());
    strcpy((char *)wifi_config.sta.password, password_.c_str());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start the WiFi stack
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for the WiFi stack to start
    auto bits = xEventGroupWaitBits(event_group_, WIFI_EVENT_CONNECTED | WIFI_EVENT_FAILED, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_EVENT_FAILED) {
        ESP_LOGE(TAG, "WifiStation start failed");
    } else {
        ESP_LOGI(TAG, "WifiStation started");
    }

    // Get station info
    wifi_ap_record_t ap_info;
    ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
    ESP_LOGI(TAG, "Connected to %s rssi=%d channel=%d", ap_info.ssid, ap_info.rssi, ap_info.primary);
    rssi_ = ap_info.rssi;
    channel_ = ap_info.primary;
}

bool WifiStation::IsConnected() {
    return xEventGroupGetBits(event_group_) & WIFI_EVENT_CONNECTED;
}
