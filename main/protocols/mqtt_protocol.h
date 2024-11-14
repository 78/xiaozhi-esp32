#ifndef MQTT_PROTOCOL_H
#define MQTT_PROTOCOL_H


#include "protocol.h"
#include <mqtt.h>
#include <udp.h>
#include <cJSON.h>
#include <mbedtls/aes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <functional>
#include <string>
#include <map>
#include <mutex>
#define MQTT_PING_INTERVAL_SECONDS 90
#define MQTT_RECONNECT_INTERVAL_MS 10000

#define MQTT_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

class MqttProtocol : public Protocol {
public:
    MqttProtocol(std::map<std::string, std::string>& config);
    ~MqttProtocol();

    void OnIncomingAudio(std::function<void(const std::string& data)> callback);
    void OnIncomingJson(std::function<void(const cJSON* root)> callback);
    void SendAudio(const std::string& data);
    void SendText(const std::string& text);
    bool OpenAudioChannel();
    void CloseAudioChannel();
    void OnAudioChannelOpened(std::function<void()> callback);
    void OnAudioChannelClosed(std::function<void()> callback);

private:
    EventGroupHandle_t event_group_handle_;

    std::function<void(const cJSON* root)> on_incoming_json_;
    std::function<void(const std::string& data)> on_incoming_audio_;
    std::function<void()> on_audio_channel_opened_;
    std::function<void()> on_audio_channel_closed_;

    std::string endpoint_;
    std::string client_id_;
    std::string username_;
    std::string password_;
    std::string subscribe_topic_;
    std::string publish_topic_;

    std::mutex channel_mutex_;
    Mqtt* mqtt_ = nullptr;
    Udp* udp_ = nullptr;
    mbedtls_aes_context aes_ctx_;
    std::string aes_nonce_;
    std::string udp_server_;
    int udp_port_;
    uint32_t local_sequence_;
    uint32_t remote_sequence_;
    std::string session_id_;

    bool StartMqttClient();
    void ParseServerHello(const cJSON* root);
    std::string DecodeHexString(const std::string& hex_string);
};


#endif // MQTT_PROTOCOL_H
