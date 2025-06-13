#ifndef NO_WAKE_WORD_H
#define NO_WAKE_WORD_H

#include <vector>
#include <functional>
#include <string>

#include "wake_word.h"
#include "audio_codec.h"

class NoWakeWord : public WakeWord {
public:
    NoWakeWord() = default;
    ~NoWakeWord() = default;

    void Initialize(AudioCodec* codec) override;
    void Feed(const std::vector<int16_t>& data) override;
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) override;
    void StartDetection() override;
    void StopDetection() override;
    bool IsDetectionRunning() override;
    size_t GetFeedSize() override;
    void EncodeWakeWordData() override;
    bool GetWakeWordOpus(std::vector<uint8_t>& opus) override;
    const std::string& GetLastDetectedWakeWord() const override;

private:
    AudioCodec* codec_ = nullptr;
    std::string last_detected_wake_word_;
};

#endif 