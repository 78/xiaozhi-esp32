#ifndef _WIFI_STATION_H_
#define _WIFI_STATION_H_

#include <string>
#include "esp_event.h"

class WifiStation {
public:
    WifiStation();
    void Start();
    bool IsConnected();
    std::string ssid() { return ssid_; }
    std::string ip_address() { return ip_address_; }
    int8_t rssi() { return rssi_; }
    uint8_t channel() { return channel_; }

private:
    EventGroupHandle_t event_group_;
    std::string ssid_;
    std::string password_;
    std::string ip_address_;
    uint8_t rssi_ = 0;
    uint8_t channel_ = 0;
    int reconnect_count_ = 0;
};

#endif // _WIFI_STATION_H_
