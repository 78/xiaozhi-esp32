#include "wifi_configuration_ap.h"
#include <cstdio>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <lwip/ip_addr.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <cJSON.h>
#include <esp_smartconfig.h>
#include "ssid_manager.h"

#define TAG "WifiConfigurationAp"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

extern const char index_html_start[] asm("_binary_wifi_configuration_html_start");
extern const char done_html_start[] asm("_binary_wifi_configuration_done_html_start");

WifiConfigurationAp& WifiConfigurationAp::GetInstance() {
    static WifiConfigurationAp instance;
    return instance;
}

WifiConfigurationAp::WifiConfigurationAp()
{
    event_group_ = xEventGroupCreate();
    language_ = "zh-CN";
    sleep_mode_ = false;
}

std::vector<wifi_ap_record_t> WifiConfigurationAp::GetAccessPoints()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return ap_records_;
}   

WifiConfigurationAp::~WifiConfigurationAp()
{
    if (scan_timer_) {
        esp_timer_stop(scan_timer_);
        esp_timer_delete(scan_timer_);
    }
    if (event_group_) {
        vEventGroupDelete(event_group_);
    }
    // Unregister event handlers if they were registered
    if (instance_any_id_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id_);
    }
    if (instance_got_ip_) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip_);
    }
}

void WifiConfigurationAp::SetLanguage(const std::string &&language)
{
    language_ = language;
}

void WifiConfigurationAp::SetSsidPrefix(const std::string &&ssid_prefix)
{
    ssid_prefix_ = ssid_prefix;
}

void WifiConfigurationAp::Start()
{
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WifiConfigurationAp::WifiEventHandler,
                                                        this,
                                                        &instance_any_id_));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WifiConfigurationAp::IpEventHandler,
                                                        this,
                                                        &instance_got_ip_));

    StartAccessPoint();
    StartWebServer();
    
    // Start scan immediately
    esp_wifi_scan_start(nullptr, false);
    // Setup periodic WiFi scan timer
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            auto* self = static_cast<WifiConfigurationAp*>(arg);
            if (!self->is_connecting_) {
                esp_wifi_scan_start(nullptr, false);
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifi_scan_timer",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &scan_timer_));
}

std::string WifiConfigurationAp::GetSsid()
{
    // Get MAC and use it to generate a unique SSID
    uint8_t mac[6];
#if CONFIG_IDF_TARGET_ESP32P4
    esp_wifi_get_mac(WIFI_IF_AP, mac);
#else
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
#endif
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s-%02X%02X", ssid_prefix_.c_str(), mac[4], mac[5]);
    return std::string(ssid);
}

std::string WifiConfigurationAp::GetWebServerUrl()
{
    // http://192.168.4.1
    return "http://192.168.4.1";
}

void WifiConfigurationAp::StartAccessPoint()
{
    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    ap_netif_ = esp_netif_create_default_wifi_ap();

    // Set the router IP address to 192.168.4.1
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(ap_netif_);
    esp_netif_set_ip_info(ap_netif_, &ip_info);
    esp_netif_dhcps_start(ap_netif_);
    // Start the DNS server
    dns_server_.Start(ip_info.gw);

    // Initialize the WiFi stack in Access Point mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Get the SSID
    std::string ssid = GetSsid();

    // Set the WiFi configuration
    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.ap.ssid, ssid.c_str());
    wifi_config.ap.ssid_len = ssid.length();
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    // Start the WiFi Access Point
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

#ifdef CONFIG_SOC_WIFI_SUPPORT_5G
    // Temporarily use only 2.4G Wi-Fi.
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY));
#endif

    ESP_LOGI(TAG, "Access Point started with SSID %s", ssid.c_str());

    // 加载高级配置
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        // 读取OTA URL
        char ota_url[256] = {0};
        size_t ota_url_size = sizeof(ota_url);
        err = nvs_get_str(nvs, "ota_url", ota_url, &ota_url_size);
        if (err == ESP_OK) {
            ota_url_ = ota_url;
        }

        // 读取WiFi功率
        err = nvs_get_i8(nvs, "max_tx_power", &max_tx_power_);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "WiFi max tx power from NVS: %d", max_tx_power_);
            ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(max_tx_power_));
        } else {
            esp_wifi_get_max_tx_power(&max_tx_power_);
        }

        // 读取BSSID记忆设置
        uint8_t remember_bssid = 0;
        err = nvs_get_u8(nvs, "remember_bssid", &remember_bssid);
        if (err == ESP_OK) {
            remember_bssid_ = remember_bssid != 0;
        } else {
            remember_bssid_ = false; // 默认值
        }

        // 读取睡眠模式设置
        uint8_t sleep_mode = 0;
        err = nvs_get_u8(nvs, "sleep_mode", &sleep_mode);
        if (err == ESP_OK) {
            sleep_mode_ = sleep_mode != 0;
        } else {
            sleep_mode_ = true; // 默认值
        }

        nvs_close(nvs);
    }
}

