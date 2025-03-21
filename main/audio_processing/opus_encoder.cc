#include "opus_encoder.h"
#include <esp_log.h>

#define TAG "OpusEncoderWrapper"


OpusEncoderWrapper::OpusEncoderWrapper(int sample_rate, int channels, int duration_ms) {
    int error = 0;
    esp_opus_enc_config_t opus_enc = ESP_OPUS_ENC_CONFIG_DEFAULT();
    opus_enc.sample_rate = sample_rate;
    opus_enc.channel = channels;
    opus_enc.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS;
    opus_enc.complexity = 1;
    opus_enc.bitrate = 20000;
    opus_enc.enable_dtx = true;
    opus_enc.enable_vbr = true;
    esp_opus_enc_register();

    esp_opus_enc_open(&opus_enc, sizeof(esp_opus_enc_config_t), &opus_enc_);
    if (opus_enc_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create audio encoder, error code: %d", error);
        return;
    }
    esp_opus_enc_get_frame_size(opus_enc_, &frame_size_, &out_byte_size_);
    frame_size_ /= 2;
}

OpusEncoderWrapper::~OpusEncoderWrapper() {
    if (opus_enc_ != nullptr) {
        esp_opus_enc_close(opus_enc_);
    }
}

void OpusEncoderWrapper::Encode(std::vector<int16_t>&& pcm, std::function<void(std::vector<uint8_t>&& opus)> handler) {
    if (opus_enc_ == nullptr) {
        ESP_LOGE(TAG, "Audio encoder is not configured");
        return;
    }

    if (in_buffer_.empty()) {
        in_buffer_ = std::move(pcm);
    } else {
        in_buffer_.insert(in_buffer_.end(), pcm.begin(), pcm.end());
    }

    while (in_buffer_.size() >= frame_size_) {
        std::vector<uint8_t> opus(out_byte_size_);
        esp_audio_enc_in_frame_t in ={
            .buffer = (uint8_t*)in_buffer_.data(),
            .len = (uint32_t)frame_size_*2,
        };
        esp_audio_enc_out_frame_t out ={
            .buffer = opus.data(),
            .len = opus.size(),
            .encoded_bytes = 0,
        };
        auto ret = esp_opus_enc_process(opus_enc_, &in, &out);
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to encode audio, error code: %d, out.len:%ld", ret, out.len);
            return;
        }
        opus.resize(out.encoded_bytes);
        ESP_LOGD(TAG, "Encode audio, frame_size:%d, out_byte_size:%d", frame_size_, opus.size());
        if (handler != nullptr) {
            handler(std::move(opus));
        }
        in_buffer_.erase(in_buffer_.begin(), in_buffer_.begin() + frame_size_);
    }
}

void OpusEncoderWrapper::ResetState() {
    ESP_LOGD(TAG, "%s", __func__);
    if (opus_enc_ != nullptr) {
        in_buffer_.clear();
    }
}

void OpusEncoderWrapper::SetDtx(bool enable) {
    ESP_LOGD(TAG, "%s, enable:%d", __func__, enable);
}

void OpusEncoderWrapper::SetComplexity(int complexity) {
    ESP_LOGD(TAG, "%s, complexity:%d", __func__, complexity);
}
