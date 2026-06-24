#ifndef _AGORA_RTC_PROTOCOL_H_
#define _AGORA_RTC_PROTOCOL_H_

#include "protocol.h"
#include "device_api_client.h"
#include "audio/lock_free_ring_buffer.h"

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "agora_rtc_api.h"

#define AGORA_JOINED_EVENT    (1 << 0)
#define AGORA_RTM_LOGIN_EVENT (1 << 1)
#define AGORA_AI_QOS          (true) // Enable AI QoS feature (configurable at compile time)

class AgoraRtcProtocol : public Protocol {
public:
    AgoraRtcProtocol();
    ~AgoraRtcProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;

    // Device API client accessor (for activation flow)
    DeviceApiClient& GetDeviceApiClient() { return device_api_; }

    // Run the pairing flow (blocking). Returns true when device is bound.
    bool RunPairingFlow();

    // Check if device is already bound (has device_token)
    bool IsDeviceBound() const { return device_api_.HasDeviceToken(); }

private:
    EventGroupHandle_t event_group_handle_;
    connection_id_t conn_id_ = CONNECTION_ID_INVALID;
    std::atomic<bool> joined_{false};
    std::atomic<bool> rtm_logged_in_{false};
    std::atomic<bool> sdk_initialized_{false};
    std::string remote_rtm_uid_;
    uint32_t rtm_msg_id_ = 0;

    // Device API and conversation state
    DeviceApiClient device_api_;
    ConversationInfo current_conversation_;

    // Downlink AEC reference ring buffer (lock-free SPSC, PSRAM-allocated)
    static constexpr size_t kRefBufferMaxSamples = 16000; // 1 second @ 16kHz
    std::unique_ptr<LockFreeRingBuffer> ref_ring_buffer_;

    // Downlink PCM ring buffer: RTC callback → ringbuf → downlink task → AudioService
    // 512KB = 262144 int16_t samples ≈ 273 frames (60ms@16kHz) ≈ 16 seconds
    static constexpr size_t kDownlinkBufferSamples = 512 * 1024 / sizeof(int16_t); // 262144
    static constexpr size_t kDownlinkFrameSamples = 960; // 60ms @ 16kHz
    std::unique_ptr<LockFreeRingBuffer> downlink_ring_buffer_;
    TaskHandle_t downlink_task_handle_ = nullptr;
    std::atomic<bool> downlink_task_running_{false};
    std::atomic<bool> downlink_clear_requested_{false};
    SemaphoreHandle_t downlink_exit_sem_ = nullptr;

    static void DownlinkTask(void* arg);
    void DownlinkTaskLoop();

    bool SendText(const std::string& text) override;
    bool InitSdk(const std::string& app_id);
    void FiniSdk();

    // RTC static callbacks
    static void OnJoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms);
    static void OnError(connection_id_t conn_id, int code, const char* msg);
    static void OnUserJoinedWithUserAccount(connection_id_t conn_id, const user_info_t* user, int elapsed_ms);
    static void OnUserOfflineWithUserAccount(connection_id_t conn_id, const user_info_t* user, int reason);
    static void OnAudioData(connection_id_t conn_id, uint32_t uid, uint16_t sent_ts,
                            const void* data_ptr, size_t data_len, const audio_frame_info_t* info_ptr);
    static void OnUserMuteAudio(connection_id_t conn_id, uint32_t uid, bool muted);
    static void OnConnectionLost(connection_id_t conn_id);
    static void OnReconnecting(connection_id_t conn_id);
    static void OnRejoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms);

    // RTM static callbacks
    static void OnRtmEvent(const char* rtm_uid, rtm_event_type_e event_type, rtm_err_code_e err_code);
    static void OnRtmData(const char* rtm_uid, const void* msg, size_t msg_len, const char* custom_type);
    static void OnRtmSendDataResult(const char* rtm_uid, uint32_t msg_id, rtm_msg_state_e state);
};

#endif // _AGORA_RTC_PROTOCOL_H_
