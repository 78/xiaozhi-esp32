#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_timer.h>

#include <string>
#include <mutex>
#include <list>

#include <opus_encoder.h>
#include <opus_decoder.h>
#include <opus_resampler.h>

#include "protocol.h"
#include "ota.h"
#include "background_task.h"
#if CONFIG_USE_ALARM
//test
#include "AlarmClock.h"
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
#include "wake_word_detect.h"
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
#include "audio_processor.h"
#endif

#define SCHEDULE_EVENT (1 << 0)
#define AUDIO_INPUT_READY_EVENT (1 << 1)
#define AUDIO_OUTPUT_READY_EVENT (1 << 2)

enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateUpgrading,
    kDeviceStateActivating,
    kDeviceStateFatalError
};

#define OPUS_FRAME_DURATION_MS 60  // 恢复原始值确保与服务器兼容

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
    DeviceState GetDeviceState() const { return device_state_; }
    bool IsVoiceDetected() const { return voice_detected_; }
    void Schedule(std::function<void()> callback);
    void SetDeviceState(DeviceState state);
    void Alert(const char* status, const char* message, const char* emotion = "", const std::string_view& sound = "");
    void DismissAlert();
    void AbortSpeaking(AbortReason reason);
    void ToggleChatState();
    void StartListening();
    void StopListening();
    void UpdateIotStates();
    void Reboot();
    void WakeWordInvoke(const std::string& wake_word);
    void PlaySound(const std::string_view& sound);
    bool CanEnterSleepMode();
    Protocol& GetProtocol() { return *protocol_; }
    Ota& GetOta() { return ota_; }
    
    // 图片资源管理功能已移除 - 网络功能已禁用
    bool IsOtaCheckCompleted() const { return ota_check_completed_; }
    
    // 图片下载模式控制方法
    void PauseAudioProcessing();  // 暂停音频处理
    void ResumeAudioProcessing(); // 恢复音频处理
    
    // 检查音频队列是否为空（用于判断开机提示音是否播放完成）
    bool IsAudioQueueEmpty() const;
    
    // **新增：强力音频保护机制**
    bool IsAudioActivityHigh() const;
    bool IsAudioProcessingCritical() const;
    void SetAudioPriorityMode(bool enabled);
    int GetAudioPerformanceScore() const;
    
    // **新增：智能分级音频保护**
    enum AudioActivityLevel {
        AUDIO_IDLE = 0,        // 完全空闲，允许正常图片播放
        AUDIO_STANDBY = 1,     // 待机状态，允许低帧率播放  
        AUDIO_ACTIVE = 2,      // 活跃状态，需要降低图片优先级
        AUDIO_CRITICAL = 3     // 关键状态，完全暂停图片播放
    };
    
    AudioActivityLevel GetAudioActivityLevel() const;
    bool IsRealAudioProcessing() const;
#if CONFIG_USE_ALARM
    //test
    AlarmManager* alarm_m_ = nullptr;
    std::list<std::vector<uint8_t>> audio_decode_queue_;
    std::unique_ptr<Protocol> protocol_;
#endif
private:
    Application();
    ~Application();

#if CONFIG_USE_WAKE_WORD_DETECT
    WakeWordDetect wake_word_detect_;
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
    AudioProcessor audio_processor_;
#endif
    Ota ota_;
    std::mutex mutex_;
    std::list<std::function<void()>> main_tasks_;
    EventGroupHandle_t event_group_ = nullptr;
    esp_timer_handle_t clock_timer_handle_ = nullptr;
    volatile DeviceState device_state_ = kDeviceStateUnknown;
    ListeningMode listening_mode_ = kListeningModeAutoStop;
#if CONFIG_USE_REALTIME_CHAT
    bool realtime_chat_enabled_ = true;
#else
    bool realtime_chat_enabled_ = false;  // 使用auto模式而不是realtime模式
#endif
    bool aborted_ = false;
    bool voice_detected_ = false;
    int clock_ticks_ = 0;
    TaskHandle_t main_loop_task_handle_ = nullptr;
    TaskHandle_t check_new_version_task_handle_ = nullptr;
    
    // 图片资源管理功能已移除 - 网络功能已禁用
    bool ota_check_completed_ = false;

    // Audio encode / decode
    TaskHandle_t audio_loop_task_handle_ = nullptr;
    BackgroundTask* background_task_ = nullptr;
    std::chrono::steady_clock::time_point last_output_time_;
#if CONFIG_USE_ALARM

#else
    std::list<std::vector<uint8_t>> audio_decode_queue_;
    std::unique_ptr<Protocol> protocol_;
#endif
    std::unique_ptr<OpusEncoderWrapper> opus_encoder_;
    std::unique_ptr<OpusDecoderWrapper> opus_decoder_;

    OpusResampler input_resampler_;
    OpusResampler reference_resampler_;
    OpusResampler output_resampler_;

    void MainLoop();
    void OnAudioInput();
    void OnAudioOutput();
    void ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples);
    void ResetDecoder();
    void SetDecodeSampleRate(int sample_rate, int frame_duration);
    void CheckNewVersion();
    void ShowActivationCode();
    void OnClockTimer();
    void SetListeningMode(ListeningMode mode);
    void AudioLoop();
};

#endif // _APPLICATION_H_
