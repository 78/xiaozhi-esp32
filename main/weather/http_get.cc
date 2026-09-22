#include "http_get.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>

#define TAG "HttpGet"

HttpResult HttpGet(const std::string& url, int timeout_ms) {
    HttpResult result;

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = timeout_ms;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize client for %s", url.c_str());
        return result;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Open failed for %s: %s", url.c_str(), esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return result;
    }

    esp_http_client_fetch_headers(client);
    result.status = esp_http_client_get_status_code(client);

    char buffer[1024];
    int read_len;
    while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        result.body.append(buffer, static_cast<size_t>(read_len));
    }

    ESP_LOGD(TAG, "GET %s -> %d (%d bytes)", url.c_str(), result.status,
             static_cast<int>(result.body.size()));
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}
