#include "wifi_manager.h"
#include "settings.h"
#include "wifi_station.h"
#include <esp_log.h>

#define TAG "WifiManager"

WifiManager& WifiManager::GetInstance() {
    static WifiManager instance;
    return instance;
}

bool WifiManager::Initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "WifiManager already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing WifiManager...");
    
    // 配置WiFi设置
    ConfigureWifiSettings();
    
    // 设置默认凭据
    SetDefaultCredentials();
    
    initialized_ = true;
    ESP_LOGI(TAG, "WifiManager initialized successfully");
    return true;
}

void WifiManager::ConfigureWifiSettings() {
    ESP_LOGI(TAG, "Configuring WiFi settings...");
    
    // 配置WiFi参数到NVS
    Settings wifi_settings("wifi", true);

    // 设置不记住BSSID (不区分MAC地址)
    wifi_settings.SetInt("remember_bssid", 0);

    // 设置最大发射功率
    wifi_settings.SetInt("max_tx_power", 0);
    
    ESP_LOGI(TAG, "WiFi settings configured");
}

void WifiManager::SetDefaultCredentials() {
    ESP_LOGI(TAG, "Setting default WiFi credentials...");
    
    // 添加默认WiFi配置
    auto &wifi_station = WifiStation::GetInstance();
    wifi_station.AddAuth("xoxo", "12340000");

    ESP_LOGI(TAG, "Default WiFi credentials added: SSID=xoxo, Password=12340000");
}
