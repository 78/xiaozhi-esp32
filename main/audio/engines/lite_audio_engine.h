#ifndef LITE_AUDIO_ENGINE_H
#define LITE_AUDIO_ENGINE_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio_engine.h"

class EspWakeWord;

class LiteAudioEngine : public AudioEngine {
public:
    LiteAudioEngine();
    ~LiteAudioEngine() override;

    bool Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) override;
    void Feed(std::vector<int16_t>&& data) override;

    void EnableWakeWordDetection(bool enable) override;
    void EnableVoiceProcessing(bool enable) override;
    void EnableDeviceAec(bool enable) override;

    bool HasWakeWord() const override;
    bool IsWakeWordDetectionEnabled() const override;
    bool IsVoiceProcessingEnabled() const override;
    bool IsAfeWakeWord() const override { return false; }
    size_t GetFeedSize() const override;

    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) override;
    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override;
    void OnVadStateChange(std::function<void(bool speaking)> callback) override;

    void EncodeWakeWordData() override;
    bool GetWakeWordOpus(std::vector<uint8_t>& opus) override;
    const std::string& GetLastDetectedWakeWord() const override;

private:
    AudioCodec* codec_ = nullptr;
    std::unique_ptr<EspWakeWord> wake_word_;
    std::atomic<bool> wake_word_enabled_ = false;
    std::atomic<bool> voice_processing_enabled_ = false;
    int frame_samples_ = 0;
    std::vector<int16_t> output_buffer_;
    std::mutex output_mutex_;

    std::function<void(const std::string&)> wake_word_detected_callback_;
    std::function<void(std::vector<int16_t>&&)> output_callback_;
    std::function<void(bool)> vad_state_change_callback_;
    std::string empty_wake_word_;

    void OutputRawAudio(const std::vector<int16_t>& data);
};

#endif
