#include "wifi_Smart_config.h"
#include "wifi_station.h"
#include <cstring>  // 包含 memcpy
#include <strings.h>  // 包含 bzero

const char *WiFiSmartConfig::TAG = "smartconfig_example";

WiFiSmartConfig::WiFiSmartConfig() {
    s_wifi_event_group = xEventGroupCreate();
}

WiFiSmartConfig::~WiFiSmartConfig() {
    vEventGroupDelete(s_wifi_event_group);
}

void WiFiSmartConfig::initialise_wifi() {
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, this));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFiSmartConfig::start_smartconfig() {
    xTaskCreatePinnedToCore(smartconfig_example_task, "smartconfig_example_task", 4096, this, 3, NULL, tskNO_AFFINITY);
}

void WiFiSmartConfig::event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    WiFiSmartConfig *self = static_cast<WiFiSmartConfig*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        self->start_smartconfig();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(self->s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(self->s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

#ifdef CONFIG_SET_MAC_ADDRESS_OF_TARGET_AP
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            ESP_LOGI(TAG, "Set MAC address of target AP: "MACSTR" ", MAC2STR(evt->bssid));
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }
#endif
        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
            ESP_LOGI(TAG, "RVD_DATA:");
            for (int i = 0; i < 33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }
        // 使用 std::string 构造函数将 uint8_t 数组转换为 std::string
        std::string ssid_str(reinterpret_cast<const char*>(ssid));
        std::string password_str(reinterpret_cast<const char*>(password));

        // 保存数据
        self->Save(ssid_str, password_str);

        //连接网络
        // ESP_ERROR_CHECK( esp_wifi_disconnect() );
        // ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        // esp_wifi_connect();
        // auto& wifi_station = WifiStation::GetInstance();
        // wifi_station.Start();

    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(self->s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

void WiFiSmartConfig::smartconfig_example_task(void * parm) {
    WiFiSmartConfig *self = static_cast<WiFiSmartConfig*>(parm);
    EventBits_t uxBits;

    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));

    while (1) {
        uxBits = xEventGroupWaitBits(self->s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

void WiFiSmartConfig::Save(const std::string &ssid, const std::string &password)
{
    // Open the NVS flash
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &nvs_handle));

    // Write the SSID and password to the NVS flash
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid.c_str()));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "password", password.c_str()));

    // Commit the changes
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));

    // Close the NVS flash
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "WiFi configuration saved");
    // Use xTaskCreate to create a new task that restarts the ESP32
    xTaskCreate([](void *ctx) {
        ESP_LOGI(TAG, "Restarting the ESP32 in 3 second");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }, "restart_task", 4096, NULL, 5, NULL);
}
