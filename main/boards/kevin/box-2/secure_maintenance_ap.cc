#include "secure_maintenance_ap.h"

#include <cstring>

#include <esp_log.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <lwip/ip_addr.h>
#include <mbedtls/platform_util.h>
#include <wifi_manager.h>

#include "local_control_panel.h"

namespace {

const char* TAG = "SecureMaintenanceAP";

bool Check(esp_err_t result, const char* action) {
    if (result == ESP_OK) {
        return true;
    }
    ESP_LOGE(TAG, "%s failed: %s", action, esp_err_to_name(result));
    return false;
}

}  // namespace

SecureMaintenanceAccessPoint::SecureMaintenanceAccessPoint(LocalControlPanel& panel)
    : panel_(panel) {}

SecureMaintenanceAccessPoint::~SecureMaintenanceAccessPoint() {
    Stop();
}

bool SecureMaintenanceAccessPoint::Start() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return true;
        }
    }

    auto& wifi = WifiManager::GetInstance();
    if (!wifi.IsInitialized()) {
        WifiManagerConfig config;
        config.ssid_prefix = "KevinBox";
        config.show_ota_config = false;
        config.show_sleep_config = false;
        if (!wifi.Initialize(config)) {
            ESP_LOGE(TAG, "WiFi manager initialization failed");
            return false;
        }
    }
    wifi.StopConfigAp();
    wifi.StopStation();

    const std::string ssid = GenerateSsid();
    const std::string password = GeneratePassword();
    esp_netif_t* netif = esp_netif_create_default_wifi_ap();
    if (netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create maintenance AP interface");
        return false;
    }

    esp_netif_ip_info_t ip_info = {};
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(netif);
    if (!Check(esp_netif_set_ip_info(netif, &ip_info), "set AP address") ||
        !Check(esp_netif_dhcps_start(netif), "start DHCP server")) {
        esp_netif_destroy_default_wifi(netif);
        return false;
    }

    wifi_config_t wifi_config = {};
    strlcpy(reinterpret_cast<char*>(wifi_config.ap.ssid), ssid.c_str(),
            sizeof(wifi_config.ap.ssid));
    strlcpy(reinterpret_cast<char*>(wifi_config.ap.password), password.c_str(),
            sizeof(wifi_config.ap.password));
    wifi_config.ap.ssid_len = static_cast<uint8_t>(ssid.size());
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.pmf_cfg.capable = true;
    wifi_config.ap.pmf_cfg.required = true;

    const bool wifi_started =
        Check(esp_wifi_set_mode(WIFI_MODE_AP), "set AP mode") &&
        Check(esp_wifi_set_config(WIFI_IF_AP, &wifi_config), "set AP configuration") &&
        Check(esp_wifi_set_ps(WIFI_PS_NONE), "disable AP power save") &&
        Check(esp_wifi_start(), "start maintenance AP");
    mbedtls_platform_zeroize(wifi_config.ap.password, sizeof(wifi_config.ap.password));
    if (!wifi_started) {
        esp_wifi_stop();
        esp_netif_destroy_default_wifi(netif);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ap_netif_ = netif;
        ssid_ = ssid;
        password_ = password;
        running_ = true;
    }
    if (!panel_.Start(LocalControlPanel::Exposure::Maintenance)) {
        Stop();
        return false;
    }
    ESP_LOGI(TAG, "Secure maintenance access point started");
    return true;
}

void SecureMaintenanceAccessPoint::Stop() {
    esp_netif_t* netif = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ && ap_netif_ == nullptr) {
            return;
        }
        netif = ap_netif_;
        ap_netif_ = nullptr;
        running_ = false;
        ssid_.clear();
        if (!password_.empty()) {
            mbedtls_platform_zeroize(password_.data(), password_.size());
        }
        password_.clear();
    }
    panel_.Stop();
    esp_wifi_stop();
    if (netif != nullptr) {
        esp_netif_destroy_default_wifi(netif);
    }
    ESP_LOGI(TAG, "Secure maintenance access point stopped");
}

bool SecureMaintenanceAccessPoint::IsRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

std::string SecureMaintenanceAccessPoint::GetSsid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ssid_;
}

std::string SecureMaintenanceAccessPoint::GetPassword() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return password_;
}

std::string SecureMaintenanceAccessPoint::GenerateSsid() {
    const uint32_t random = esp_random();
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%04X", static_cast<unsigned>(random & 0xffff));
    return std::string("KevinBox-") + suffix;
}

std::string SecureMaintenanceAccessPoint::GeneratePassword() {
    static constexpr char kAlphabet[] =
        "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    std::string password(14, 'A');
    for (char& character : password) {
        character = kAlphabet[esp_random() % (sizeof(kAlphabet) - 1)];
    }
    return password;
}
