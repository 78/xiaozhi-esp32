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

#include "protocol.h"
#include "display.h"
#include "board.h"
#include "ota.h"
#include "background_task.h"

#if CONFIG_IDF_TARGET_ESP32S3
#include "wake_word_detect.h"
#include "audio_processor.h"
#endif

#define SCHEDULE_EVENT (1 << 0)
#define AUDIO_INPUT_READY_EVENT (1 << 1)
#define AUDIO_OUTPUT_READY_EVENT (1 << 2)

enum ChatState {
    kChatStateUnknown,
    kChatStateIdle,
    kChatStateConnecting,
    kChatStateListening,
    kChatStateSpeaking,
    kChatStateUpgrading
};

#define OPUS_FRAME_DURATION_MS 60

class Application {
public:
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }
    // 删除拷贝构造函数和赋值运算符
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Start();
    ChatState GetChatState() const { return chat_state_; }
    void Schedule(std::function<void()> callback);
    void SetChatState(ChatState state);
    void Alert(const std::string&& title, const std::string&& message);
    void AbortSpeaking(AbortReason reason);
    void ToggleChatState();
    void StartListening();
    void StopListening();

private:
    Application();
    ~Application();

#if CONFIG_IDF_TARGET_ESP32S3
    WakeWordDetect wake_word_detect_;
    AudioProcessor audio_processor_;
#endif
    Ota ota_;
    std::mutex mutex_;
    std::list<std::function<void()>> main_tasks_;
    Protocol* protocol_ = nullptr;
    EventGroupHandle_t event_group_;
    volatile ChatState chat_state_ = kChatStateUnknown;
    bool keep_listening_ = false;
    bool aborted_ = false;

    // Audio encode / decode
    BackgroundTask background_task_;
    std::chrono::steady_clock::time_point last_output_time_;
    std::list<std::string> audio_decode_queue_;

    OpusEncoder opus_encoder_;
    OpusDecoder* opus_decoder_ = nullptr;

    int opus_decode_sample_rate_ = -1;
    OpusResampler input_resampler_;
    OpusResampler reference_resampler_;
    OpusResampler output_resampler_;

    void MainLoop();
    void InputAudio();
    void OutputAudio();
    void ResetDecoder();
    void SetDecodeSampleRate(int sample_rate);
    void CheckNewVersion();

    void PlayLocalFile(const char* data, size_t size);
};

#endif // _APPLICATION_H_
