#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include "WifiStation.h"
#include "AudioDevice.h"
#include "OpusEncoder.h"
#include "WebSocketClient.h"
#include "BuiltinLed.h"

#include "opus.h"
#include "resampler_structs.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_afe_sr_models.h"
#include "esp_nsn_models.h"
#include <mutex>

#define DETECTION_RUNNING 1
#define COMMUNICATION_RUNNING 2


enum ChatState {
    kChatStateIdle,
    kChatStateConnecting,
    kChatStateListening,
    kChatStateSpeaking,
};

class Application {
public:
    Application();
    ~Application();
    void Start();

private:
    WifiStation wifi_station_;
    AudioDevice audio_device_;
    BuiltinLed builtin_led_;

    std::recursive_mutex mutex_;
    WebSocketClient* ws_client_ = nullptr;
    esp_afe_sr_data_t* afe_detection_data_ = nullptr;
    esp_afe_sr_data_t* afe_communication_data_ = nullptr;
    EventGroupHandle_t event_group_;
    char* wakenet_model_ = NULL;
    char* nsnet_model_ = NULL;
    volatile ChatState chat_state_ = kChatStateIdle;

    // Audio encode / decode
    TaskHandle_t audio_feed_task_ = nullptr;
    StaticTask_t audio_encode_task_buffer_;
    StackType_t* audio_encode_task_stack_ = nullptr;
    QueueHandle_t audio_encode_queue_ = nullptr;

    TaskHandle_t audio_decode_task_ = nullptr;
    StaticTask_t audio_decode_task_buffer_;
    StackType_t* audio_decode_task_stack_ = nullptr;
    QueueHandle_t audio_decode_queue_ = nullptr;

    OpusEncoder opus_encoder_;
    OpusDecoder* opus_decoder_ = nullptr;

    int opus_duration_ms_ = 60;
    silk_resampler_state_struct resampler_state_;

    void SetChatState(ChatState state);
    void StartDetection();
    void StartCommunication();
    void StartWebSocketClient();

    void AudioFeedTask();
    void AudioDetectionTask();
    void AudioCommunicationTask();
    void AudioEncodeTask();
    void AudioDecodeTask();
};

#endif // _APPLICATION_H_
