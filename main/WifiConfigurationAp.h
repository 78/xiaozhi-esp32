#ifndef _WIFI_CONFIGURATION_AP_H_
#define _WIFI_CONFIGURATION_AP_H_

#include <string>
#include "esp_http_server.h"
#include "BuiltinLed.h"

class WifiConfigurationAp {
public:
    WifiConfigurationAp();
    void Start();

private:
    BuiltinLed builtin_led_;
    httpd_handle_t server_ = NULL;
    EventGroupHandle_t event_group_;

    std::string GetSsid();
    void StartAccessPoint();
    void StartWebServer();
    bool ConnectToWifi(const std::string &ssid, const std::string &password);
    void Save(const std::string &ssid, const std::string &password);
};

#endif // _WIFI_CONFIGURATION_AP_H_
