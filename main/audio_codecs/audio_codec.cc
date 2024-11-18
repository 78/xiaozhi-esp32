#include "audio_codec.h"
#include "board.h"
#include "settings.h"

#include <esp_log.h>
#include <cstring>
#include <driver/i2s_common.h>

#define TAG "AudioCodec"

AudioCodec::AudioCodec() {
    audio_event_group_ = xEventGroupCreate();
}

AudioCodec::~AudioCodec() {
    if (audio_input_task_ != nullptr) {
        vTaskDelete(audio_input_task_);
    }
    if (audio_output_task_ != nullptr) {
        vTaskDelete(audio_output_task_);
    }
    if (audio_event_group_ != nullptr) {
        vEventGroupDelete(audio_event_group_);
    }
}

void AudioCodec::OnInputData(std::function<void(std::vector<int16_t>&& data)> callback) {
    on_input_data_ = callback;
}

void AudioCodec::OutputData(std::vector<int16_t>& data) {
    std::lock_guard<std::mutex> lock(audio_output_queue_mutex_);
    audio_output_queue_.emplace_back(std::move(data));
    audio_output_queue_cv_.notify_one();
}

IRAM_ATTR bool AudioCodec::on_sent(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    auto audio_codec = (AudioCodec*)user_ctx;
    xEventGroupSetBits(audio_codec->audio_event_group_, AUDIO_EVENT_OUTPUT_DONE);
    return false;
}

void AudioCodec::Start() {
    Settings settings("audio", false);
    output_volume_ = settings.GetInt("output_volume", output_volume_);

    // 注册音频输出回调
    i2s_event_callbacks_t callbacks = {};
    callbacks.on_sent = on_sent;
    i2s_channel_register_event_callback(tx_handle_, &callbacks, this);

    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));

    EnableInput(true);
    EnableOutput(true);

    // 创建音频输入任务
    if (audio_input_task_ == nullptr) {
        xTaskCreate([](void* arg) {
            auto audio_device = (AudioCodec*)arg;
            audio_device->InputTask();
        }, "audio_input", 4096 * 2, this, 3, &audio_input_task_);
    }
    // 创建音频输出任务
    if (audio_output_task_ == nullptr) {
        xTaskCreate([](void* arg) {
            auto audio_device = (AudioCodec*)arg;
            audio_device->OutputTask();
        }, "audio_output", 4096 * 2, this, 3, &audio_output_task_);
    }
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

void AudioCodec::OutputTask() {
    while (true) {
        std::unique_lock<std::mutex> lock(audio_output_queue_mutex_);
        if (!audio_output_queue_cv_.wait_for(lock, std::chrono::seconds(30), [this]() {
                return !audio_output_queue_.empty();
            })) {
            // If timeout, disable output
            EnableOutput(false);
            continue;
        }
        auto data = std::move(audio_output_queue_.front());
        audio_output_queue_.pop_front();
        lock.unlock();

        if (!output_enabled_) {
            EnableOutput(true);
        }

        xEventGroupClearBits(audio_event_group_, AUDIO_EVENT_OUTPUT_DONE);
        Write(data.data(), data.size());
        audio_output_queue_cv_.notify_all();
    }
}

void AudioCodec::WaitForOutputDone() {
    // Wait for the output queue to be empty and the output is done
    std::unique_lock<std::mutex> lock(audio_output_queue_mutex_);
    audio_output_queue_cv_.wait(lock, [this]() {
        return audio_output_queue_.empty();
    });
    lock.unlock();
    xEventGroupWaitBits(audio_event_group_, AUDIO_EVENT_OUTPUT_DONE, pdFALSE, pdFALSE, portMAX_DELAY);
}

void AudioCodec::ClearOutputQueue() {
    std::lock_guard<std::mutex> lock(audio_output_queue_mutex_);
    audio_output_queue_.clear();
}

void AudioCodec::SetOutputVolume(int volume) {
    output_volume_ = volume;
    ESP_LOGI(TAG, "Set output volume to %d", output_volume_);
    
    Settings settings("audio", true);
    settings.SetInt("output_volume", output_volume_);
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
