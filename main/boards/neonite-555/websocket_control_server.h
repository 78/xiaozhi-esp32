#ifndef WEBSOCKET_CONTROL_SERVER_H
#define WEBSOCKET_CONTROL_SERVER_H

#include <esp_http_server.h>
#include <cJSON.h>
#include <string>
#include <map>
#include <functional>

class WebSocketControlServer {
public:
    WebSocketControlServer();
    ~WebSocketControlServer();

    bool Start(int port = 8080);
    
    void Stop();

    size_t GetClientCount() const;

    /** 设置首个 WebSocket 客户端连接时的回调 */
    void SetOnClientConnectedCallback(std::function<void()> cb) { on_connected_cb_ = cb; }

    /** 设置最后一个 WebSocket 客户端断开时的回调 */
    void SetOnAllClientsDisconnectedCallback(std::function<void()> cb) { on_disconnected_cb_ = cb; }

private:
    httpd_handle_t server_handle_;
    std::map<int, httpd_req_t*> clients_;

    std::function<void()> on_connected_cb_;
    std::function<void()> on_disconnected_cb_;

    static esp_err_t ws_handler(httpd_req_t *req);
    
    void HandleMessage(httpd_req_t *req, const char* data, size_t len);
    void AddClient(httpd_req_t *req);
    void RemoveClient(httpd_req_t *req);
    static WebSocketControlServer* instance_;
};

#endif // WEBSOCKET_CONTROL_SERVER_H

