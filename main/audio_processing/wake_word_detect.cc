#include "wake_word_detect.h"
#include "application.h"

#include <esp_log.h>
#include <model_path.h>
#include <arpa/inet.h>
#include <sstream>

#define DETECTION_RUNNING_EVENT 1

static const char* TAG = "WakeWordDetect";

WakeWordDetect::WakeWordDetect()
    : afe_detection_data_(nullptr),
      wake_word_pcm_(),
      wake_word_opus_() {

    event_group_ = xEventGroupCreate();
}

WakeWordDetect::~WakeWordDetect() {
    if (afe_detection_data_ != nullptr) {
        esp_afe_sr_v1.destroy(afe_detection_data_);
    }

    if (wake_word_encode_task_stack_ != nullptr) {
        heap_caps_free(wake_word_encode_task_stack_);
    }

    vEventGroupDelete(event_group_);
}

void WakeWordDetect::Initialize(int channels, bool reference) {
    channels_ = channels;
    reference_ = reference;
    int ref_num = reference_ ? 1 : 0;

    srmodel_list_t *models = esp_srmodel_init("model");
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "Model %d: %s", i, models->model_name[i]);
        if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
            wakenet_model_ = models->model_name[i];
            auto words = esp_srmodel_get_wake_words(models, wakenet_model_);
            // split by ";" to get all wake words
            std::stringstream ss(words);
            std::string word;
            while (std::getline(ss, word, ';')) {
                wake_words_.push_back(word);
            }
        }
    }

    afe_config_t afe_config = {
        .aec_init = reference_,
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
        .afe_perferred_core = 1,
        .afe_perferred_priority = 1,
        .afe_ringbuf_size = 50,
        .memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM,
        .afe_linear_gain = 1.0,
        .agc_mode = AFE_MN_PEAK_AGC_MODE_2,
        .pcm_config = {
            .total_ch_num = channels_,
            .mic_num = channels_ - ref_num,
            .ref_num = ref_num,
            .sample_rate = 16000
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
    }, "audio_detection", 4096 * 2, this, 1, nullptr);
}

void WakeWordDetect::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
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

void WakeWordDetect::Feed(std::vector<int16_t>& data) {
    input_buffer_.insert(input_buffer_.end(), data.begin(), data.end());

    auto chunk_size = esp_afe_sr_v1.get_feed_chunksize(afe_detection_data_) * channels_;
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
            StopDetection();
            last_detected_wake_word_ = wake_words_[res->wake_word_index - 1];

            if (wake_word_detected_callback_) {
                wake_word_detected_callback_(last_detected_wake_word_);
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
    wake_word_opus_.clear();
    if (wake_word_encode_task_stack_ == nullptr) {
        wake_word_encode_task_stack_ = (StackType_t*)heap_caps_malloc(4096 * 8, MALLOC_CAP_SPIRAM);
    }
    wake_word_encode_task_ = xTaskCreateStatic([](void* arg) {
        auto this_ = (WakeWordDetect*)arg;
        auto start_time = esp_timer_get_time();
        // encode detect packets
        OpusEncoder* encoder = new OpusEncoder();
        encoder->Configure(16000, 1, 60);
        encoder->SetComplexity(0);

        for (auto& pcm: this_->wake_word_pcm_) {
            encoder->Encode(pcm, [this_](const uint8_t* opus, size_t opus_size) {
                std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
                this_->wake_word_opus_.emplace_back(std::string(reinterpret_cast<const char*>(opus), opus_size));
                this_->wake_word_cv_.notify_all();
            });
        }
        this_->wake_word_pcm_.clear();

        auto end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "Encode wake word opus %zu packets in %lld ms", this_->wake_word_opus_.size(), (end_time - start_time) / 1000);
        {
            std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
            this_->wake_word_opus_.push_back("");
            this_->wake_word_cv_.notify_all();
        }
        delete encoder;
        vTaskDelete(NULL);
    }, "encode_detect_packets", 4096 * 8, this, 1, wake_word_encode_task_stack_, &wake_word_encode_task_buffer_);
}

bool WakeWordDetect::GetWakeWordOpus(std::string& opus) {
    std::unique_lock<std::mutex> lock(wake_word_mutex_);
    wake_word_cv_.wait(lock, [this]() {
        return !wake_word_opus_.empty();
    });
    opus.swap(wake_word_opus_.front());
    wake_word_opus_.pop_front();
    return !opus.empty();
}