void WifiConfigurationAp::StartWebServer()
{
    // Start the web server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 24;
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_ERROR_CHECK(httpd_start(&server_, &config));

    // Register the index.html file
    httpd_uri_t index_html = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, index_html_start, strlen(index_html_start));
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &index_html));

    // Register the /saved/list URI
    httpd_uri_t saved_list = {
        .uri = "/saved/list",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            auto ssid_list = SsidManager::GetInstance().GetSsidList();
            std::string json_str = "[";
            for (const auto& ssid : ssid_list) {
                json_str += "\"" + ssid.ssid + "\",";
            }
            if (json_str.length() > 1) {
                json_str.pop_back(); // Remove the last comma
            }
            json_str += "]";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, json_str.c_str(), HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &saved_list));

    // Register the /saved/set_default URI
    httpd_uri_t saved_set_default = {
        .uri = "/saved/set_default",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            std::string uri = req->uri;
            auto pos = uri.find("?index=");
            if (pos != std::string::npos) {
                int index = -1;
                sscanf(&req->uri[pos+7], "%d", &index);
                ESP_LOGI(TAG, "Set default item %d", index);
                SsidManager::GetInstance().SetDefaultSsid(index);
            }
            // send {}
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &saved_set_default));

    // Register the /saved/delete URI
    httpd_uri_t saved_delete = {
        .uri = "/saved/delete",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            std::string uri = req->uri;
            auto pos = uri.find("?index=");
            if (pos != std::string::npos) {
                int index = -1;
                sscanf(&req->uri[pos+7], "%d", &index);
                ESP_LOGI(TAG, "Delete saved list item %d", index);
                SsidManager::GetInstance().RemoveSsid(index);
            }
            // send {}
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &saved_delete));

    // Register the /scan URI
    httpd_uri_t scan = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);
            std::lock_guard<std::mutex> lock(this_->mutex_);

            // Send the scan results as JSON
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_sendstr_chunk(req, "[");
            for (int i = 0; i < this_->ap_records_.size(); i++) {
                ESP_LOGI(TAG, "SSID: %s, RSSI: %d, Authmode: %d",
                    (char *)this_->ap_records_[i].ssid, this_->ap_records_[i].rssi, this_->ap_records_[i].authmode);
                char buf[128];
                snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"rssi\":%d,\"authmode\":%d}",
                    (char *)this_->ap_records_[i].ssid, this_->ap_records_[i].rssi, this_->ap_records_[i].authmode);
                httpd_resp_sendstr_chunk(req, buf);
                if (i < this_->ap_records_.size() - 1) {
                    httpd_resp_sendstr_chunk(req, ",");
                }
            }
            httpd_resp_sendstr_chunk(req, "]");
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &scan));

    // Register the form submission
    httpd_uri_t form_submit = {
        .uri = "/submit",
        .method = HTTP_POST,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            char *buf;
            size_t buf_len = req->content_len;
            if (buf_len > 1024) { // 限制最大请求体大小
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
                return ESP_FAIL;
            }

            buf = (char *)malloc(buf_len + 1);
            if (!buf) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
                return ESP_FAIL;
            }

            int ret = httpd_req_recv(req, buf, buf_len);
            if (ret <= 0) {
                free(buf);
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                    httpd_resp_send_408(req);
                } else {
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request");
                }
                return ESP_FAIL;
            }
            buf[ret] = '\0';

            // 解析 JSON 数据
            cJSON *json = cJSON_Parse(buf);
            free(buf);
            if (!json) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
                return ESP_FAIL;
            }

            cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(json, "ssid");
            cJSON *password_item = cJSON_GetObjectItemCaseSensitive(json, "password");

            if (!cJSON_IsString(ssid_item) || (ssid_item->valuestring == NULL) || (strlen(ssid_item->valuestring) >= 33)) {
                cJSON_Delete(json);
                httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid SSID\"}", HTTPD_RESP_USE_STRLEN);
                return ESP_OK;
            }

            std::string ssid_str = ssid_item->valuestring;
            std::string password_str = "";
            if (cJSON_IsString(password_item) && (password_item->valuestring != NULL) && (strlen(password_item->valuestring) < 65)) {
                password_str = password_item->valuestring;
            }

            // 获取当前对象
            auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);
            if (!this_->ConnectToWifi(ssid_str, password_str)) {
                cJSON_Delete(json);
                httpd_resp_send(req, "{\"success\":false,\"error\":\"Failed to connect to the Access Point\"}", HTTPD_RESP_USE_STRLEN);
                return ESP_OK;
            }

            this_->Save(ssid_str, password_str);
            cJSON_Delete(json);
            // 设置成功响应
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &form_submit));

    // Register the done.html page
    httpd_uri_t done_html = {
        .uri = "/done.html",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, done_html_start, strlen(done_html_start));
            return ESP_OK;
        },
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &done_html));

    // Register the reboot endpoint
    httpd_uri_t reboot = {
        .uri = "/reboot",
        .method = HTTP_POST,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            auto* this_ = static_cast<WifiConfigurationAp*>(req->user_ctx);
            
            // 设置响应头，防止浏览器缓存
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Cache-Control", "no-store");
            httpd_resp_set_hdr(req, "Connection", "close");
            // 发送响应
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
            
            // 创建一个延迟重启任务
            ESP_LOGI(TAG, "Rebooting...");
            xTaskCreate([](void *ctx) {
                // 等待200ms确保HTTP响应完全发送
                vTaskDelay(pdMS_TO_TICKS(200));
                // 停止Web服务器
                auto* self = static_cast<WifiConfigurationAp*>(ctx);
                if (self->server_) {
                    httpd_stop(self->server_);
                }
                // 再等待100ms确保所有连接都已关闭
                vTaskDelay(pdMS_TO_TICKS(100));
                // 执行重启
                esp_restart();
            }, "reboot_task", 4096, this_, 5, NULL);
            
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &reboot));

    auto captive_portal_handler = [](httpd_req_t *req) -> esp_err_t {
        auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);
        std::string url = this_->GetWebServerUrl() + "/?lang=" + this_->language_;
        // Set content type to prevent browser warnings
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", url.c_str());
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    };

    // Register all common captive portal detection endpoints
    const char* captive_portal_urls[] = {
        "/hotspot-detect.html",    // Apple
        "/generate_204*",           // Android
        "/mobile/status.php",      // Android
        "/check_network_status.txt", // Windows
        "/ncsi.txt",              // Windows
        "/fwlink/",               // Microsoft
        "/connectivity-check.html", // Firefox
        "/success.txt",           // Various
        "/portal.html",           // Various
        "/library/test/success.html" // Apple
    };

    for (const auto& url : captive_portal_urls) {
        httpd_uri_t redirect_uri = {
            .uri = url,
            .method = HTTP_GET,
            .handler = captive_portal_handler,
            .user_ctx = this
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &redirect_uri));
    }

    // Register the /advanced/config URI
    httpd_uri_t advanced_config = {
        .uri = "/advanced/config",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            // 获取当前对象
            auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);
            
            // 创建JSON对象
            cJSON *json = cJSON_CreateObject();
            if (!json) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
                return ESP_FAIL;
            }

            // 添加配置项到JSON
            if (!this_->ota_url_.empty()) {
                cJSON_AddStringToObject(json, "ota_url", this_->ota_url_.c_str());
            }
            cJSON_AddNumberToObject(json, "max_tx_power", this_->max_tx_power_);
            cJSON_AddBoolToObject(json, "remember_bssid", this_->remember_bssid_);
            cJSON_AddBoolToObject(json, "sleep_mode", this_->sleep_mode_);

            // 发送JSON响应
            char *json_str = cJSON_PrintUnformatted(json);
            cJSON_Delete(json);
            if (!json_str) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to print JSON");
                return ESP_FAIL;
            }

            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, json_str, strlen(json_str));
            free(json_str);
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &advanced_config));

    // Register the /advanced/submit URI
    httpd_uri_t advanced_submit = {
        .uri = "/advanced/submit",
        .method = HTTP_POST,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            char *buf;
            size_t buf_len = req->content_len;
            if (buf_len > 1024) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
                return ESP_FAIL;
            }

            buf = (char *)malloc(buf_len + 1);
            if (!buf) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
                return ESP_FAIL;
            }

            int ret = httpd_req_recv(req, buf, buf_len);
            if (ret <= 0) {
                free(buf);
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                    httpd_resp_send_408(req);
                } else {
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request");
                }
                return ESP_FAIL;
            }
            buf[ret] = '\0';

            // 解析JSON数据
            cJSON *json = cJSON_Parse(buf);
            free(buf);
            if (!json) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
                return ESP_FAIL;
            }

            // 获取当前对象
            auto *this_ = static_cast<WifiConfigurationAp *>(req->user_ctx);

            // 打开NVS
            nvs_handle_t nvs;
            esp_err_t err = nvs_open("wifi", NVS_READWRITE, &nvs);
            if (err != ESP_OK) {
                cJSON_Delete(json);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open NVS");
                return ESP_FAIL;
            }

            // 保存OTA URL
            cJSON *ota_url = cJSON_GetObjectItem(json, "ota_url");
            if (cJSON_IsString(ota_url) && ota_url->valuestring) {
                this_->ota_url_ = ota_url->valuestring;
                err = nvs_set_str(nvs, "ota_url", this_->ota_url_.c_str());
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to save OTA URL: %d", err);
                }
            }

            // 保存WiFi功率
            cJSON *max_tx_power = cJSON_GetObjectItem(json, "max_tx_power");
            if (cJSON_IsNumber(max_tx_power)) {
                this_->max_tx_power_ = max_tx_power->valueint;
                err = esp_wifi_set_max_tx_power(this_->max_tx_power_);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to set WiFi power: %d", err);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set WiFi power");
                    return ESP_FAIL;
                }
                err = nvs_set_i8(nvs, "max_tx_power", this_->max_tx_power_);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to save WiFi power: %d", err);
                }
            }

            // 保存BSSID记忆设置
            cJSON *remember_bssid = cJSON_GetObjectItem(json, "remember_bssid");
            if (cJSON_IsBool(remember_bssid)) {
                this_->remember_bssid_ = cJSON_IsTrue(remember_bssid);
                err = nvs_set_u8(nvs, "remember_bssid", this_->remember_bssid_ ? 1 : 0);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to save remember_bssid: %d", err);
                }
            }

            // 保存睡眠模式设置
            cJSON *sleep_mode = cJSON_GetObjectItem(json, "sleep_mode");
            if (cJSON_IsBool(sleep_mode)) {
                this_->sleep_mode_ = cJSON_IsTrue(sleep_mode);
                err = nvs_set_u8(nvs, "sleep_mode", this_->sleep_mode_ ? 1 : 0);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to save sleep_mode: %d", err);
                }
            }

            // 提交更改
            err = nvs_commit(nvs);
            nvs_close(nvs);
            cJSON_Delete(json);

            if (err != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save configuration");
                return ESP_FAIL;
            }

            // 发送成功响应
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);

            ESP_LOGI(TAG, "Saved settings: ota_url=%s, max_tx_power=%d, remember_bssid=%d, sleep_mode=%d",
                this_->ota_url_.c_str(), this_->max_tx_power_, this_->remember_bssid_, this_->sleep_mode_);
            return ESP_OK;
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &advanced_submit));

    ESP_LOGI(TAG, "Web server started");
}

