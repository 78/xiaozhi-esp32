#include "audio_processor.h"
#include <esp_log.h>

#define PROCESSOR_RUNNING 0x01

static const char* TAG = "AudioProcessor";

AudioProcessor::AudioProcessor()
    : afe_data_(nullptr) {
    event_group_ = xEventGroupCreate();
}

void AudioProcessor::Initialize(int channels, bool reference) {
    channels_ = channels;
    reference_ = reference;
    int ref_num = reference_ ? 1 : 0;

    std::string input_format;
    for (int i = 0; i < channels_ - ref_num; i++) {
        input_format.push_back('M');
    }
    for (int i = 0; i < ref_num; i++) {
        input_format.push_back('R');
    }

    afe_config_t* afe_config = afe_config_init(input_format.c_str(), NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
    afe_config->aec_init = false;
    afe_config->aec_mode = AEC_MODE_VOIP_HIGH_PERF;
    afe_config->ns_init = true;
    afe_config->vad_init = true;
    afe_config->vad_mode = VAD_MODE_0;
    afe_config->vad_min_noise_ms = 100;
    afe_config->afe_perferred_core = 1;
    afe_config->afe_perferred_priority = 1;
    afe_config->agc_init = true;
    afe_config->agc_mode = AFE_AGC_MODE_WEBRTC;
    afe_config->agc_compression_gain_db = 10;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    afe_iface_ = esp_afe_handle_from_config(afe_config);
    afe_data_ = afe_iface_->create_from_config(afe_config);
    
    xTaskCreate([](void* arg) {
        auto this_ = (AudioProcessor*)arg;
        this_->AudioProcessorTask();
        vTaskDelete(NULL);
    }, "audio_communication", 4096, this, 3, NULL);
}

AudioProcessor::~AudioProcessor() {
    if (afe_data_ != nullptr) {
        afe_iface_->destroy(afe_data_);
    }
    vEventGroupDelete(event_group_);
}

void AudioProcessor::Input(const std::vector<int16_t>& data) {
    input_buffer_.insert(input_buffer_.end(), data.begin(), data.end());

    auto feed_size = afe_iface_->get_feed_chunksize(afe_data_) * channels_;
    while (input_buffer_.size() >= feed_size) {
        auto chunk = input_buffer_.data();
        afe_iface_->feed(afe_data_, chunk);
        input_buffer_.erase(input_buffer_.begin(), input_buffer_.begin() + feed_size);
    }
}

void AudioProcessor::Start() {
    xEventGroupSetBits(event_group_, PROCESSOR_RUNNING);
}

void AudioProcessor::Stop() {
    xEventGroupClearBits(event_group_, PROCESSOR_RUNNING);
    afe_iface_->reset_buffer(afe_data_);
}

bool AudioProcessor::IsRunning() {
    return xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING;
}

void AudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = callback;
}

void AudioProcessor::OnVadStateChange(std::function<void(bool speaking)> callback) {
    vad_state_change_callback_ = callback;
}

void AudioProcessor::AudioProcessorTask() {
    auto fetch_size = afe_iface_->get_fetch_chunksize(afe_data_);
    auto feed_size = afe_iface_->get_feed_chunksize(afe_data_);
    ESP_LOGI(TAG, "Audio communication task started, feed size: %d fetch size: %d",
        feed_size, fetch_size);

    while (true) {
        xEventGroupWaitBits(event_group_, PROCESSOR_RUNNING, pdFALSE, pdTRUE, portMAX_DELAY);

        auto res = afe_iface_->fetch_with_delay(afe_data_, portMAX_DELAY);
        if ((xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING) == 0) {
            continue;
        }
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            if (res != nullptr) {
                ESP_LOGI(TAG, "Error code: %d", res->ret_value);
            }
            continue;
        }

        // VAD state change
        if (vad_state_change_callback_) {
            if (res->vad_state == VAD_SPEECH && !is_speaking_) {
                is_speaking_ = true;
                vad_state_change_callback_(true);
            } else if (res->vad_state == VAD_SILENCE && is_speaking_) {
                is_speaking_ = false;
                vad_state_change_callback_(false);
            }
        }

        if (output_callback_) {
            output_callback_(std::vector<int16_t>(res->data, res->data + res->data_size / sizeof(int16_t)));
        }
    }
}
