#ifndef _AGORA_RTC_PROTOCOL_H_
#define _AGORA_RTC_PROTOCOL_H_

#include "protocol.h"

#include <string>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include "agora_rtc_api.h"

#define AGORA_JOINED_EVENT   (1 << 0)
#define AGORA_RTM_LOGIN_EVENT (1 << 1)

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
    std::atomic<bool> rtm_logged_in_{false};
    std::atomic<bool> sdk_initialized_{false};
    std::string remote_rtm_uid_;

    bool SendText(const std::string& text) override;
    bool InitSdk();
    void FiniSdk();

    // RTC static callbacks
    static void OnJoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms);
    static void OnError(connection_id_t conn_id, int code, const char* msg);
    static void OnUserJoined(connection_id_t conn_id, uint32_t uid, int elapsed_ms);
    static void OnUserOffline(connection_id_t conn_id, uint32_t uid, int reason);
    static void OnUserJoinedWithUserAccount(connection_id_t conn_id, const user_info_t* user, int elapsed_ms);
    static void OnUserOfflineWithUserAccount(connection_id_t conn_id, const user_info_t* user, int reason);
    static void OnAudioData(connection_id_t conn_id, uint32_t uid, uint16_t sent_ts,
                           const void* data_ptr, size_t data_len, const audio_frame_info_t* info_ptr);
    static void OnConnectionLost(connection_id_t conn_id);
    static void OnReconnecting(connection_id_t conn_id);
    static void OnRejoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms);

    // RTM static callbacks
    static void OnRtmEvent(const char* rtm_uid, rtm_event_type_e event_type, rtm_err_code_e err_code);
    static void OnRtmData(const char* rtm_uid, const void* msg, size_t msg_len, const char* custom_type);
    static void OnRtmSendDataResult(const char* rtm_uid, uint32_t msg_id, rtm_msg_state_e state);
};

#endif // _AGORA_RTC_PROTOCOL_H_
