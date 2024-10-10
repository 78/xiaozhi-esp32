#include <esp_log.h>
#include <model_path.h>

#include "WakeWordDetect.h"
#include "Application.h"

#define DETECTION_RUNNING_EVENT 1
#define WAKE_WORD_ENCODED_EVENT 2

static const char* TAG = "WakeWordDetect";

WakeWordDetect::WakeWordDetect()
    : afe_detection_data_(nullptr),
      wake_word_pcm_(),
      wake_word_opus_() {

    event_group_ = xEventGroupCreate();

    srmodel_list_t *models = esp_srmodel_init("model");
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "Model %d: %s", i, models->model_name[i]);
        if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
            wakenet_model_ = models->model_name[i];
        }
    }

    afe_config_t afe_config = {
        .aec_init = false,
        .se_init = true,
        .vad_init = true,
        .wakenet_init = true,
        .voice_communication_init = false,
        .voice_communication_agc_init = false,
        .voice_communication_agc_gain = 10,
        .vad_mode = VAD_MODE_3,
        .wakenet_model_name = wakenet_model_,
        .wakenet_model_name_2 = NULL,
        .wakenet_mode = DET_MODE_90,
        .afe_mode = SR_MODE_HIGH_PERF,
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
            .sample_rate = CONFIG_AUDIO_INPUT_SAMPLE_RATE
        },
        .debug_init = false,
        .debug_hook = {{ AFE_DEBUG_HOOK_MASE_TASK_IN, NULL }, { AFE_DEBUG_HOOK_FETCH_TASK_IN, NULL }},
        .afe_ns_mode = NS_MODE_SSP,
        .afe_ns_model_name = NULL,
        .fixed_first_channel = true,
    };

    afe_detection_data_ = esp_afe_sr_v1.create_from_config(&afe_config);

    xTaskCreate([](void* arg) {
        auto this_ = (WakeWordDetect*)arg;
        this_->AudioDetectionTask();
        vTaskDelete(NULL);
    }, "audio_detection", 4096 * 2, this, 5, NULL);
}

WakeWordDetect::~WakeWordDetect() {
    if (afe_detection_data_ != nullptr) {
        esp_afe_sr_v1.destroy(afe_detection_data_);
    }

    if (wake_word_encode_task_stack_ != nullptr) {
        free(wake_word_encode_task_stack_);
    }

    vEventGroupDelete(event_group_);
}

void WakeWordDetect::OnWakeWordDetected(std::function<void()> callback) {
    wake_word_detected_callback_ = callback;
}

void WakeWordDetect::OnVadStateChange(std::function<void(bool speaking)> callback) {
    vad_state_change_callback_ = callback;
}

void WakeWordDetect::StartDetection() {
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT);
}

void WakeWordDetect::StopDetection() {
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT);
}

bool WakeWordDetect::IsDetectionRunning() {
    return xEventGroupGetBits(event_group_) & DETECTION_RUNNING_EVENT;
}

void WakeWordDetect::Feed(const int16_t* data, int size) {
    input_buffer_.insert(input_buffer_.end(), data, data + size);

    auto chunk_size = esp_afe_sr_v1.get_feed_chunksize(afe_detection_data_);
    while (input_buffer_.size() >= chunk_size) {
        esp_afe_sr_v1.feed(afe_detection_data_, input_buffer_.data());
        input_buffer_.erase(input_buffer_.begin(), input_buffer_.begin() + chunk_size);
    }
}

void WakeWordDetect::AudioDetectionTask() {
    auto chunk_size = esp_afe_sr_v1.get_fetch_chunksize(afe_detection_data_);
    ESP_LOGI(TAG, "Audio detection task started, chunk size: %d", chunk_size);

    while (true) {
        xEventGroupWaitBits(event_group_, DETECTION_RUNNING_EVENT, pdFALSE, pdTRUE, portMAX_DELAY);

        auto res = esp_afe_sr_v1.fetch(afe_detection_data_);
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            if (res != nullptr) {
                ESP_LOGI(TAG, "Error code: %d", res->ret_value);
            }
            continue;;
        }

        // Store the wake word data for voice recognition, like who is speaking
        StoreWakeWordData((uint16_t*)res->data, res->data_size / sizeof(uint16_t));

        // VAD state change
        if (vad_state_change_callback_) {
            if (res->vad_state == AFE_VAD_SPEECH && !is_speaking_) {
                is_speaking_ = true;
                vad_state_change_callback_(true);
            } else if (res->vad_state == AFE_VAD_SILENCE && is_speaking_) {
                is_speaking_ = false;
                vad_state_change_callback_(false);
            }
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "Wake word detected");
            StopDetection();

            if (wake_word_detected_callback_) {
                wake_word_detected_callback_();
            }
        }
    }
}

void WakeWordDetect::StoreWakeWordData(uint16_t* data, size_t samples) {
    // store audio data to wake_word_pcm_
    std::vector<int16_t> pcm(data, data + samples);
    wake_word_pcm_.emplace_back(std::move(pcm));
    // keep about 2 seconds of data, detect duration is 32ms (sample_rate == 16000, chunksize == 512)
    while (wake_word_pcm_.size() > 2000 / 32) {
        wake_word_pcm_.pop_front();
    }
}

void WakeWordDetect::EncodeWakeWordData() {
    if (wake_word_encode_task_stack_ == nullptr) {
        wake_word_encode_task_stack_ = (StackType_t*)heap_caps_malloc(8*4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    wake_word_encode_task_ = xTaskCreateStatic([](void* arg) {
        auto this_ = (WakeWordDetect*)arg;
        auto start_time = esp_timer_get_time();
        // encode detect packets
        OpusEncoder* encoder = new OpusEncoder();
        encoder->Configure(CONFIG_AUDIO_INPUT_SAMPLE_RATE, 1, 60);
        encoder->SetComplexity(0);
        this_->wake_word_opus_.resize(4096 * 4);
        size_t offset = 0;

        for (auto& pcm: this_->wake_word_pcm_) {
            encoder->Encode(pcm, [this_, &offset](const uint8_t* opus, size_t opus_size) {
                size_t protocol_size = sizeof(BinaryProtocol) + opus_size;
                if (offset + protocol_size < this_->wake_word_opus_.size()) {
                    auto protocol = (BinaryProtocol*)(&this_->wake_word_opus_[offset]);
                    protocol->version = htons(PROTOCOL_VERSION);
                    protocol->type = htons(0);
                    protocol->reserved = 0;
                    protocol->timestamp = 0;
                    protocol->payload_size = htonl(opus_size);
                    memcpy(protocol->payload, opus, opus_size);
                    offset += protocol_size;
                }
            });
        }
        this_->wake_word_pcm_.clear();
        this_->wake_word_opus_.resize(offset);

        auto end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "Encode wake word opus: %zu bytes in %lld ms", this_->wake_word_opus_.size(), (end_time - start_time) / 1000);
        xEventGroupSetBits(this_->event_group_, WAKE_WORD_ENCODED_EVENT);
        delete encoder;
        vTaskDelete(NULL);
    }, "encode_detect_packets", 4096 * 8, this, 1, wake_word_encode_task_stack_, &wake_word_encode_task_buffer_);
}

const std::string&& WakeWordDetect::GetWakeWordStream() {
    xEventGroupWaitBits(event_group_, WAKE_WORD_ENCODED_EVENT, pdTRUE, pdTRUE, portMAX_DELAY);
    return std::move(wake_word_opus_);
}
