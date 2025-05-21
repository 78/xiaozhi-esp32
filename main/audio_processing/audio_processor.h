#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <string>
#include <vector>
#include <functional>

#include "audio_codec.h"

class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;
    
    virtual void Initialize(AudioCodec* codec, bool realtime_chat) = 0;
    virtual void Feed(const std::vector<int16_t>& data) = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() = 0;
    virtual void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) = 0;
    virtual void OnVadStateChange(std::function<void(bool speaking)> callback) = 0;
    virtual size_t GetFeedSize() = 0;
};

#endif
