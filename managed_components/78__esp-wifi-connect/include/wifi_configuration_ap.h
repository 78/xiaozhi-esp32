#ifndef _WIFI_CONFIGURATION_AP_H_
#define _WIFI_CONFIGURATION_AP_H_

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>

#include <esp_http_server.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <esp_netif.h>
#include <esp_wifi_types_generic.h>

#include "dns_server.h"
#include "sdkconfig.h"

/**
 * WifiConfigurationAp - WiFi configuration access point
 * 
 * Creates a WiFi hotspot with a captive portal for configuring WiFi credentials.
 * Note: WiFi driver must be initialized before using this class.
 */
class WifiConfigurationAp {
public:
    WifiConfigurationAp();
    ~WifiConfigurationAp();

    // Delete copy constructor and assignment operator
    WifiConfigurationAp(const WifiConfigurationAp&) = delete;
    WifiConfigurationAp& operator=(const WifiConfigurationAp&) = delete;

    void SetSsidPrefix(const std::string &&ssid_prefix);
    void SetSsidPrefix(const std::string &ssid_prefix);
    void SetLanguage(const std::string &&language);
    void SetLanguage(const std::string &language);
    void Start();
    void Stop();
#if !CONFIG_IDF_TARGET_ESP32P4
    void StartSmartConfig();
#endif
    bool ConnectToWifi(const std::string &ssid, const std::string &password);
    void Save(const std::string &ssid, const std::string &password);
    std::vector<wifi_ap_record_t> GetAccessPoints();
    std::string GetSsid();
    std::string GetWebServerUrl();

    /**
     * Set callback for when exit is requested from config mode
     * This is called when user requests to exit config mode (e.g., via /exit endpoint)
     */
    void OnExitRequested(std::function<void()> callback);

private:
    std::mutex mutex_;
    std::unique_ptr<DnsServer> dns_server_;
    httpd_handle_t server_ = NULL;
    EventGroupHandle_t event_group_;
    std::string ssid_prefix_;
    std::string language_;
    esp_event_handler_instance_t instance_any_id_;
    esp_event_handler_instance_t instance_got_ip_;
    esp_timer_handle_t scan_timer_ = nullptr;
    bool is_connecting_ = false;
    esp_netif_t* ap_netif_ = nullptr;
    std::vector<wifi_ap_record_t> ap_records_;

    // 高级配置项
    std::string ota_url_;
    int8_t max_tx_power_;
    bool remember_bssid_;
    bool sleep_mode_;
    
    // Display GPIOs
    int8_t display_mosi_;
    int8_t display_clk_;
    int8_t display_dc_;
    int8_t display_rst_;
    int8_t display_cs_;
    int8_t display_bl_;

    // Audio GPIOs
    int8_t audio_i2s_bclk_;
    int8_t audio_i2s_ws_;
    int8_t audio_i2s_dout_;
    int8_t audio_i2s_din_;
    int8_t audio_pa_enable_;
    int8_t audio_volume_;

    // Display Appearance
    int8_t display_orientation_;
    std::string display_theme_;
    int8_t display_brightness_;

    // Log Console
    static char* log_buffer_;
    static size_t log_pos_;
    static std::mutex log_mutex_;
    static int vprintf_handler(const char* format, va_list args);

    // Callbacks
    std::function<void()> on_exit_requested_;

    void StartAccessPoint();
    void StartWebServer();

    // Event handlers
    static void WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void IpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
#if !CONFIG_IDF_TARGET_ESP32P4
    static void SmartConfigEventHandler(void* arg, esp_event_base_t event_base, 
                                      int32_t event_id, void* event_data);
    esp_event_handler_instance_t sc_event_instance_ = nullptr;
#endif
};

#endif // _WIFI_CONFIGURATION_AP_H_
