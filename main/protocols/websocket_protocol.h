#ifndef _WEBSOCKET_PROTOCOL_H_
#define _WEBSOCKET_PROTOCOL_H_


#include "protocol.h"

#include <web_socket.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_timer.h>

#define WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

#define PING_INTERVAL_SECONDS 30
#define RECONNECT_MAX_DELAY_SECONDS 60
#define RECONNECT_INITIAL_DELAY_SECONDS 1
#define RECONNECT_BACKOFF_MULTIPLIER 2

class WebsocketProtocol : public Protocol {
public:
    WebsocketProtocol();
    ~WebsocketProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;

    bool ConnectControlChannel();
    void DisconnectControlChannel();
    bool IsPersistent() const { return persistent_mode_; }

private:
    EventGroupHandle_t event_group_handle_;
    std::unique_ptr<WebSocket> websocket_;
    int version_ = 1;

    bool persistent_mode_ = false;
    bool audio_channel_open_ = false;
    bool should_reconnect_ = false;
    int reconnect_delay_seconds_ = RECONNECT_INITIAL_DELAY_SECONDS;
    esp_timer_handle_t ping_timer_ = nullptr;
    esp_timer_handle_t reconnect_timer_ = nullptr;

    void ParseServerHello(const cJSON* root);
    bool SendText(const std::string& text) override;
    std::string GetHelloMessage();
    void SetupCallbacks();

    void ScheduleReconnect();
    void CancelReconnect();
    void StartPingTimer();
    void StopPingTimer();
};

#endif
