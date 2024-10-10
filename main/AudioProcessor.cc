#include "AudioProcessor.h"
#include <esp_log.h>

#define PROCESSOR_RUNNING 0x01

static const char* TAG = "AudioProcessor";

AudioProcessor::AudioProcessor()
    : afe_communication_data_(nullptr) {
    event_group_ = xEventGroupCreate();

    afe_config_t afe_config = {
        .aec_init = false,
        .se_init = false,
        .vad_init = false,
        .wakenet_init = false,
        .voice_communication_init = true,
        .voice_communication_agc_init = true,
        .voice_communication_agc_gain = 10,
        .vad_mode = VAD_MODE_3,
        .wakenet_model_name = NULL,
        .wakenet_model_name_2 = NULL,
        .wakenet_mode = DET_MODE_90,
        .afe_mode = SR_MODE_LOW_COST,
        .afe_perferred_core = 0,
        .afe_perferred_priority = 5,
        .afe_ringbuf_size = 50,
        .memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM,
        .afe_linear_gain = 1.0,
        .agc_mode = AFE_MN_PEAK_AGC_MODE_2,
        .pcm_config = {
            .total_ch_num = 1,
            .mic_num = 1,
            .ref_num = 0,
            .sample_rate = CONFIG_AUDIO_INPUT_SAMPLE_RATE,
        },
        .debug_init = false,
        .debug_hook = {{ AFE_DEBUG_HOOK_MASE_TASK_IN, NULL }, { AFE_DEBUG_HOOK_FETCH_TASK_IN, NULL }},
        .afe_ns_mode = NS_MODE_SSP,
        .afe_ns_model_name = NULL,
        .fixed_first_channel = true,
    };

    afe_communication_data_ = esp_afe_vc_v1.create_from_config(&afe_config);
    
    xTaskCreate([](void* arg) {
        auto this_ = (AudioProcessor*)arg;
        this_->AudioProcessorTask();
        vTaskDelete(NULL);
    }, "audio_communication", 4096 * 2, this, 5, NULL);
}

AudioProcessor::~AudioProcessor() {
    if (afe_communication_data_ != nullptr) {
        esp_afe_vc_v1.destroy(afe_communication_data_);
    }
    vEventGroupDelete(event_group_);
}

void AudioProcessor::Input(const int16_t* data, int size) {
    input_buffer_.insert(input_buffer_.end(), data, data + size);

    auto chunk_size = esp_afe_vc_v1.get_feed_chunksize(afe_communication_data_);
    while (input_buffer_.size() >= chunk_size) {
        auto chunk = input_buffer_.data();
        esp_afe_vc_v1.feed(afe_communication_data_, chunk);
        input_buffer_.erase(input_buffer_.begin(), input_buffer_.begin() + chunk_size);
    }
}

void AudioProcessor::Start() {
    xEventGroupSetBits(event_group_, PROCESSOR_RUNNING);
}

void AudioProcessor::Stop() {
    xEventGroupClearBits(event_group_, PROCESSOR_RUNNING);
}

bool AudioProcessor::IsRunning() {
    return xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING;
}

void AudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = callback;
}

void AudioProcessor::AudioProcessorTask() {
    int chunk_size = esp_afe_vc_v1.get_fetch_chunksize(afe_communication_data_);
    ESP_LOGI(TAG, "Audio communication task started, chunk size: %d", chunk_size);

    while (true) {
        xEventGroupWaitBits(event_group_, PROCESSOR_RUNNING, pdFALSE, pdTRUE, portMAX_DELAY);

        auto res = esp_afe_vc_v1.fetch(afe_communication_data_);
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            if (res != nullptr) {
                ESP_LOGI(TAG, "Error code: %d", res->ret_value);
            }
            continue;
        }

        if (output_callback_) {
            output_callback_(std::vector<int16_t>(res->data, res->data + res->data_size / sizeof(int16_t)));
        }
    }
}
