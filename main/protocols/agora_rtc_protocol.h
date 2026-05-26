#ifndef _AGORA_RTC_PROTOCOL_H_
#define _AGORA_RTC_PROTOCOL_H_

#include "protocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <atomic>
#include <string>

extern "C" {
#include "agora_rtc_api.h"
}

#define AGORA_PROTOCOL_JOINED_EVENT (1 << 0)

class AgoraRtcProtocol : public Protocol {
public:
    AgoraRtcProtocol();
    ~AgoraRtcProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;

private:
    EventGroupHandle_t event_group_handle_;
    connection_id_t conn_id_ = CONNECTION_ID_INVALID;
    std::atomic<bool> joined_{false};
    std::atomic<bool> channel_opened_{false};

    std::string app_id_;
    std::string channel_name_;
    std::string token_;
    uint32_t uid_ = 0;

    bool SendText(const std::string& text) override;

    // Agora SDK callbacks
    static void OnJoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms);
    static void OnConnectionLost(connection_id_t conn_id);
    static void OnRejoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms);
    static void OnError(connection_id_t conn_id, int code, const char* msg);
    static void OnAudioData(connection_id_t conn_id, uint32_t uid, uint16_t sent_ts,
                            const void* data, size_t len, const audio_frame_info_t* info);
    static void OnUserJoined(connection_id_t conn_id, uint32_t uid, int elapsed_ms);
    static void OnUserOffline(connection_id_t conn_id, uint32_t uid, int reason);
    static void OnStreamMessage(connection_id_t conn_id, uint32_t uid, int stream_id,
                                const char* data, size_t length, uint64_t sent_ts);
};

#endif // _AGORA_RTC_PROTOCOL_H_
