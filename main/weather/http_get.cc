#include "http_get.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>

#define TAG "HttpGet"

namespace {
constexpr size_t kMaxBodySize = 16 * 1024;

// Replace the value of any "key=" query parameter so API keys never hit logs.
std::string RedactUrl(const std::string& url) {
    size_t pos = 0;
    while (true) {
        pos = url.find("key=", pos);
        if (pos == std::string::npos) {
            return url;
        }
        // Only match a real query parameter: start of URL, or right after ?/&.
        if (pos == 0 || url[pos - 1] == '?' || url[pos - 1] == '&') {
            break;
        }
        pos += 4;
    }
    size_t end = pos + 4;
    while (end < url.size() && url[end] != '&') {
        ++end;
    }
    return url.substr(0, pos) + "key=***" + url.substr(end);
}
}  // namespace

HttpResult HttpGet(const std::string& url, int timeout_ms) {
    HttpResult result;
    std::string redacted = RedactUrl(url);

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = timeout_ms;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize client for %s", redacted.c_str());
        return result;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Open failed for %s: %s", redacted.c_str(), esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return result;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        // A negative result can simply mean a chunked Transfer-Encoding: when
        // a status code is available, headers are parsed and the read loop
        // below handles chunked bodies. Only fail when there is no status.
        if (esp_http_client_get_status_code(client) <= 0) {
            ESP_LOGW(TAG, "Fetch headers failed for %s", redacted.c_str());
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return result;
        }
    }
    result.status = esp_http_client_get_status_code(client);

    char buffer[1024];
    int read_len;
    while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        if (result.body.size() + static_cast<size_t>(read_len) > kMaxBodySize) {
            ESP_LOGW(TAG, "Response body exceeds %d bytes for %s",
                     static_cast<int>(kMaxBodySize), redacted.c_str());
            result = HttpResult{};  // Oversized/truncated body -> transport failure
            break;
        }
        result.body.append(buffer, static_cast<size_t>(read_len));
    }
    if (read_len < 0) {
        ESP_LOGW(TAG, "Read error for %s", redacted.c_str());
        result = HttpResult{};
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (result.status != 0) {
        ESP_LOGD(TAG, "GET %s -> %d (%d bytes)", redacted.c_str(), result.status,
                 static_cast<int>(result.body.size()));
    }
    return result;
}