bool WifiConfigurationAp::ConnectToWifi(const std::string &ssid, const std::string &password)
{
    if (ssid.empty()) {
        ESP_LOGE(TAG, "SSID cannot be empty");
        return false;
    }
    
    if (ssid.length() > 32) {  // WiFi SSID 最大长度
        ESP_LOGE(TAG, "SSID too long");
        return false;
    }

    if (password.length() > 64) {
        ESP_LOGE(TAG, "Password too long");
        return false;
    }
    
    is_connecting_ = true;
    esp_wifi_scan_stop();
    xEventGroupClearBits(event_group_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config));
    strlcpy((char *)wifi_config.sta.ssid, ssid.c_str(), 32);
    strlcpy((char *)wifi_config.sta.password, password.c_str(), 64);
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.failure_retry_cnt = 1;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    auto ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi: %d", ret);
        is_connecting_ = false;
        return false;
    }
    ESP_LOGI(TAG, "Connecting to WiFi %s", ssid.c_str());

    // Wait for the connection to complete for 5 seconds
    EventBits_t bits = xEventGroupWaitBits(event_group_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    is_connecting_ = false;

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi %s", ssid.c_str());
        esp_wifi_disconnect();
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi %s", ssid.c_str());
        return false;
    }
}

void WifiConfigurationAp::Save(const std::string &ssid, const std::string &password)
{
    ESP_LOGI(TAG, "Save SSID %s %d", ssid.c_str(), ssid.length());
    SsidManager::GetInstance().AddSsid(ssid, password);
}

