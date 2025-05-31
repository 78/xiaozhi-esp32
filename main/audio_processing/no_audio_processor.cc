#include "no_audio_processor.h"
#include <esp_log.h>

#define TAG "NoAudioProcessor"

void NoAudioProcessor::Initialize(AudioCodec* codec) {
    codec_ = codec;
}

void NoAudioProcessor::Feed(const std::vector<int16_t>& data) {
    if (!is_running_ || !output_callback_) {
        return;
    }
    // 直接将输入数据传递给输出回调
    output_callback_(std::vector<int16_t>(data));
}

void NoAudioProcessor::Start() {
    is_running_ = true;
}

void NoAudioProcessor::Stop() {
    is_running_ = false;
}

bool NoAudioProcessor::IsRunning() {
    return is_running_;
}

void NoAudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = callback;
}

void NoAudioProcessor::OnVadStateChange(std::function<void(bool speaking)> callback) {
    vad_state_change_callback_ = callback;
}

size_t NoAudioProcessor::GetFeedSize() {
    if (!codec_) {
        return 0;
    }
    // 返回一个固定的帧大小，比如 30ms 的数据
    return 30 * codec_->input_sample_rate() / 1000;
}

void NoAudioProcessor::EnableDeviceAec(bool enable) {
    if (enable) {
        ESP_LOGE(TAG, "Device AEC is not supported");
    }
}
