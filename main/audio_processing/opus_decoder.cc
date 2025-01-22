#include "opus_decoder.h"
#include <esp_log.h>
#include "esp_audio_dec_default.h"

#define TAG "OpusDecoderWrapper"


OpusDecoderWrapper::OpusDecoderWrapper(int sample_rate, int channels, int duration_ms)
{
    int error = 0;
    esp_opus_dec_register();
    esp_opus_dec_cfg_t opus_cfg = {
        .sample_rate = (uint32_t)sample_rate,
        .channel = (uint8_t)channels,
        .self_delimited = false,
    };
    esp_opus_dec_open(&opus_cfg, sizeof(esp_opus_dec_cfg_t), &audio_dec_);
    if (audio_dec_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create audio decoder, error code: %d", error);
        return;
    }
    frame_size_ = sample_rate / 1000 * channels * duration_ms;
}

OpusDecoderWrapper::~OpusDecoderWrapper()
{
    if (audio_dec_ != nullptr) {
        esp_audio_dec_close(audio_dec_);
    }
}

bool OpusDecoderWrapper::Decode(std::vector<uint8_t> &&opus, std::vector<int16_t> &pcm)
{
    if (audio_dec_ == nullptr) {
        ESP_LOGE(TAG, "Audio decoder is not configured");
        return false;
    }
    pcm.resize(frame_size_);

    esp_audio_dec_info_t info = {0};
    esp_audio_dec_in_raw_t in = {
        .buffer = opus.data(),
        .len = opus.size(),
    };
    esp_audio_dec_out_frame_t out = {
        .buffer = (uint8_t*)pcm.data(),
        .len = 2880,
    };
    int ret = esp_opus_dec_decode(audio_dec_, &in, &out, &info);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to decode audio, error code: %d, OPUS sz:%d, PCM sz:%d, consumed:%ld, needed_size:%ld", ret, opus.size(), pcm.size(), in.consumed, out.needed_size);
        return false;
    }
    pcm.resize(out.decoded_size/2);
    ESP_LOGD(TAG, "Decode audio:%d, OPUS sz:%d, PCM sz:%d, consumed:%ld, needed_size:%ld", ret, opus.size(), pcm.size(), in.consumed, out.needed_size);

    return true;
}

void OpusDecoderWrapper::ResetState()
{
    ESP_LOGD(TAG, "%s", __func__);
}

