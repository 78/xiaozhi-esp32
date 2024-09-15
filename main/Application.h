#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include "AudioDevice.h"
#include "OpusEncoder.h"
#include "OpusResampler.h"
#include "WebSocketClient.h"
#include "FirmwareUpgrade.h"

#include "opus.h"
#include "resampler_structs.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_afe_sr_models.h"
#include "esp_nsn_models.h"
#include <mutex>
#include <list>

#define DETECTION_RUNNING 1
#define COMMUNICATION_RUNNING 2
#define DETECT_PACKETS_ENCODED 4


enum ChatState {
    kChatStateIdle,
    kChatStateConnecting,
    kChatStateListening,
    kChatStateSpeaking,
    kChatStateWakeWordDetected,
    kChatStateTesting,
    kChatStateUpgrading
};

class Application {
public:
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }

    void Start();

    // 删除拷贝构造函数和赋值运算符
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

private:
    Application();
    ~Application();

    AudioDevice audio_device_;
    FirmwareUpgrade firmware_upgrade_;

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
    int opus_decode_sample_rate_ = CONFIG_AUDIO_OUTPUT_SAMPLE_RATE;
    OpusResampler opus_resampler_;
    OpusResampler test_resampler_;
    std::vector<iovec> test_pcm_;

    TaskHandle_t wake_word_encode_task_ = nullptr;
    StaticTask_t wake_word_encode_task_buffer_;
    StackType_t* wake_word_encode_task_stack_ = nullptr;
    std::list<iovec> wake_word_pcm_;
    std::vector<iovec> wake_word_opus_;

    void SetDecodeSampleRate(int sample_rate);
    void SetChatState(ChatState state);
    void StartDetection();
    void StartCommunication();
    void StartWebSocketClient();
    void StoreWakeWordData(uint8_t* data, size_t size);
    void EncodeWakeWordData();
    void SendWakeWordData();
    void CheckTestButton();
    void PlayTestAudio();
    void CheckNewVersion();
    
    void AudioFeedTask();
    void AudioDetectionTask();
    void AudioCommunicationTask();
    void AudioEncodeTask();
    void AudioDecodeTask();
};

#endif // _APPLICATION_H_
