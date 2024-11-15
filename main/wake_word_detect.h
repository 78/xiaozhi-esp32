#ifndef WAKE_WORD_DETECT_H
#define WAKE_WORD_DETECT_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_afe_sr_models.h>
#include <esp_nsn_models.h>

#include <list>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>


class WakeWordDetect {
public:
    WakeWordDetect();
    ~WakeWordDetect();

    void Initialize(int channels, bool reference);
    void Feed(std::vector<int16_t>& data);
    void OnWakeWordDetected(std::function<void()> callback);
    void OnVadStateChange(std::function<void(bool speaking)> callback);
    void StartDetection();
    void StopDetection();
    bool IsDetectionRunning();
    void EncodeWakeWordData();
    bool GetWakeWordOpus(std::string& opus);

private:
    esp_afe_sr_data_t* afe_detection_data_ = nullptr;
    char* wakenet_model_ = NULL;
    std::vector<int16_t> input_buffer_;
    EventGroupHandle_t event_group_;
    std::function<void()> wake_word_detected_callback_;
    std::function<void(bool speaking)> vad_state_change_callback_;
    bool is_speaking_ = false;
    int channels_;
    bool reference_;

    TaskHandle_t audio_detection_task_ = nullptr;
    StaticTask_t audio_detection_task_buffer_;
    StackType_t* audio_detection_task_stack_ = nullptr;

    TaskHandle_t wake_word_encode_task_ = nullptr;
    StaticTask_t wake_word_encode_task_buffer_;
    StackType_t* wake_word_encode_task_stack_ = nullptr;
    std::list<std::vector<int16_t>> wake_word_pcm_;
    std::list<std::string> wake_word_opus_;
    std::mutex wake_word_mutex_;
    std::condition_variable wake_word_cv_;

    void StoreWakeWordData(uint16_t* data, size_t size);
    void AudioDetectionTask();
};

#endif
