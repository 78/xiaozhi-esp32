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

#include <nvs.h>
#include <nvs_flash.h>

#ifdef CONFIG_USE_AFE_SR
#include "wake_word_detect.h"
#include "audio_processor.h"
#endif


enum ChatState {
    kChatStateUnknown,
    kChatStateIdle,
    kChatStateConnecting,
    kChatStateListening,
    kChatStateSpeaking,
    kChatStateWakeWordDetected,
    kChatStateUpgrading
};

#define OPUS_FRAME_DURATION_MS 60

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
    Protocol* protocol_ = nullptr;
    EventGroupHandle_t event_group_;
    volatile ChatState chat_state_ = kChatStateUnknown;
    bool skip_to_end_ = false;

    // Audio encode / decode
    TaskHandle_t audio_encode_task_ = nullptr;
    StaticTask_t audio_encode_task_buffer_;
    StackType_t* audio_encode_task_stack_ = nullptr;
    std::list<std::vector<int16_t>> audio_encode_queue_;
    std::list<std::string> audio_decode_queue_;

    OpusEncoder opus_encoder_;
    OpusDecoder* opus_decoder_ = nullptr;

    int opus_decode_sample_rate_ = -1;
    OpusResampler input_resampler_;
    OpusResampler reference_resampler_;
    OpusResampler output_resampler_;

    void MainLoop();
    void SetDecodeSampleRate(int sample_rate);
    void CheckNewVersion();

    void AudioEncodeTask();
    void PlayLocalFile(const char* data, size_t size);
};

#endif // _APPLICATION_H_
