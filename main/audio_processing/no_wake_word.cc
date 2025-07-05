#include "no_wake_word.h"
#include <esp_log.h>

#define TAG "NoWakeWord"

void NoWakeWord::Initialize(AudioCodec* codec) {
    codec_ = codec;
}

void NoWakeWord::Feed(const std::vector<int16_t>& data) {
    // Do nothing - no wake word processing
}

void NoWakeWord::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    // Do nothing - no wake word processing
}

void NoWakeWord::StartDetection() {
    // Do nothing - no wake word processing
}

void NoWakeWord::StopDetection() {
    // Do nothing - no wake word processing
}

bool NoWakeWord::IsDetectionRunning() {
    return false;  // No wake word processing
}

size_t NoWakeWord::GetFeedSize() {
    return 0;  // No specific feed size requirement
}

void NoWakeWord::EncodeWakeWordData() {
    // Do nothing - no encoding needed
}

bool NoWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    opus.clear();
    return false;  // No opus data available
}

const std::string& NoWakeWord::GetLastDetectedWakeWord() const {
    return last_detected_wake_word_;
}