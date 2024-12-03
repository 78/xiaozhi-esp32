#ifndef WIFI_SMART_CONFIG_H
#define WIFI_SMART_CONFIG_H

#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_smartconfig.h>


class WiFiSmartConfig {
public:
    static WiFiSmartConfig& GetInstance() {
        static WiFiSmartConfig instance;
        return instance;
    }

    WiFiSmartConfig(const WiFiSmartConfig&) = delete;
    WiFiSmartConfig& operator=(const WiFiSmartConfig&) = delete;

    void initialise_wifi();
    void start_smartconfig();
    void Save(const std::string &ssid, const std::string &password);

private:
    WiFiSmartConfig();
    ~WiFiSmartConfig();

    static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void smartconfig_example_task(void * parm);
    
    
    EventGroupHandle_t s_wifi_event_group;
    static const int CONNECTED_BIT = BIT0;
    static const int ESPTOUCH_DONE_BIT = BIT1;
    static const char *TAG;
};

#endif // WIFI_SMART_CONFIG_H
