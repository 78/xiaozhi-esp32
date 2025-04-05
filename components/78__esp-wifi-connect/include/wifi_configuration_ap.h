#ifndef _WIFI_CONFIGURATION_AP_H_
#define _WIFI_CONFIGURATION_AP_H_

#include <string>
#include <esp_http_server.h>
#include <esp_event.h>
#include <esp_timer.h>
#include "dns_server.h"
#include <esp_netif.h>
struct ServerUrls {
    char ota_url[128];
    char websocket_url[128];
};
#define NVS_NAMESPACE "wifi"

class WifiConfigurationAp {
public:
    static WifiConfigurationAp& GetInstance();
    void SetSsidPrefix(const std::string &&ssid_prefix);
    void SetLanguage(const std::string &&language);
    void Start();
    void Stop();
    void StartSmartConfig();

    std::string GetSsid();
    std::string GetWebServerUrl();

    // Delete copy constructor and assignment operator
    WifiConfigurationAp(const WifiConfigurationAp&) = delete;
    WifiConfigurationAp& operator=(const WifiConfigurationAp&) = delete;

private:
    // Private constructor
    WifiConfigurationAp();
    ~WifiConfigurationAp();

    DnsServer dns_server_;
    httpd_handle_t server_ = NULL;
    EventGroupHandle_t event_group_;
    std::string ssid_prefix_;
    std::string language_;
    esp_event_handler_instance_t instance_any_id_;
    esp_event_handler_instance_t instance_got_ip_;
    esp_timer_handle_t scan_timer_ = nullptr;
    bool is_connecting_ = false;
    esp_netif_t* ap_netif_ = nullptr;

    void StartAccessPoint();
    void StartWebServer();
    bool ConnectToWifi(const std::string &ssid, const std::string &password);
    void Save(const std::string &ssid, const std::string &password);
    void Save_server_url(const std::string &ota_url, const std::string &websocket_url);
    ServerUrls Read_server_url();

    // Event handlers
    static void WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void IpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void SmartConfigEventHandler(void* arg, esp_event_base_t event_base, 
                                      int32_t event_id, void* event_data);
    esp_event_handler_instance_t sc_event_instance_ = nullptr;
};

#endif // _WIFI_CONFIGURATION_AP_H_
