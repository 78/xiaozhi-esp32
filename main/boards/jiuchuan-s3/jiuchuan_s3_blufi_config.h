/*
 * @FileDesc: 
 * @Author: none
 * @Date: 2025-06-30 16:17:03
 * @LastEditTime: 2025-07-01 09:26:02
 * @LastEditors: Hangon66 2630612613@qq.com
 * @Version: 0.0.1
 * @Usage: 
 * @FilePath: \jiuchuan-xiaozhi-sound\main\boards\jiuchuan-s3\jiuchuan_s3_blufi_config.h
 */

#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
#include "esp_bt.h"
#endif

#include "esp_blufi_api.h"

#include "esp_blufi.h"
#include <driver/rtc_io.h>
#include "jiuchuan_s3_blufi.h"
#include "ssid_manager.h"
#define WIFI_LIST_NUM   10

#define EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY 2
#define EXAMPLE_INVALID_REASON                255
#define EXAMPLE_INVALID_RSSI                  -128

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#else
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#endif


class JiuChuanS3BlufiConfigurationAp {
public:
    static JiuChuanS3BlufiConfigurationAp& GetInstance();
	void EnterBluFiConfigMode(void);

private:
    static wifi_config_t sta_config;
    static wifi_config_t ap_config;

    /* FreeRTOS event group to signal when we are connected & ready to make a request */
    static EventGroupHandle_t wifi_event_group;

    /* The event group allows multiple bits for each event,
    but we only care about one event - are we connected
    to the AP with an IP? */
    static const int CONNECTED_BIT = BIT0;

    static uint8_t example_wifi_retry;

    /* store the station info for send back to phone */
    static bool gl_sta_connected;
    static bool gl_sta_got_ip;
    static bool ble_is_connected;
    static uint8_t gl_sta_bssid[6];
    static uint8_t gl_sta_ssid[32];
    static int gl_sta_ssid_len;
    static wifi_sta_list_t gl_sta_list;
    static bool gl_sta_is_connecting;
    static esp_blufi_extra_info_t gl_sta_conn_info;
    static esp_blufi_callbacks_t example_callbacks;

    static void example_record_wifi_conn_info(int rssi, uint8_t reason);
    static void example_wifi_connect(void);
    static bool example_wifi_reconnect(void);
    static int softap_get_current_connection_number(void);
    static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    void initialise_wifi(void);
    static void example_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);
};

