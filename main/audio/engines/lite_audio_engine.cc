#include "lite_audio_engine.h"

#include <esp_log.h>
#include <esp_wn_models.h>

#include "wake_words/esp_wake_word.h"

#define TAG "LiteAudioEngine"

LiteAudioEngine::LiteAudioEngine() = default;
LiteAudioEngine::~LiteAudioEngine() = default;

bool LiteAudioEngine::Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) {
    codec_ = codec;
    frame_samples_ = frame_duration_ms * 16000 / 1000;
    output_buffer_.reserve(frame_samples_);

    bool has_wakenet = models_list != nullptr &&
        esp_srmodel_filter(models_list, ESP_WN_PREFIX, nullptr) != nullptr;
#if CONFIG_USE_ESP_WAKE_WORD
    has_wakenet = has_wakenet || models_list == nullptr;
#endif
    if (has_wakenet) {
        wake_word_ = std::make_unique<EspWakeWord>();
        wake_word_->OnWakeWordDetected([this](const std::string& wake_word) {
            wake_word_enabled_ = false;
            if (wake_word_detected_callback_) {
                wake_word_detected_callback_(wake_word);
            }
        });
        if (!wake_word_->Initialize(codec_, models_list)) {
            ESP_LOGE(TAG, "Failed to initialize standalone WakeNet");
            wake_word_.reset();
            return false;
        }
    }

    ESP_LOGI(TAG, "Initialized, WakeNet: %s", wake_word_ ? "yes" : "no");
    return true;
}

void LiteAudioEngine::Feed(std::vector<int16_t>&& data) {
    if (wake_word_enabled_ && wake_word_) {
        wake_word_->Feed(data);
    }
    if (voice_processing_enabled_) {
        OutputRawAudio(data);
    }
}

void LiteAudioEngine::EnableWakeWordDetection(bool enable) {
    if (!wake_word_) {
        wake_word_enabled_ = false;
        return;
    }

    wake_word_enabled_ = enable;
    if (enable) {
        wake_word_->Start();
    } else {
        wake_word_->Stop();
    }
}

void LiteAudioEngine::EnableVoiceProcessing(bool enable) {
    voice_processing_enabled_ = enable;
    if (!enable) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        output_buffer_.clear();
    }
}

void LiteAudioEngine::EnableDeviceAec(bool enable) {
    if (enable) {
        ESP_LOGW(TAG, "Device AEC is not supported by the lite engine");
    }
}

bool LiteAudioEngine::HasWakeWord() const {
    return wake_word_ != nullptr;
}

bool LiteAudioEngine::IsWakeWordDetectionEnabled() const {
    return wake_word_enabled_;
}

bool LiteAudioEngine::IsVoiceProcessingEnabled() const {
    return voice_processing_enabled_;
}

size_t LiteAudioEngine::GetFeedSize() const {
    if (wake_word_) {
        return wake_word_->GetFeedSize();
    }
    return frame_samples_;
}

void LiteAudioEngine::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = std::move(callback);
}

void LiteAudioEngine::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = std::move(callback);
}

void LiteAudioEngine::OnVadStateChange(std::function<void(bool speaking)> callback) {
    vad_state_change_callback_ = std::move(callback);
}

void LiteAudioEngine::EncodeWakeWordData() {
    if (wake_word_) {
        wake_word_->EncodeWakeWordData();
    }
}

bool LiteAudioEngine::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    return wake_word_ && wake_word_->GetWakeWordOpus(opus);
}

const std::string& LiteAudioEngine::GetLastDetectedWakeWord() const {
    return wake_word_ ? wake_word_->GetLastDetectedWakeWord() : empty_wake_word_;
}

void LiteAudioEngine::OutputRawAudio(const std::vector<int16_t>& data) {
    if (!output_callback_ || codec_ == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(output_mutex_);
    const size_t channels = codec_->input_channels();
    if (channels <= 1) {
        output_buffer_.insert(output_buffer_.end(), data.begin(), data.end());
    } else {
        for (size_t i = 0; i < data.size(); i += channels) {
            output_buffer_.push_back(data[i]);
        }
    }

    while (output_buffer_.size() >= static_cast<size_t>(frame_samples_)) {
        if (output_buffer_.size() == static_cast<size_t>(frame_samples_)) {
            output_callback_(std::move(output_buffer_));
            output_buffer_.clear();
            output_buffer_.reserve(frame_samples_);
        } else {
            output_callback_(std::vector<int16_t>(
                output_buffer_.begin(), output_buffer_.begin() + frame_samples_));
            output_buffer_.erase(output_buffer_.begin(), output_buffer_.begin() + frame_samples_);
        }
    }
}
