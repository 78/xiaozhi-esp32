#include "ml307_http.h"
#include <esp_log.h>
#include <cstring>
#include <sstream>
#include <chrono>

static const char *TAG = "Ml307Http";

Ml307Http::Ml307Http(Ml307AtModem& modem) : modem_(modem) {
    event_group_handle_ = xEventGroupCreate();

    command_callback_it_ = modem_.RegisterCommandResponseCallback([this](const std::string& command, const std::vector<AtArgumentValue>& arguments) {
        if (command == "MHTTPURC") {
            if (arguments[1].int_value == http_id_) {
                auto& type = arguments[0].string_value;
                if (type == "header") {
                    body_.clear();
                    status_code_ = arguments[2].int_value;
                    ParseResponseHeaders(modem_.DecodeHex(arguments[4].string_value));
                    xEventGroupSetBits(event_group_handle_, ML307_HTTP_EVENT_HEADERS_RECEIVED);
                } else if (type == "content") {
                    // +MHTTPURC: "content",<httpid>,<content_len>,<sum_len>,<cur_len>,<data>
                    std::string decoded_data;
                    modem_.DecodeHexAppend(decoded_data, arguments[5].string_value.c_str(), arguments[5].string_value.length());

                    std::lock_guard<std::mutex> lock(mutex_);
                    body_.append(decoded_data);
                    if (arguments[3].int_value >= arguments[2].int_value) {
                        eof_ = true;
                    }
                    body_offset_ += arguments[4].int_value;
                    if (arguments[3].int_value > body_offset_) {
                        ESP_LOGE(TAG, "body_offset_: %zu, arguments[3].int_value: %d", body_offset_, arguments[3].int_value);
                        Close();
                        return;
                    }
                    cv_.notify_one();  // 使用条件变量通知
                } else if (type == "err") {
                    error_code_ = arguments[2].int_value;
                    xEventGroupSetBits(event_group_handle_, ML307_HTTP_EVENT_ERROR);
                }
            }
        } else if (command == "MHTTPCREATE") {
            http_id_ = arguments[0].int_value;
            xEventGroupSetBits(event_group_handle_, ML307_HTTP_EVENT_INITIALIZED);
        } else if (command == "FIFO_OVERFLOW") {
            xEventGroupSetBits(event_group_handle_, ML307_HTTP_EVENT_ERROR);
            Close();
        }
    });
}

int Ml307Http::Read(char* buffer, size_t buffer_size) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (eof_ && body_.empty()) {
        return 0;
    }
    
    // 使用条件变量等待数据
    auto timeout = std::chrono::milliseconds(HTTP_CONNECT_TIMEOUT_MS);
    bool received = cv_.wait_for(lock, timeout, [this] { 
        return !body_.empty() || eof_; 
    });
    
    if (!received) {
        ESP_LOGE(TAG, "等待HTTP内容接收超时");
        return -1;
    }
    
    size_t bytes_to_read = std::min(body_.size(), buffer_size);
    std::memcpy(buffer, body_.data(), bytes_to_read);
    body_.erase(0, bytes_to_read);
    
    return bytes_to_read;
}

Ml307Http::~Ml307Http() {
    if (connected_) {
        Close();
    }
    modem_.UnregisterCommandResponseCallback(command_callback_it_);
    vEventGroupDelete(event_group_handle_);
}

void Ml307Http::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void Ml307Http::SetContent(const std::string& content) {
    content_ = content;
}

void Ml307Http::ParseResponseHeaders(const std::string& headers) {
    std::istringstream iss(headers);
    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream line_iss(line);
        std::string key, value;
        std::getline(line_iss, key, ':');
        std::getline(line_iss, value);
        response_headers_[key] = value;
    }
}

