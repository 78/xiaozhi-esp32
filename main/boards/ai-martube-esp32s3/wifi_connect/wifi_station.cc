#include "wifi_station.h"
#include <cstring>
#include <algorithm>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs.h>
#include "nvs_flash.h"
#include <esp_netif.h>
#include <esp_system.h>
#include "ssid_manager.h"

#define TAG "WifiStation"
#define WIFI_EVENT_CONNECTED BIT0
#define MAX_RECONNECT_COUNT 5

WifiStation& WifiStation::GetInstance() {
    static WifiStation instance;
    return instance;
}

WifiStation::WifiStation() {
    // Create the event group
    event_group_ = xEventGroupCreate();

    // 读取配置
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %d", err);
    }
    err = nvs_get_i8(nvs, "max_tx_power", &max_tx_power_);
    if (err != ESP_OK) {
        max_tx_power_ = 0;
    }
    err = nvs_get_u8(nvs, "remember_bssid", &remember_bssid_);
    if (err != ESP_OK) {
        remember_bssid_ = 0;
    }
    nvs_close(nvs);
}

WifiStation::~WifiStation() {
    vEventGroupDelete(event_group_);
}

void WifiStation::AddAuth(const std::string &&ssid, const std::string &&password) {
    auto& ssid_manager = SsidManager::GetInstance();
    ssid_manager.AddSsid(ssid, password);
}

void WifiStation::Stop() {
    if (timer_handle_ != nullptr) {
        esp_timer_stop(timer_handle_);
        esp_timer_delete(timer_handle_);
        timer_handle_ = nullptr;
    }

    esp_wifi_scan_stop();
    
    // 取消注册事件处理程序
    if (instance_any_id_ != nullptr) {
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id_));
        instance_any_id_ = nullptr;
    }
    if (instance_got_ip_ != nullptr) {
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip_));
        instance_got_ip_ = nullptr;
    }

    // Reset the WiFi stack
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());

    if (station_netif_ != nullptr) {
        // TODO: esp_netif_destroy will cause crash
        // esp_netif_destroy(station_netif_);
        station_netif_ = nullptr;
    }
}

void WifiStation::OnScanBegin(std::function<void()> on_scan_begin) {
    on_scan_begin_ = on_scan_begin;
}

void WifiStation::OnConnect(std::function<void(const std::string& ssid)> on_connect) {
    on_connect_ = on_connect;
}

void WifiStation::OnConnected(std::function<void(const std::string& ssid)> on_connected) {
    on_connected_ = on_connected;
}

void WifiStation::Start() {
    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WifiStation::WifiEventHandler,
                                                        this,
                                                        &instance_any_id_));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WifiStation::IpEventHandler,
                                                        this,
                                                        &instance_got_ip_));

    // Create the default event loop
    station_netif_ = esp_netif_create_default_wifi_sta();

    // Initialize the WiFi stack in station mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = false;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    if (max_tx_power_ != 0) {
        ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(max_tx_power_));
    }

    // Setup the timer to scan WiFi
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            esp_wifi_scan_start(nullptr, false);
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "WiFiScanTimer",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
}

bool WifiStation::WaitForConnected(int timeout_ms) {
    auto bits = xEventGroupWaitBits(event_group_, WIFI_EVENT_CONNECTED, pdFALSE, pdFALSE, timeout_ms / portTICK_PERIOD_MS);
    return (bits & WIFI_EVENT_CONNECTED) != 0;
}

void WifiStation::HandleScanResult() {
    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(ap_num * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&ap_num, ap_records);
    // sort by rssi descending
    std::sort(ap_records, ap_records + ap_num, [](const wifi_ap_record_t& a, const wifi_ap_record_t& b) {
        return a.rssi > b.rssi;
    });

    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    for (int i = 0; i < ap_num; i++) {
        auto ap_record = ap_records[i];
        auto it = std::find_if(ssid_list.begin(), ssid_list.end(), [ap_record](const SsidItem& item) {
            return strcmp((char *)ap_record.ssid, item.ssid.c_str()) == 0;
        });
        if (it != ssid_list.end()) {
            ESP_LOGI(TAG, "Found AP: %s, BSSID: %02x:%02x:%02x:%02x:%02x:%02x, RSSI: %d, Channel: %d, Authmode: %d",
                (char *)ap_record.ssid, 
                ap_record.bssid[0], ap_record.bssid[1], ap_record.bssid[2],
                ap_record.bssid[3], ap_record.bssid[4], ap_record.bssid[5],
                ap_record.rssi, ap_record.primary, ap_record.authmode);
            WifiApRecord record = {
                .ssid = it->ssid,
                .password = it->password,
                .channel = ap_record.primary,
                .authmode = ap_record.authmode
            };
            memcpy(record.bssid, ap_record.bssid, 6);
            connect_queue_.push_back(record);
        }
    }
    free(ap_records);

    if (connect_queue_.empty()) {
        ESP_LOGI(TAG, "Wait for next scan");
        esp_timer_start_once(timer_handle_, 10 * 1000);
        return;
    }

    StartConnect();
}

void WifiStation::StartConnect() {
    auto ap_record = connect_queue_.front();
    connect_queue_.erase(connect_queue_.begin());
    ssid_ = ap_record.ssid;
    password_ = ap_record.password;

    if (on_connect_) {
        on_connect_(ssid_);
    }

    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, ap_record.ssid.c_str());
    strcpy((char *)wifi_config.sta.password, ap_record.password.c_str());
    if (remember_bssid_) {
        wifi_config.sta.channel = ap_record.channel;
        memcpy(wifi_config.sta.bssid, ap_record.bssid, 6);
        wifi_config.sta.bssid_set = true;
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    reconnect_count_ = 0;
    ESP_ERROR_CHECK(esp_wifi_connect());
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

void WifiStation::SetPowerSaveMode(bool enabled) {
    ESP_ERROR_CHECK(esp_wifi_set_ps(enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE));
}

// Static event handler functions
void WifiStation::WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    auto* this_ = static_cast<WifiStation*>(arg);
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_scan_start(nullptr, false);
        if (this_->on_scan_begin_) {
            this_->on_scan_begin_();
        }
    } else if (event_id == WIFI_EVENT_SCAN_DONE) {
        this_->HandleScanResult();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(this_->event_group_, WIFI_EVENT_CONNECTED);
        if (this_->reconnect_count_ < MAX_RECONNECT_COUNT) {
            esp_wifi_connect();
            this_->reconnect_count_++;
            ESP_LOGI(TAG, "Reconnecting %s (attempt %d / %d)", this_->ssid_.c_str(), this_->reconnect_count_, MAX_RECONNECT_COUNT);
            return;
        }

        if (!this_->connect_queue_.empty()) {
            this_->StartConnect();
            return;
        }
        
        ESP_LOGI(TAG, "No more AP to connect, wait for next scan");
        esp_timer_start_once(this_->timer_handle_, 10 * 1000);
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
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
    if (this_->on_connected_) {
        this_->on_connected_(this_->ssid_);
    }
    this_->connect_queue_.clear();
    this_->reconnect_count_ = 0;
}
