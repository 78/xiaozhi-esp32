#include "afe_audio_processor.h"
#include <esp_log.h>

#define PROCESSOR_RUNNING 0x01

#define TAG "AfeAudioProcessor"

AfeAudioProcessor::AfeAudioProcessor()
    : afe_data_(nullptr) {
    event_group_ = xEventGroupCreate();
}

void AfeAudioProcessor::Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) {
    codec_ = codec;
    frame_samples_ = frame_duration_ms * 16000 / 1000;

    // Pre-allocate output buffer capacity
    output_buffer_.reserve(frame_samples_);

    int ref_num = codec_->input_reference() ? 1 : 0;

    std::string input_format;
    for (int i = 0; i < codec_->input_channels() - ref_num; i++) {
        input_format.push_back('M');
    }
    for (int i = 0; i < ref_num; i++) {
        input_format.push_back('R');
    }

    srmodel_list_t *models;
    if (models_list == nullptr) {
        models = esp_srmodel_init("model");
    } else {
        models = models_list;
    }

    char* ns_model_name = esp_srmodel_filter(models, ESP_NSNET_PREFIX, NULL);
    char* vad_model_name = esp_srmodel_filter(models, ESP_VADN_PREFIX, NULL);
    
    afe_config_t* afe_config = afe_config_init(input_format.c_str(), NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
    afe_config->aec_mode = AEC_MODE_VOIP_HIGH_PERF;
    // Single device-side VAD (vadnet1-medium). These three knobs are the ONLY
    // end-of-speech tuning surface (no app-level debounce):
    //   vad_mode          MODE_0 was too permissive (slow/unreliable to report
    //                     VAD_SILENCE); MODE_1 reliably declares silence.
    //   vad_min_noise_ms  end-of-speech hangover — min silence before ending the
    //                     turn, so a child's mid-sentence pause doesn't cut them off.
    //   vad_min_speech_ms min sustained speech before onset — suppresses noise
    //                     false-starts.
    // Starting points — tune empirically on serial (see plan Task 3).
    afe_config->vad_mode = VAD_MODE_1;
    afe_config->vad_min_speech_ms = 128;
    afe_config->vad_min_noise_ms = 700;
    if (vad_model_name != nullptr) {
        afe_config->vad_model_name = vad_model_name;
    }

    if (ns_model_name != nullptr) {
        afe_config->ns_init = true;
        afe_config->ns_model_name = ns_model_name;
        afe_config->afe_ns_mode = AFE_NS_MODE_NET;
    } else {
        afe_config->ns_init = false;
    }

    afe_config->agc_init = false;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

#ifdef CONFIG_USE_DEVICE_AEC
    afe_config->aec_init = true;
    afe_config->vad_init = false;
    vad_enabled_ = false;
#else
    afe_config->aec_init = false;
    afe_config->vad_init = true;
    vad_enabled_ = true;
#endif

    afe_iface_ = esp_afe_handle_from_config(afe_config);
    afe_data_ = afe_iface_->create_from_config(afe_config);
    
    xTaskCreate([](void* arg) {
        auto this_ = (AfeAudioProcessor*)arg;
        this_->AudioProcessorTask();
        vTaskDelete(NULL);
    }, "audio_communication", 4096, this, 3, NULL);
}

AfeAudioProcessor::~AfeAudioProcessor() {
    if (afe_data_ != nullptr) {
        afe_iface_->destroy(afe_data_);
    }
    vEventGroupDelete(event_group_);
}

size_t AfeAudioProcessor::GetFeedSize() {
    if (afe_data_ == nullptr) {
        return 0;
    }
    return afe_iface_->get_feed_chunksize(afe_data_);
}

void AfeAudioProcessor::Feed(std::vector<int16_t>&& data) {
    if (afe_data_ == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    // Check running state inside lock to avoid TOCTOU race with Stop()
    if (!IsRunning()) {
        return;
    }
    input_buffer_.insert(input_buffer_.end(), data.begin(), data.end());
    size_t chunk_size = afe_iface_->get_feed_chunksize(afe_data_) * codec_->input_channels();
    while (input_buffer_.size() >= chunk_size) {
        afe_iface_->feed(afe_data_, input_buffer_.data());
        input_buffer_.erase(input_buffer_.begin(), input_buffer_.begin() + chunk_size);
    }
}

void AfeAudioProcessor::Start() {
    xEventGroupSetBits(event_group_, PROCESSOR_RUNNING);
}

void AfeAudioProcessor::Stop() {
    xEventGroupClearBits(event_group_, PROCESSOR_RUNNING);
    reset_pending_.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    if (afe_data_ != nullptr) {
        afe_iface_->reset_buffer(afe_data_);
    }
    input_buffer_.clear();
}

bool AfeAudioProcessor::IsRunning() {
    return xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING;
}

void AfeAudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = callback;
}

void AfeAudioProcessor::OnVadStateChange(std::function<void(bool speaking)> callback) {
    vad_state_change_callback_ = callback;
}

void AfeAudioProcessor::AudioProcessorTask() {
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
        if (reset_pending_.exchange(false, std::memory_order_relaxed)) {
            is_speaking_ = false;
            output_buffer_.clear();
            continue;   // discard this fetch; it may belong to the prior session
        }
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            if (res != nullptr) {
                ESP_LOGI(TAG, "Error code: %d", res->ret_value);
            }
            continue;
        }

        // Frame energy for the noise-onset guard. AGC is off (agc_init=false)
        // and NS only attenuates noise, so this RMS is on the same scale as the
        // raw MIC meter in audio_service — a fixed threshold is meaningful.
        uint32_t frame_rms = 0;
        if (res->data != nullptr && res->data_size > 0) {
            size_t n = res->data_size / sizeof(int16_t);
            uint64_t sqsum = 0;
            for (size_t i = 0; i < n; i++) {
                int32_t s = res->data[i];
                sqsum += static_cast<uint64_t>(s) * static_cast<uint64_t>(s);
            }
            if (n > 0) frame_rms = static_cast<uint32_t>(__builtin_sqrt(sqsum / n));
        }

        // VAD state — update is_speaking_ regardless of callback presence
        // (gating reads it), then notify on edges.
        // Onset guard threshold (int16 RMS). The neural VAD occasionally flags
        // low-energy room noise as speech (worse with the +6dB mic gain), which
        // streams a 20-30s noise burst upstream and can trip a false
        // "gặp trục trặc". Tune on serial: ambient noise stays below, a child's
        // speech onset stays above. See the "VAD onset ..." log lines.
        static constexpr uint32_t kVadOnsetRmsThreshold = 240;
        // Rate-limit the suppression log to one line per noise episode.
        static bool onset_suppress_logged = false;

        bool was_speaking = is_speaking_;
        if (res->vad_state == VAD_SPEECH) {
            // Require real energy to START a turn; once speaking, let the VAD's
            // own hangover (vad_min_noise_ms) end it so a child's quiet
            // syllables aren't clipped mid-sentence.
            if (is_speaking_ || frame_rms >= kVadOnsetRmsThreshold) {
                is_speaking_ = true;
                onset_suppress_logged = false;
            } else if (!onset_suppress_logged) {
                ESP_LOGI(TAG, "VAD onset suppressed (noise): rms=%lu < thr=%u",
                         (unsigned long)frame_rms, (unsigned)kVadOnsetRmsThreshold);
                onset_suppress_logged = true;
            }
        } else if (res->vad_state == VAD_SILENCE) {
            is_speaking_ = false;
            onset_suppress_logged = false;
        }
        bool rising  = is_speaking_ && !was_speaking;   // silence -> speech
        bool falling = !is_speaking_ && was_speaking;   // speech  -> silence
        if (rising) {
            ESP_LOGI(TAG, "VAD onset (speech): rms=%lu", (unsigned long)frame_rms);
        }
        if ((rising || falling) && vad_state_change_callback_) {
            vad_state_change_callback_(is_speaking_);
        }

#ifdef CONFIG_VAD_GATED_UPSTREAM
        const bool gating = IsUpstreamGatingActive();
#else
        const bool gating = false;
#endif

        if (gating && falling) {
            // Discard the partial sub-frame tail so residual samples can't bleed
            // into the next utterance after the eos marker.
            output_buffer_.clear();
        }

        if (output_callback_ && (!gating || is_speaking_)) {
            // On the rising edge, emit the AFE pre-speech cache first so the
            // first word isn't clipped.
            if (gating && rising && res->vad_cache != nullptr && res->vad_cache_size > 0) {
                size_t cache_samples = res->vad_cache_size / sizeof(int16_t);
                output_buffer_.insert(output_buffer_.end(), res->vad_cache,
                                      res->vad_cache + cache_samples);
            }
            size_t samples = res->data_size / sizeof(int16_t);
            output_buffer_.insert(output_buffer_.end(), res->data, res->data + samples);
            while (output_buffer_.size() >= frame_samples_) {
                if (output_buffer_.size() == frame_samples_) {
                    output_callback_(std::move(output_buffer_));
                    output_buffer_.clear();
                    output_buffer_.reserve(frame_samples_);
                } else {
                    output_callback_(std::vector<int16_t>(output_buffer_.begin(),
                                                          output_buffer_.begin() + frame_samples_));
                    output_buffer_.erase(output_buffer_.begin(),
                                         output_buffer_.begin() + frame_samples_);
                }
            }
        }
    }
}

void AfeAudioProcessor::EnableDeviceAec(bool enable) {
    if (enable) {
#if CONFIG_USE_DEVICE_AEC
        afe_iface_->disable_vad(afe_data_);
        afe_iface_->enable_aec(afe_data_);
#else
        ESP_LOGE(TAG, "Device AEC is not supported");
#endif
    } else {
        afe_iface_->disable_aec(afe_data_);
        afe_iface_->enable_vad(afe_data_);
    }
}
