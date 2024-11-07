#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <opus.h>
#include <mutex>
#include <list>
#include <condition_variable>

#include "opus_encoder.h"
#include "opus_resampler.h"
#include <web_socket.h>

#include "display.h"
#include "board.h"
#include "ota.h"

#ifdef CONFIG_USE_AFE_SR
#include "wake_word_detect.h"
#include "audio_processor.h"
#endif

#define DETECTION_RUNNING 1
#define COMMUNICATION_RUNNING 2

#define PROTOCOL_VERSION 3
struct BinaryProtocol3 {
    uint8_t type;
    uint8_t reserved;
    uint16_t payload_size;
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
    kChatStateUnknown,
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
    ChatState GetChatState() const { return chat_state_; }
    void Schedule(std::function<void()> callback);
    void SetChatState(ChatState state);
    void Alert(const std::string&& title, const std::string&& message);
    void AbortSpeaking();
    void ToggleChatState();
    // 删除拷贝构造函数和赋值运算符
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

private:
    Application();
    ~Application();

#ifdef CONFIG_USE_AFE_SR
    WakeWordDetect wake_word_detect_;
    AudioProcessor audio_processor_;
#endif
    Ota ota_;
    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::list<std::function<void()>> main_tasks_;
    WebSocket* ws_client_ = nullptr;
    EventGroupHandle_t event_group_;
    volatile ChatState chat_state_ = kChatStateUnknown;
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
    int opus_decode_sample_rate_ = -1;
    OpusResampler input_resampler_;
    OpusResampler reference_resampler_;
    OpusResampler output_resampler_;

    TaskHandle_t main_loop_task_ = nullptr;
    StaticTask_t main_loop_task_buffer_;
    StackType_t* main_loop_task_stack_ = nullptr;

    void MainLoop();
    BinaryProtocol3* AllocateBinaryProtocol3(const uint8_t* payload, size_t payload_size);
    void ParseBinaryProtocol3(const char* data, size_t size);
    void SetDecodeSampleRate(int sample_rate);
    void StartWebSocketClient();
    void CheckNewVersion();

    void AudioEncodeTask();
    void AudioPlayTask();
    void HandleAudioPacket(AudioPacket* packet);
    void PlayLocalFile(const char* data, size_t size);
};

#endif // _APPLICATION_H_
