#ifndef WAKE_WORD_DETECT_H
#define WAKE_WORD_DETECT_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#if CONFIG_IDF_TARGET_ESP32C3
#include "model_path.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#else
#include <esp_afe_sr_models.h>
#include <esp_nsn_models.h>
#endif

#include <list>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "audio_codec.h"

class WakeWordDetect {
public:
    WakeWordDetect();
    ~WakeWordDetect();

    void Initialize(AudioCodec* codec);
    void Feed(const std::vector<int16_t>& data);
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback);
    void StartDetection();
    void StopDetection();
    bool IsDetectionRunning();
    size_t GetFeedSize();
    void EncodeWakeWordData();
    bool GetWakeWordOpus(std::vector<uint8_t>& opus);
    const std::string& GetLastDetectedWakeWord() const { return last_detected_wake_word_; }

private:
#if CONFIG_IDF_TARGET_ESP32C3
    esp_wn_iface_t *wakenet_iface_ = nullptr;
    model_iface_data_t *wakenet_data_ = nullptr;
#else
    esp_afe_sr_iface_t* afe_iface_ = nullptr;
    esp_afe_sr_data_t* afe_data_ = nullptr;
#endif
    char* wakenet_model_ = NULL;
    std::vector<std::string> wake_words_;
    EventGroupHandle_t event_group_;
    std::function<void(const std::string& wake_word)> wake_word_detected_callback_;
    AudioCodec* codec_ = nullptr;
    std::string last_detected_wake_word_;

    TaskHandle_t wake_word_encode_task_ = nullptr;
    StaticTask_t wake_word_encode_task_buffer_;
    StackType_t* wake_word_encode_task_stack_ = nullptr;
    std::list<std::vector<int16_t>> wake_word_pcm_;
    std::list<std::vector<uint8_t>> wake_word_opus_;
    std::mutex wake_word_mutex_;
    std::condition_variable wake_word_cv_;

    void StoreWakeWordData(uint16_t* data, size_t size);

#ifndef CONFIG_IDF_TARGET_ESP32C3
    void AudioDetectionTask();
#endif
};

#endif