void WifiConfigurationAp::WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    WifiConfigurationAp* self = static_cast<WifiConfigurationAp*>(arg);
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(self->event_group_, WIFI_CONNECTED_BIT);
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(self->event_group_, WIFI_FAIL_BIT);
    } else if (event_id == WIFI_EVENT_SCAN_DONE) {
        std::lock_guard<std::mutex> lock(self->mutex_);
        uint16_t ap_num = 0;
        esp_wifi_scan_get_ap_num(&ap_num);

        self->ap_records_.resize(ap_num);
        esp_wifi_scan_get_ap_records(&ap_num, self->ap_records_.data());

        // 扫描完成，等待10秒后再次扫描
        esp_timer_start_once(self->scan_timer_, 10 * 1000000);
    }
}

void WifiConfigurationAp::IpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    WifiConfigurationAp* self = static_cast<WifiConfigurationAp*>(arg);
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(self->event_group_, WIFI_CONNECTED_BIT);
    }
}

void WifiConfigurationAp::StartSmartConfig()
{
    // 注册SmartConfig事件处理器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID,
                                                        &WifiConfigurationAp::SmartConfigEventHandler, this, &sc_event_instance_));

    // 初始化SmartConfig配置
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    // cfg.esp_touch_v2_enable_crypt = true;
    // cfg.esp_touch_v2_key = "1234567890123456"; // 设置16字节加密密钥

    // 启动SmartConfig服务
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    ESP_LOGI(TAG, "SmartConfig started");
}

