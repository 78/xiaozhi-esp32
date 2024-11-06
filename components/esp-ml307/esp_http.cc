#include "esp_http.h"
#include <esp_tls.h>
#include <esp_log.h>
#include <esp_crt_bundle.h>
#include <cstring>

static const char* TAG = "EspHttp";

EspHttp::EspHttp() : client_(nullptr), status_code_(0) {}

EspHttp::~EspHttp() {
    Close();
}

void EspHttp::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void EspHttp::SetContent(const std::string& content) {
    content_ = content;
}

bool EspHttp::Open(const std::string& method, const std::string& url) {
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;

    ESP_LOGI(TAG, "Opening HTTP connection to %s", url.c_str());

    client_ = esp_http_client_init(&config);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return false;
    }

    esp_http_client_set_method(client_, 
        method == "GET" ? HTTP_METHOD_GET : 
        method == "POST" ? HTTP_METHOD_POST : 
        method == "PUT" ? HTTP_METHOD_PUT : 
        method == "DELETE" ? HTTP_METHOD_DELETE : HTTP_METHOD_GET);

    for (const auto& header : headers_) {
        esp_http_client_set_header(client_, header.first.c_str(), header.second.c_str());
    }

    esp_err_t err = esp_http_client_open(client_, content_.length());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to perform HTTP request: %s", esp_err_to_name(err));
        Close();
        return false;
    }

    auto written = esp_http_client_write(client_, content_.data(), content_.length());
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write request body: %s", esp_err_to_name(err));
        Close();
        return false;
    }
    content_length_ = esp_http_client_fetch_headers(client_);
    if (content_length_ <= 0) {
        ESP_LOGE(TAG, "Failed to fetch headers");
        Close();
        return false;
    }
    return true;
}

void EspHttp::Close() {
    if (client_) {
        esp_http_client_cleanup(client_);
        client_ = nullptr;
    }
}

int EspHttp::GetStatusCode() const {
    return status_code_;
}

std::string EspHttp::GetResponseHeader(const std::string& key) const {
    if (!client_) return "";
    char* value = nullptr;
    esp_http_client_get_header(client_, key.c_str(), &value);
    if (!value) return "";
    std::string result(value);
    free(value);
    return result;
}

size_t EspHttp::GetBodyLength() const {
    return content_length_;
}

const std::string& EspHttp::GetBody() {
    response_body_.resize(content_length_);
    assert(Read(const_cast<char*>(response_body_.data()), content_length_) == content_length_);
    return response_body_;
}

int EspHttp::Read(char* buffer, size_t buffer_size) {
    if (!client_) return -1;
    return esp_http_client_read(client_, buffer, buffer_size);
}

esp_err_t EspHttp::HttpEventHandler(esp_http_client_event_t *evt) {
    EspHttp* http = static_cast<EspHttp*>(evt->user_data);
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0) {
                http->response_body_.append((char*)evt->data, evt->data_len);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}
