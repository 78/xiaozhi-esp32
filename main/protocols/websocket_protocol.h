#ifndef _WEBSOCKET_PROTOCOL_H_
#define _WEBSOCKET_PROTOCOL_H_


#include "protocol.h"

#include <web_socket.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string>

#define WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

class WebsocketProtocol : public Protocol {
public:
    WebsocketProtocol();
    ~WebsocketProtocol();

    void Start() override;
    void SendAudio(const std::vector<uint8_t>& data) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel() override;
    bool IsAudioChannelOpened() const override;
private:
    EventGroupHandle_t event_group_handle_;
    WebSocket* websocket_ = nullptr;

    void ParseServerHello(const cJSON* root);
    bool SendText(const std::string& text) override;

    // 生成随机值的辅助方法
    std::string GenerateRandomUUID();
    std::string GenerateRandomMacAddress();
};

#endif