void WifiConfigurationAp::SmartConfigEventHandler(void *arg, esp_event_base_t event_base,
                                                  int32_t event_id, void *event_data)
{
    WifiConfigurationAp *self = static_cast<WifiConfigurationAp *>(arg);

    if (event_base == SC_EVENT){
        switch (event_id){
        case SC_EVENT_SCAN_DONE:
            ESP_LOGI(TAG, "SmartConfig scan done");
            break;
        case SC_EVENT_FOUND_CHANNEL:
            ESP_LOGI(TAG, "Found SmartConfig channel");
            break;
        case SC_EVENT_GOT_SSID_PSWD:{
            ESP_LOGI(TAG, "Got SmartConfig credentials");
            smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;

            char ssid[32], password[64];
            memcpy(ssid, evt->ssid, sizeof(evt->ssid));
            memcpy(password, evt->password, sizeof(evt->password));
            ESP_LOGI(TAG, "SmartConfig SSID: %s, Password: %s", ssid, password);
            // 尝试连接WiFi会失败，故不连接
            self->Save(ssid, password);
            xTaskCreate([](void *ctx){
                ESP_LOGI(TAG, "Restarting in 3 second");
                vTaskDelay(pdMS_TO_TICKS(3000));
                esp_restart();
            }, "restart_task", 4096, NULL, 5, NULL);
            break;
        }
        case SC_EVENT_SEND_ACK_DONE:
            ESP_LOGI(TAG, "SmartConfig ACK sent");
            esp_smartconfig_stop();
            break;
        }
    }
}

void WifiConfigurationAp::Stop() {
    // 停止SmartConfig服务
    if (sc_event_instance_) {
        esp_event_handler_instance_unregister(SC_EVENT, ESP_EVENT_ANY_ID, sc_event_instance_);
        sc_event_instance_ = nullptr;
    }
    esp_smartconfig_stop();

    // 停止定时器
    if (scan_timer_) {
        esp_timer_stop(scan_timer_);
        esp_timer_delete(scan_timer_);
        scan_timer_ = nullptr;
    }

    // 停止Web服务器
    if (server_) {
        httpd_stop(server_);
        server_ = nullptr;
    }

    // 停止DNS服务器
    dns_server_.Stop();

    // 注销事件处理器
    if (instance_any_id_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id_);
        instance_any_id_ = nullptr;
    }
    if (instance_got_ip_) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip_);
        instance_got_ip_ = nullptr;
    }

    // 停止WiFi并重置模式
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_wifi_set_mode(WIFI_MODE_NULL);

    // 释放网络接口资源
    if (ap_netif_) {
        esp_netif_destroy(ap_netif_);
        ap_netif_ = nullptr;
    }

    ESP_LOGI(TAG, "Wifi configuration AP stopped");
}
