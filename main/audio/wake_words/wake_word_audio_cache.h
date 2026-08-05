#ifndef WAKE_WORD_AUDIO_CACHE_H
#define WAKE_WORD_AUDIO_CACHE_H

#include <cstddef>
#include <cstdint>
#include <mutex>

class WakeWordAudioCache {
public:
    WakeWordAudioCache() = default;
    ~WakeWordAudioCache();

    WakeWordAudioCache(const WakeWordAudioCache&) = delete;
    WakeWordAudioCache& operator=(const WakeWordAudioCache&) = delete;

    bool Initialize(size_t sample_count);
    void Store(const int16_t* data, size_t samples);
    size_t Read(size_t offset, int16_t* output, size_t samples) const;
    size_t Size() const;
    void Clear();

private:
    int16_t* buffer_ = nullptr;
    size_t capacity_ = 0;
    size_t size_ = 0;
    size_t write_position_ = 0;
    mutable std::mutex mutex_;
};

#endif
