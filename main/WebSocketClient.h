#ifndef _WEBSOCKET_CLIENT_H_
#define _WEBSOCKET_CLIENT_H_

#include <functional>
#include <string>
#include "esp_websocket_client.h"
#include "freertos/event_groups.h"

#define WEBSOCKET_CONNECTED_BIT BIT0
#define WEBSOCKET_DISCONNECTED_BIT BIT1
#define WEBSOCKET_ERROR_BIT BIT2

class WebSocketClient {
public:
    WebSocketClient(bool auto_reconnect = false);
    ~WebSocketClient();

    void SetHeader(const char* key, const char* value);
    bool IsConnected() const;
    bool Connect(const char* uri);
    void Send(const std::string& data);
    void Send(const void* data, size_t len, bool binary = false);

    void OnConnected(std::function<void()> callback);
    void OnDisconnected(std::function<void()> callback);
    void OnData(std::function<void(const char*, size_t, bool binary)> callback);
    void OnError(std::function<void(int)> callback);
    void OnClosed(std::function<void()> callback);

private:
    esp_websocket_client_handle_t client_ = NULL;
    EventGroupHandle_t event_group_;
    std::function<void(const char*, size_t, bool binary)> on_data_;
    std::function<void(int)> on_error_;
    std::function<void()> on_closed_;
    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;
};

#endif // _WEBSOCKET_CLIENT_H_
