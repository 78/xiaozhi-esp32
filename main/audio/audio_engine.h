#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <model_path.h>

#include "audio_codec.h"

class AudioEngine {
public:
    virtual ~AudioEngine() = default;

    virtual bool Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) = 0;
    virtual void Feed(std::vector<int16_t>&& data) = 0;

    virtual void EnableWakeWordDetection(bool enable) = 0;
    virtual void EnableVoiceProcessing(bool enable) = 0;
    virtual void EnableDeviceAec(bool enable) = 0;

    virtual bool HasWakeWord() const = 0;
    virtual bool IsWakeWordDetectionEnabled() const = 0;
    virtual bool IsVoiceProcessingEnabled() const = 0;
    virtual bool IsAfeWakeWord() const = 0;
    virtual std::vector<std::string> GetWakeWords() const = 0;
    virtual size_t GetFeedSize() const = 0;

    virtual void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) = 0;
    virtual void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) = 0;
    virtual void OnVadStateChange(std::function<void(bool speaking)> callback) = 0;

    // Wake word detection threshold: 0.4 is the easiest to trigger, 0.9999 the hardest, and
    // 0 means leave the model's compiled-in default alone. Not pure: an engine with no
    // threshold to set says so by returning false and reporting 0.
    virtual bool SetWakeWordThreshold(float threshold) { return false; }
    virtual float GetWakeWordThreshold() const { return 0.0f; }

    // How many times the engine's own processing side came back with a result since the last
    // read, and how many came back only to be discarded. Both zero while frames are still being
    // fed is the signature of a processing side that is not coming back at all - which is
    // invisible from the feed side, where every frame still looks accepted. Not pure: an engine
    // with no separate processing task reports zero.
    virtual uint32_t TakeFetchCount() { return 0; }
    virtual uint32_t TakeFetchDropCount() { return 0; }

    virtual void EncodeWakeWordData() = 0;
    virtual bool GetWakeWordOpus(std::vector<uint8_t>& opus) = 0;
    virtual const std::string& GetLastDetectedWakeWord() const = 0;
};

#endif