bool Ml307Http::Open(const std::string& method, const std::string& url) {
    method_ = method;
    url_ = url;
    // 解析URL
    size_t protocol_end = url.find("://");
    if (protocol_end != std::string::npos) {
        protocol_ = url.substr(0, protocol_end);
        size_t host_start = protocol_end + 3;
        size_t path_start = url.find("/", host_start);
        if (path_start != std::string::npos) {
            host_ = url.substr(host_start, path_start - host_start);
            path_ = url.substr(path_start);
        } else {
            host_ = url.substr(host_start);
            path_ = "/";
        }
    } else {
        // URL格式不正确
        ESP_LOGE(TAG, "无效的URL格式");
        return false;
    }

    // 创建HTTP连接
    char command[256];
    sprintf(command, "AT+MHTTPCREATE=\"%s://%s\"", protocol_.c_str(), host_.c_str());
    if (!modem_.Command(command)) {
        ESP_LOGE(TAG, "创建HTTP连接失败");
        return false;
    }

    auto bits = xEventGroupWaitBits(event_group_handle_, ML307_HTTP_EVENT_INITIALIZED, pdTRUE, pdFALSE, pdMS_TO_TICKS(HTTP_CONNECT_TIMEOUT_MS));
    if (!(bits & ML307_HTTP_EVENT_INITIALIZED)) {
        ESP_LOGE(TAG, "等待HTTP连接创建超时");
        return false;
    }
    connected_ = true;
    ESP_LOGI(TAG, "HTTP 连接已创建，ID: %d", http_id_);

    if (protocol_ == "https") {
        sprintf(command, "AT+MHTTPCFG=\"ssl\",%d,1,0", http_id_);
        modem_.Command(command);
    }

    // Set HEX encoding OFF
    sprintf(command, "AT+MHTTPCFG=\"encoding\",%d,0,0", http_id_);
    modem_.Command(command);

    // Flow control to 1024 bytes per 100ms
    sprintf(command, "AT+MHTTPCFG=\"fragment\",%d,1024,100", http_id_);
    modem_.Command(command);

    // Set headers
    for (const auto& header : headers_) {
        auto line = header.first + ": " + header.second;
        sprintf(command, "AT+MHTTPCFG=\"header\",%d,%s", http_id_, line.c_str());
        modem_.Command(command);
    }

    if (!content_.empty() && method_ == "POST") {
        sprintf(command, "AT+MHTTPCONTENT=%d,0,%zu", http_id_, content_.size());
        modem_.Command(command);
        modem_.Command(content_);
    }

    // Set HEX encoding ON
    sprintf(command, "AT+MHTTPCFG=\"encoding\",%d,1,1", http_id_);
    modem_.Command(command);

    // Send request
    // method to value: 1. GET 2. POST 3. PUT 4. DELETE 5. HEAD
    const char* methods[6] = {"UNKNOWN", "GET", "POST", "PUT", "DELETE", "HEAD"};
    int method_value = 1;
    for (int i = 0; i < 6; i++) {
        if (strcmp(methods[i], method_.c_str()) == 0) {
            method_value = i;
            break;
        }
    }
    sprintf(command, "AT+MHTTPREQUEST=%d,%d,0,", http_id_, method_value);
    modem_.Command(std::string(command) + modem_.EncodeHex(path_));

    // Wait for headers
    bits = xEventGroupWaitBits(event_group_handle_, ML307_HTTP_EVENT_HEADERS_RECEIVED | ML307_HTTP_EVENT_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(HTTP_CONNECT_TIMEOUT_MS));
    if (bits & ML307_HTTP_EVENT_ERROR) {
        ESP_LOGE(TAG, "HTTP请求错误: %s", ErrorCodeToString(error_code_).c_str());
        return false;
    }
    if (!(bits & ML307_HTTP_EVENT_HEADERS_RECEIVED)) {
        ESP_LOGE(TAG, "等待HTTP头部接收超时");
        return false;
    }

    if (status_code_ >= 400) {
        ESP_LOGE(TAG, "HTTP请求失败，状态码: %d", status_code_);
        return false;
    }

    auto it = response_headers_.find("Content-Length");
    if (it != response_headers_.end()) {
        content_length_ = std::stoul(it->second);
    }
    eof_ = false;
    body_offset_ = 0;
    ESP_LOGI(TAG, "HTTP请求成功，状态码: %d", status_code_);
    return true;
}

size_t Ml307Http::GetBodyLength() const {
    return content_length_;
}

const std::string& Ml307Http::GetBody() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    auto timeout = std::chrono::milliseconds(HTTP_CONNECT_TIMEOUT_MS);
    bool received = cv_.wait_for(lock, timeout, [this] { 
        return eof_; 
    });
    
    if (!received) {
        ESP_LOGE(TAG, "等待HTTP内容接收完成超时");
        return body_;
    }
    
    return body_;
}

void Ml307Http::Close() {
    if (!connected_) {
        return;
    }
    char command[32];
    sprintf(command, "AT+MHTTPDEL=%d", http_id_);
    modem_.Command(command);

    connected_ = false;
    eof_ = true;
    cv_.notify_one();
    ESP_LOGI(TAG, "HTTP连接已关闭，ID: %d", http_id_);
}

std::string Ml307Http::ErrorCodeToString(int error_code) {
    switch (error_code) {
        case 1: return "域名解析失败";
        case 2: return "连接服务器失败";
        case 3: return "连接服务器超时";
        case 4: return "SSL握手失败";
        case 5: return "连接异常断开";
        case 6: return "请求响应超时";
        case 7: return "接收数据解析失败";
        case 8: return "缓存空间不足";
        case 9: return "数据丢包";
        case 10: return "写文件失败";
        case 255: return "未知错误";
        default: return "未定义错误";
    }
}

std::string Ml307Http::GetResponseHeader(const std::string& key) const {
    auto it = response_headers_.find(key);
    if (it != response_headers_.end()) {
        return it->second;
    }
    return "";
}