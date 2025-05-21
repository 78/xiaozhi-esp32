#ifndef DUMMY_AUDIO_PROCESSOR_H
#define DUMMY_AUDIO_PROCESSOR_H

#include <vector>
#include <functional>

#include "audio_processor.h"
#include "audio_codec.h"

class DummyAudioProcessor : public AudioProcessor {
public:
    DummyAudioProcessor() = default;
    ~DummyAudioProcessor() = default;

    void Initialize(AudioCodec* codec, bool realtime_chat) override;
    void Feed(const std::vector<int16_t>& data) override;
    void Start() override;
    void Stop() override;
    bool IsRunning() override;
    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override;
    void OnVadStateChange(std::function<void(bool speaking)> callback) override;
    size_t GetFeedSize() override;

private:
    AudioCodec* codec_ = nullptr;
    std::function<void(std::vector<int16_t>&& data)> output_callback_;
    std::function<void(bool speaking)> vad_state_change_callback_;
    bool is_running_ = false;
};

#endif 