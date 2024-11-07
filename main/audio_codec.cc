#include "audio_codec.h"
#include "board.h"

#include <esp_log.h>
#include <cstring>

#define TAG "AudioCodec"

AudioCodec::AudioCodec() {
}

AudioCodec::~AudioCodec() {
    if (audio_input_task_ != nullptr) {
        vTaskDelete(audio_input_task_);
    }
}

void AudioCodec::OnInputData(std::function<void(std::vector<int16_t>&& data)> callback) {
    on_input_data_ = callback;

    // 创建音频输入任务
    if (audio_input_task_ == nullptr) {
        xTaskCreate([](void* arg) {
            auto audio_device = (AudioCodec*)arg;
            audio_device->InputTask();
        }, "audio_input", 4096 * 2, this, 3, &audio_input_task_);
    }
}

void AudioCodec::OutputData(std::vector<int16_t>& data) {
    Write(data.data(), data.size());
}

void AudioCodec::InputTask() {
    int duration = 30;
    int input_frame_size = input_sample_rate_ / 1000 * duration * input_channels_;
    while (true) {
        std::vector<int16_t> input_data(input_frame_size);
        int samples = Read(input_data.data(), input_data.size());
        if (samples > 0) {
            if (on_input_data_) {
                on_input_data_(std::move(input_data));
            }
        }
    }
}

void AudioCodec::SetOutputVolume(int volume) {
    output_volume_ = volume;
    ESP_LOGI(TAG, "Set output volume to %d", output_volume_);
}

void AudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

void AudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    output_enabled_ = enable;
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}
