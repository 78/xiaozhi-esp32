#ifndef _WEBSOCKET_PROTOCOL_H_
#define _WEBSOCKET_PROTOCOL_H_


#include "protocol.h"

#include <web_socket.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_timer.h>

#include <atomic>
#include <memory>
#include <mutex>

#define WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

// 心跳参数: 30s 一次 ping; 90s 没收到 pong 视为掉线
#define WEBSOCKET_PING_INTERVAL_MS    30000
#define WEBSOCKET_PONG_TIMEOUT_MS     90000
// 重连退避: 1s -> 60s 封顶, 每次失败乘 2
#define WEBSOCKET_RECONNECT_INITIAL_S 1
#define WEBSOCKET_RECONNECT_MAX_S     60

class WebsocketProtocol : public Protocol {
public:
    WebsocketProtocol();
    ~WebsocketProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;

private:
    // 防止 destructor 后 scheduled callback / timer callback 访问 dead this
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

    EventGroupHandle_t event_group_handle_;
    std::mutex websocket_mutex_;
    std::unique_ptr<WebSocket> websocket_;
    int version_ = 1;

    // audio session 是否激活 (vs WS 长连接是否在, 后者由 websocket_ != nullptr && IsConnected 判定)
    std::atomic<bool> audio_session_active_{false};

    // 心跳
    esp_timer_handle_t ping_timer_ = nullptr;
    std::atomic<int64_t> last_pong_time_us_{0};

    // 重连
    esp_timer_handle_t reconnect_timer_ = nullptr;
    int reconnect_backoff_s_ = WEBSOCKET_RECONNECT_INITIAL_S;

    bool ConnectAndHello();
    void StartPingTimer();
    void StopPingTimer();
    void SendPing();
    void ScheduleReconnect();
    void HandleDisconnected();

    void ParseServerHello(const cJSON* root);
    bool SendText(const std::string& text) override;
    std::string GetHelloMessage();
};

#endif
