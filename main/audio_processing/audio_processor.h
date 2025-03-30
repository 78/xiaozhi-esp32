#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <esp_afe_sr_models.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <string>
#include <vector>
#include <functional>

#include "audio_codec.h"

class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    void Initialize(AudioCodec* codec, bool realtime_chat);
    void Feed(const std::vector<int16_t>& data);
    void Start();
    void Stop();
    bool IsRunning();
    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback);
    void OnVadStateChange(std::function<void(bool speaking)> callback);
    size_t GetFeedSize();

private:
    EventGroupHandle_t event_group_ = nullptr;
    esp_afe_sr_iface_t* afe_iface_ = nullptr;
    esp_afe_sr_data_t* afe_data_ = nullptr;
    std::function<void(std::vector<int16_t>&& data)> output_callback_;
    std::function<void(bool speaking)> vad_state_change_callback_;
    AudioCodec* codec_ = nullptr;
    bool is_speaking_ = false;

    void AudioProcessorTask();
};

#endif
