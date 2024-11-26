#ifndef WIFI_SMART_CONFIG_H
#define WIFI_SMART_CONFIG_H


class WifiSmartConfig
{
private:
    /* data */
    WifiSmartConfig(/* args */);
    ~WifiSmartConfig();

public:
    static WifiSmartConfig& GetInstance(void);

    static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    static void smartconfig_example_task(void * parm);
    void initialise_wifi(void);
};











#endif