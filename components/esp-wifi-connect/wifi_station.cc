#include "wifi_station.h"
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs.h>
#include "nvs_flash.h"
#include <esp_netif.h>
#include <esp_system.h>

#define TAG "wifi"
#define WIFI_EVENT_CONNECTED BIT0
#define WIFI_EVENT_FAILED BIT1
#define MAX_RECONNECT_COUNT 5

WifiStation& WifiStation::GetInstance() {
    static WifiStation instance;
    return instance;
}

WifiStation::WifiStation() {
    // Create the event group
    event_group_ = xEventGroupCreate();

    // Get ssid and password from NVS
    nvs_handle_t nvs_handle;
    auto ret = nvs_open("wifi", NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        char ssid[32], password[64];
        size_t length = sizeof(ssid);
        ESP_ERROR_CHECK(nvs_get_str(nvs_handle, "ssid", ssid, &length));
        length = sizeof(password);
        ESP_ERROR_CHECK(nvs_get_str(nvs_handle, "password", password, &length));
        nvs_close(nvs_handle);

        ssid_ = std::string(ssid);
        password_ = std::string(password);
    }
}

WifiStation::~WifiStation() {
    vEventGroupDelete(event_group_);
}

void WifiStation::SetAuth(const std::string &&ssid, const std::string &&password) {
    ssid_ = ssid;
    password_ = password;
}

void WifiStation::Start() {
    if (ssid_.empty()) {
        return;
    }

    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WifiStation::WifiEventHandler,
                                                        this,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WifiStation::IpEventHandler,
                                                        this,
                                                        &instance_got_ip));

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
        ESP_LOGE(TAG, "WifiStation failed");
        // Reset the WiFi stack
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_deinit());
        
        // 取消注册事件处理程序
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
        return;
    }

    ESP_LOGI(TAG, "Connected to %s rssi=%d channel=%d", ssid_.c_str(), GetRssi(), GetChannel());
}

int8_t WifiStation::GetRssi() {
    // Get station info
    wifi_ap_record_t ap_info;
    ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
    return ap_info.rssi;
}

uint8_t WifiStation::GetChannel() {
    // Get station info
    wifi_ap_record_t ap_info;
    ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
    return ap_info.primary;
}

bool WifiStation::IsConnected() {
    return xEventGroupGetBits(event_group_) & WIFI_EVENT_CONNECTED;
}

// Static event handler functions
void WifiStation::WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    auto* this_ = static_cast<WifiStation*>(arg);
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(this_->event_group_, WIFI_EVENT_CONNECTED);
        if (this_->reconnect_count_ < MAX_RECONNECT_COUNT) {
            esp_wifi_connect();
            this_->reconnect_count_++;
            ESP_LOGI(TAG, "Reconnecting WiFi (attempt %d)", this_->reconnect_count_);
        } else {
            xEventGroupSetBits(this_->event_group_, WIFI_EVENT_FAILED);
            ESP_LOGI(TAG, "WiFi connection failed");
        }
    }
}

void WifiStation::IpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    auto* this_ = static_cast<WifiStation*>(arg);
    auto* event = static_cast<ip_event_got_ip_t*>(event_data);

    char ip_address[16];
    esp_ip4addr_ntoa(&event->ip_info.ip, ip_address, sizeof(ip_address));
    this_->ip_address_ = ip_address;
    ESP_LOGI(TAG, "Got IP: %s", this_->ip_address_.c_str());
    xEventGroupSetBits(this_->event_group_, WIFI_EVENT_CONNECTED);
}
