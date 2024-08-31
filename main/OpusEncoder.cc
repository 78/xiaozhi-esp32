#include "OpusEncoder.h"
#include "esp_err.h"
#include "esp_log.h"

#define TAG "OpusEncoder"

OpusEncoder::OpusEncoder() {
}

OpusEncoder::~OpusEncoder() {
    if (out_buffer_ != nullptr) {
        free(out_buffer_);
    }

    if (audio_enc_ != nullptr) {
        opus_encoder_destroy(audio_enc_);
    }
}

void OpusEncoder::Configure(int sample_rate, int channels, int duration_ms) {
    if (audio_enc_ != nullptr) {
        opus_encoder_destroy(audio_enc_);
        audio_enc_ = nullptr;
    }
    if (out_buffer_ != nullptr) {
        free(out_buffer_);
        out_buffer_ = nullptr;
    }

    int error;
    audio_enc_ = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &error);
    if (audio_enc_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create audio encoder, error code: %d", error);
        return;
    }

    // Set DTX
    opus_encoder_ctl(audio_enc_, OPUS_SET_DTX(1));
    // Set complexity to 5
    opus_encoder_ctl(audio_enc_, OPUS_SET_COMPLEXITY(5));

    frame_size_ = sample_rate / 1000 * duration_ms;
    out_size_ = sample_rate * channels * sizeof(int16_t);
    out_buffer_ = (uint8_t*)malloc(out_size_);
    assert(out_buffer_ != nullptr);
}

void OpusEncoder::Encode(const void* pcm, size_t pcm_len, std::function<void(const void*, size_t)> handler) {
    if (audio_enc_ == nullptr) {
        ESP_LOGE(TAG, "Audio encoder is not configured");
        return;
    }

    in_buffer_.append((const char*)pcm, pcm_len);

    while (in_buffer_.size() >= frame_size_ * sizeof(int16_t)) {
        auto ret = opus_encode(audio_enc_, (const opus_int16*)in_buffer_.data(), frame_size_, out_buffer_, out_size_);
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to encode audio, error code: %ld", ret);
            return;
        }

        if (handler != nullptr) {
            handler(out_buffer_, ret);
        }

        in_buffer_.erase(0, frame_size_ * sizeof(int16_t));
    }
}
