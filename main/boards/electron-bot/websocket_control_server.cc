#include "websocket_control_server.h"

#include "mcp_server.h"

#include <cJSON.h>
#include <esp_log.h>

#include <cstdlib>
#include <cstring>

static const char* TAG = "ElectronBotWS";

WebSocketControlServer* WebSocketControlServer::instance_ = nullptr;

WebSocketControlServer::WebSocketControlServer() : server_handle_(nullptr) {
    instance_ = this;
}

WebSocketControlServer::~WebSocketControlServer() {
    Stop();
    instance_ = nullptr;
}

esp_err_t WebSocketControlServer::WsHandler(httpd_req_t* req) {
    if (instance_ == nullptr) {
        return ESP_FAIL;
    }

    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake completed");
        instance_->AddClient(req);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = {};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WebSocket frame length: %d", ret);
        return ret;
    }

    uint8_t* buf = nullptr;
    if (ws_pkt.len > 0) {
        buf = static_cast<uint8_t*>(calloc(1, ws_pkt.len + 1));
        if (buf == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate WebSocket frame buffer");
            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read WebSocket frame payload: %d", ret);
            free(buf);
            return ret;
        }
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "WebSocket close frame received");
        instance_->RemoveClient(req);
        free(buf);
        return ESP_OK;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        if (ws_pkt.len > 0 && buf != nullptr) {
            buf[ws_pkt.len] = '\0';
            instance_->HandleMessage(req, reinterpret_cast<const char*>(buf), ws_pkt.len);
        }
    } else {
        ESP_LOGW(TAG, "Unsupported WebSocket frame type: %d", ws_pkt.type);
    }

    free(buf);
    return ESP_OK;
}

bool WebSocketControlServer::Start(int port) {
    if (server_handle_ != nullptr) {
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_open_sockets = 7;
    config.ctrl_port = 32769;

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = WsHandler,
        .user_ctx = nullptr,
        .is_websocket = true,
    };

    if (httpd_start(&server_handle_, &config) == ESP_OK) {
        httpd_register_uri_handler(server_handle_, &ws_uri);
        ESP_LOGI(TAG, "WebSocket control server started on port %d", port);
        return true;
    }

    ESP_LOGE(TAG, "Failed to start WebSocket control server");
    return false;
}

void WebSocketControlServer::Stop() {
    if (server_handle_ != nullptr) {
        httpd_stop(server_handle_);
        server_handle_ = nullptr;
        clients_.clear();
        ESP_LOGI(TAG, "WebSocket control server stopped");
    }
}

void WebSocketControlServer::HandleMessage(httpd_req_t* req, const char* data, size_t len) {
    (void)req;

    if (data == nullptr || len == 0) {
        ESP_LOGE(TAG, "Invalid empty WebSocket message");
        return;
    }

    if (len > 4096) {
        ESP_LOGE(TAG, "WebSocket message too long: %zu bytes", len);
        return;
    }

    char* temp_buf = static_cast<char*>(malloc(len + 1));
    if (temp_buf == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate JSON parse buffer");
        return;
    }

    memcpy(temp_buf, data, len);
    temp_buf[len] = '\0';

    cJSON* root = cJSON_Parse(temp_buf);
    free(temp_buf);

    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse WebSocket JSON message");
        return;
    }

    cJSON* payload = nullptr;
    cJSON* type = cJSON_GetObjectItem(root, "type");

    if (type != nullptr && cJSON_IsString(type) && strcmp(type->valuestring, "mcp") == 0) {
        payload = cJSON_GetObjectItem(root, "payload");
        if (payload != nullptr) {
            cJSON_DetachItemViaPointer(root, payload);
            McpServer::GetInstance().ParseMessage(payload);
            cJSON_Delete(payload);
        }
    } else {
        payload = cJSON_Duplicate(root, 1);
        if (payload != nullptr) {
            McpServer::GetInstance().ParseMessage(payload);
            cJSON_Delete(payload);
        }
    }

    if (payload == nullptr) {
        ESP_LOGE(TAG, "Invalid WebSocket message format");
    }

    cJSON_Delete(root);
}

void WebSocketControlServer::AddClient(httpd_req_t* req) {
    int sock_fd = httpd_req_to_sockfd(req);
    if (clients_.find(sock_fd) == clients_.end()) {
        clients_[sock_fd] = req;
        ESP_LOGI(TAG, "WebSocket client connected: %d (total: %zu)", sock_fd, clients_.size());
    }
}

void WebSocketControlServer::RemoveClient(httpd_req_t* req) {
    int sock_fd = httpd_req_to_sockfd(req);
    clients_.erase(sock_fd);
    ESP_LOGI(TAG, "WebSocket client disconnected: %d (total: %zu)", sock_fd, clients_.size());
}

size_t WebSocketControlServer::GetClientCount() const {
    return clients_.size();
}

struct WsBroadcastJob {
    httpd_handle_t server;
    int fd;
    char* payload;
    size_t len;
};

static void WsBroadcastSendJob(void* arg) {
    WsBroadcastJob* job = static_cast<WsBroadcastJob*>(arg);

    httpd_ws_frame_t ws_pkt = {};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = reinterpret_cast<uint8_t*>(job->payload);
    ws_pkt.len = job->len;
    ws_pkt.final = true;

    esp_err_t ret = httpd_ws_send_frame_async(job->server, job->fd, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to broadcast WebSocket message fd=%d err=%d", job->fd, ret);
    }

    free(job->payload);
    free(job);
}

void WebSocketControlServer::BroadcastMessage(const std::string& message) {
    if (server_handle_ == nullptr || clients_.empty()) {
        return;
    }

    for (auto& [fd, req] : clients_) {
        (void)req;

        WsBroadcastJob* job = static_cast<WsBroadcastJob*>(malloc(sizeof(WsBroadcastJob)));
        if (job == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate WebSocket broadcast job");
            continue;
        }

        job->server = server_handle_;
        job->fd = fd;
        job->len = message.length();
        job->payload = static_cast<char*>(malloc(message.length() + 1));
        if (job->payload == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate WebSocket broadcast payload");
            free(job);
            continue;
        }

        memcpy(job->payload, message.c_str(), message.length());
        job->payload[message.length()] = '\0';

        esp_err_t ret = httpd_queue_work(server_handle_, WsBroadcastSendJob, job);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to queue WebSocket broadcast fd=%d err=%d", fd, ret);
            free(job->payload);
            free(job);
        }
    }
}
