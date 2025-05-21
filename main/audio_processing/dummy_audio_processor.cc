#include "dummy_audio_processor.h"
#include <esp_log.h>

static const char* TAG = "DummyAudioProcessor";

void DummyAudioProcessor::Initialize(AudioCodec* codec, bool realtime_chat) {
    codec_ = codec;
}

void DummyAudioProcessor::Feed(const std::vector<int16_t>& data) {
    if (!is_running_ || !output_callback_) {
        return;
    }
    // 直接将输入数据传递给输出回调
    output_callback_(std::vector<int16_t>(data));
}

void DummyAudioProcessor::Start() {
    is_running_ = true;
}

void DummyAudioProcessor::Stop() {
    is_running_ = false;
}

bool DummyAudioProcessor::IsRunning() {
    return is_running_;
}

void DummyAudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = callback;
}

void DummyAudioProcessor::OnVadStateChange(std::function<void(bool speaking)> callback) {
    vad_state_change_callback_ = callback;
}

size_t DummyAudioProcessor::GetFeedSize() {
    if (!codec_) {
        return 0;
    }
    // 返回一个固定的帧大小，比如 30ms 的数据
    return 30 * codec_->input_sample_rate() / 1000;
}