#include "WebSocketClient.h"
#include <cstring>
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"


#define TAG "WebSocket"
#define TIMEOUT_TICKS pdMS_TO_TICKS(3000)

WebSocketClient::WebSocketClient(bool auto_reconnect) {
    event_group_ = xEventGroupCreate();

    esp_websocket_client_config_t config = {};
    config.task_prio = 1;
    config.disable_auto_reconnect = !auto_reconnect;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    client_ = esp_websocket_client_init(&config);
    assert(client_ != NULL);

    esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, [](void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
        WebSocketClient* ws = (WebSocketClient*)arg;
        esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
        switch (event_id)
        {
        case WEBSOCKET_EVENT_BEFORE_CONNECT:
            break;
        case WEBSOCKET_EVENT_CONNECTED:
            if (ws->on_connected_) {
                ws->on_connected_();
            }
            xEventGroupSetBits(ws->event_group_, WEBSOCKET_CONNECTED_BIT);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            xEventGroupSetBits(ws->event_group_, WEBSOCKET_DISCONNECTED_BIT);
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->data_len != data->payload_len) {
                ESP_LOGE(TAG, "Payload segmentating is not supported, data_len: %d, payload_len: %d", data->data_len, data->payload_len);
                break;
            }
            if (data->op_code == 8) { // Websocket close
                ESP_LOGI(TAG, "Websocket closed");
                if (ws->on_closed_) {
                    ws->on_closed_();
                }
            } else if (data->op_code == 9) {
                // Websocket ping
            } else if (data->op_code == 10) {
                // Websocket pong
            } else if (data->op_code == 1) {
                // Websocket text
                if (ws->on_data_) {
                    ws->on_data_(data->data_ptr, data->data_len, false);
                }
            } else if (data->op_code == 2) {
                // Websocket binary
                if (ws->on_data_) {
                    ws->on_data_(data->data_ptr, data->data_len, true);
                }
            } else {
                ESP_LOGI(TAG, "Unknown opcode: %d", data->op_code);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            if (ws->on_error_) {
                ws->on_error_(data->error_handle.error_type);
            }
            xEventGroupSetBits(ws->event_group_, WEBSOCKET_ERROR_BIT);
            break;
        case WEBSOCKET_EVENT_CLOSED:
            break;
        default:
            ESP_LOGI(TAG, "Event %ld", event_id);
        }
    }, this);
}

WebSocketClient::~WebSocketClient() {
    esp_websocket_client_close(client_, TIMEOUT_TICKS);
    ESP_LOGI(TAG, "Destroying websocket client");
    esp_websocket_client_destroy(client_);
}

void WebSocketClient::SetHeader(const char* key, const char* value) {
    esp_websocket_client_append_header(client_, key, value);
}

bool WebSocketClient::Connect(const char* uri) {
    esp_websocket_client_set_uri(client_, uri);
    esp_websocket_client_start(client_);

    // Wait for the connection to be established or an error
    EventBits_t bits = xEventGroupWaitBits(event_group_, WEBSOCKET_CONNECTED_BIT | WEBSOCKET_ERROR_BIT, pdFALSE, pdFALSE, TIMEOUT_TICKS);
    return bits & WEBSOCKET_CONNECTED_BIT;
}

void WebSocketClient::Send(const void* data, size_t len, bool binary) {
    if (binary) {
        esp_websocket_client_send_bin(client_, (const char*)data, len, portMAX_DELAY);
    } else {
        esp_websocket_client_send_text(client_, (const char*)data, len, portMAX_DELAY);
    }
}

void WebSocketClient::Send(const std::string& data) {
    Send(data.c_str(), data.size(), false);
}

void WebSocketClient::OnClosed(std::function<void()> callback) {
    on_closed_ = callback;
}

void WebSocketClient::OnData(std::function<void(const char*, size_t, bool binary)> callback) {
    on_data_ = callback;
}

void WebSocketClient::OnError(std::function<void(int)> callback) {
    on_error_ = callback;
}

void WebSocketClient::OnConnected(std::function<void()> callback) {
    on_connected_ = callback;
}

void WebSocketClient::OnDisconnected(std::function<void()> callback) {
    on_disconnected_ = callback;
}

bool WebSocketClient::IsConnected() const {
    return esp_websocket_client_is_connected(client_);
}
