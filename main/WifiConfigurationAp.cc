#include "WifiConfigurationAp.h"
#include <cstdio>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "lwip/ip_addr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/task.h"

#define TAG "WifiConfigurationAp"

extern const char index_html_start[] asm("_binary_wifi_configuration_ap_html_start");

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


WifiConfigurationAp::WifiConfigurationAp()
{
    event_group_ = xEventGroupCreate();
}

std::string WifiConfigurationAp::GetSsid()
{
    // Get MAC and use it to generate a unique SSID
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "ESP32-%02X%02X%02X", mac[3], mac[4], mac[5]);
    return std::string(ssid);
}

void WifiConfigurationAp::StartAccessPoint()
{
    // Get the SSID
    std::string ssid = GetSsid();

    // Register the WiFi event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        [](void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Station connected: " MACSTR, MAC2STR(event->mac));
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Station disconnected: " MACSTR, MAC2STR(event->mac));
        } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
            xEventGroupSetBits(static_cast<WifiConfigurationAp *>(ctx)->event_group_, WIFI_CONNECTED_BIT);
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupSetBits(static_cast<WifiConfigurationAp *>(ctx)->event_group_, WIFI_FAIL_BIT);
        }
    }, this));

    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    auto netif = esp_netif_create_default_wifi_ap();

    // Set the router IP address to 192.168.4.1
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(netif);
    esp_netif_set_ip_info(netif, &ip_info);
    esp_netif_dhcps_start(netif);

    // Initialize the WiFi stack in Access Point mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set the WiFi configuration
    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.ap.ssid, ssid.c_str());
    wifi_config.ap.ssid_len = ssid.length();
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    // Start the WiFi Access Point
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Access Point started with SSID %s", ssid.c_str());
}

void WifiConfigurationAp::StartWebServer()
{
    // Start the web server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_ERROR_CHECK(httpd_start(&server_, &config));

    // Register the index.html file
    httpd_uri_t index_html = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            httpd_resp_send(req, index_html_start, strlen(index_html_start));
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &index_html));

    // Register the /scan URI
    httpd_uri_t scan = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            esp_wifi_scan_start(nullptr, true);
            uint16_t ap_num = 0;
            esp_wifi_scan_get_ap_num(&ap_num);
            wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(ap_num * sizeof(wifi_ap_record_t));
            esp_wifi_scan_get_ap_records(&ap_num, ap_records);

            // Send the scan results as JSON
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr_chunk(req, "[");
            for (int i = 0; i < ap_num; i++) {
                ESP_LOGI(TAG, "SSID: %s, RSSI: %d, Authmode: %d",
                    (char *)ap_records[i].ssid, ap_records[i].rssi, ap_records[i].authmode);
                char buf[128];
                snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"rssi\":%d,\"authmode\":%d}",
                    (char *)ap_records[i].ssid, ap_records[i].rssi, ap_records[i].authmode);
                httpd_resp_sendstr_chunk(req, buf);
                if (i < ap_num - 1) {
                    httpd_resp_sendstr_chunk(req, ",");
                }
            }
            httpd_resp_sendstr_chunk(req, "]");
            httpd_resp_sendstr_chunk(req, NULL);
            free(ap_records);
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &scan));

    // Register the form submission
    httpd_uri_t form_submit = {
        .uri = "/submit",
        .method = HTTP_POST,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            char buf[128];
            int ret = httpd_req_recv(req, buf, sizeof(buf));
            if (ret <= 0) {
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                    httpd_resp_send_408(req);
                }
                return ESP_FAIL;
            }
            buf[ret] = '\0';
            ESP_LOGI(TAG, "Received form data: %s", buf);

            // Parse the form data
            char ssid[32], password[64];
            if (sscanf(buf, "ssid=%32[^&]&password=%64s", ssid, password) != 2) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid form data");
                return ESP_FAIL;
            }

            // Get this object from the user context
            auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);
            if (!this_->ConnectToWifi(ssid, password)) {
                char error[] = "Failed to connect to WiFi";
                char location[128];
                snprintf(location, sizeof(location), "/?error=%s&ssid=%s", error, ssid);
                
                httpd_resp_set_status(req, "302 Found");
                httpd_resp_set_hdr(req, "Location", location);
                httpd_resp_send(req, NULL, 0);
                return ESP_OK;
            }

            // Set HTML response
            httpd_resp_set_status(req, "200 OK");
            httpd_resp_set_type(req, "text/html");
            httpd_resp_send(req, "<h1>Done!</h1>", -1);

            this_->Save(ssid, password);
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &form_submit));

    ESP_LOGI(TAG, "Web server started");
}

void WifiConfigurationAp::Start()
{
    builtin_led_.SetBlue();
    builtin_led_.Blink(1000, 500);

    StartAccessPoint();
    StartWebServer();
}

bool WifiConfigurationAp::ConnectToWifi(const std::string &ssid, const std::string &password)
{
    // auto esp_netif = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, ssid.c_str());
    strcpy((char *)wifi_config.sta.password, password.c_str());
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.failure_retry_cnt = 1;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    auto ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi: %d", ret);
        return false;
    }
    ESP_LOGI(TAG, "Connecting to WiFi %s", ssid.c_str());

    // Wait for the connection to complete for 5 seconds
    EventBits_t bits = xEventGroupWaitBits(event_group_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi %s", ssid.c_str());
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi %s", ssid.c_str());
        return false;
    }
}

void WifiConfigurationAp::Save(const std::string &ssid, const std::string &password)
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
