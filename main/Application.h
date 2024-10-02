#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include "AudioDevice.h"
#include <OpusEncoder.h>
#include <OpusResampler.h>
#include <WebSocket.h>
#include <Ml307AtModem.h>
#include <Ml307Http.h>
#include <EspHttp.h>

#include <opus.h>
#include <resampler_structs.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <mutex>
#include <list>
#include <condition_variable>

#include "Display.h"
#include "FirmwareUpgrade.h"

#ifdef CONFIG_USE_AFE_SR
#include "WakeWordDetect.h"
#include "AudioProcessor.h"
#endif

#include "Button.h"

#define DETECTION_RUNNING 1
#define COMMUNICATION_RUNNING 2

#define PROTOCOL_VERSION 2
struct BinaryProtocol {
    uint16_t version;
    uint16_t type;
    uint32_t reserved;
    uint32_t timestamp;
    uint32_t payload_size;
    uint8_t payload[];
} __attribute__((packed));

enum AudioPacketType {
    kAudioPacketTypeUnkonwn = 0,
    kAudioPacketTypeStart,
    kAudioPacketTypeStop,
    kAudioPacketTypeData,
    kAudioPacketTypeSentenceStart,
    kAudioPacketTypeSentenceEnd
};

struct AudioPacket {
    AudioPacketType type = kAudioPacketTypeUnkonwn;
    std::string text;
    std::vector<uint8_t> opus;
    std::vector<int16_t> pcm;
    uint32_t timestamp;
};


enum ChatState {
    kChatStateIdle,
    kChatStateConnecting,
    kChatStateListening,
    kChatStateSpeaking,
    kChatStateWakeWordDetected,
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

    Button button_;
    AudioDevice audio_device_;
#ifdef CONFIG_USE_AFE_SR
    WakeWordDetect wake_word_detect_;
    AudioProcessor audio_processor_;
#endif
#ifdef CONFIG_USE_ML307
    Ml307AtModem ml307_at_modem_;
    Ml307Http http_;
#else
    EspHttp http_;
#endif
    FirmwareUpgrade firmware_upgrade_;
#ifdef CONFIG_USE_DISPLAY
    Display display_;
#endif
    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::list<std::function<void()>> main_tasks_;
    WebSocket* ws_client_ = nullptr;
    EventGroupHandle_t event_group_;
    volatile ChatState chat_state_ = kChatStateIdle;
    volatile bool break_speaking_ = false;
    bool skip_to_end_ = false;

    // Audio encode / decode
    TaskHandle_t audio_encode_task_ = nullptr;
    StaticTask_t audio_encode_task_buffer_;
    StackType_t* audio_encode_task_stack_ = nullptr;
    std::list<std::vector<int16_t>> audio_encode_queue_;
    std::list<AudioPacket*> audio_decode_queue_;
    std::list<AudioPacket*> audio_play_queue_;

    OpusEncoder opus_encoder_;
    OpusDecoder* opus_decoder_ = nullptr;

    int opus_duration_ms_ = 60;
    int opus_decode_sample_rate_ = CONFIG_AUDIO_OUTPUT_SAMPLE_RATE;
    OpusResampler opus_resampler_;

    TaskHandle_t check_new_version_task_ = nullptr;
    StaticTask_t check_new_version_task_buffer_;
    StackType_t* check_new_version_task_stack_ = nullptr;

    void MainLoop();
    void Schedule(std::function<void()> callback);
    BinaryProtocol* AllocateBinaryProtocol(const uint8_t* payload, size_t payload_size);
    void SetDecodeSampleRate(int sample_rate);
    void SetChatState(ChatState state);
    void StartWebSocketClient();
    void CheckNewVersion();
    void UpdateDisplay();

    void AudioEncodeTask();
    void AudioPlayTask();
    void HandleAudioPacket(AudioPacket* packet);
};

#endif // _APPLICATION_H_
