#include "OpusEncoder.h"
#include "esp_err.h"
#include "esp_log.h"

#define TAG "OpusEncoder"

OpusEncoder::OpusEncoder() {
}

OpusEncoder::~OpusEncoder() {
    if (audio_enc_ != nullptr) {
        opus_encoder_destroy(audio_enc_);
    }
}

void OpusEncoder::Configure(int sample_rate, int channels, int duration_ms) {
    if (audio_enc_ != nullptr) {
        opus_encoder_destroy(audio_enc_);
        audio_enc_ = nullptr;
    }

    int error;
    audio_enc_ = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &error);
    if (audio_enc_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create audio encoder, error code: %d", error);
        return;
    }

    // Set DTX
    opus_encoder_ctl(audio_enc_, OPUS_SET_DTX(1));
    SetComplexity(5);

    frame_size_ = sample_rate / 1000 * duration_ms;
    out_buffer_.resize(sample_rate * channels * sizeof(int16_t));
}

void OpusEncoder::SetComplexity(int complexity) {
    if (audio_enc_ != nullptr) {
        opus_encoder_ctl(audio_enc_, OPUS_SET_COMPLEXITY(complexity));
    }
}

void OpusEncoder::Encode(const iovec pcm, std::function<void(const iovec opus)> handler) {
    if (audio_enc_ == nullptr) {
        ESP_LOGE(TAG, "Audio encoder is not configured");
        return;
    }

    auto pcm_data = (int16_t*)pcm.iov_base;
    in_buffer_.insert(in_buffer_.end(), pcm_data, pcm_data + pcm.iov_len / sizeof(int16_t));

    while (in_buffer_.size() >= frame_size_) {
        auto ret = opus_encode(audio_enc_, in_buffer_.data(), frame_size_, out_buffer_.data(), out_buffer_.size());
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to encode audio, error code: %ld", ret);
            return;
        }

        if (handler != nullptr) {
            handler(iovec { out_buffer_.data(), (size_t)ret });
        }

        in_buffer_.erase(in_buffer_.begin(), in_buffer_.begin() + frame_size_);
    }
}

void OpusEncoder::ResetState() {
    if (audio_enc_ != nullptr) {
        opus_encoder_ctl(audio_enc_, OPUS_RESET_STATE);
    }
    in_buffer_.clear();
}
