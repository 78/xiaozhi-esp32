#include "wake_word_audio_cache.h"

#include <algorithm>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_log.h>

#define TAG "WakeWordCache"

WakeWordAudioCache::~WakeWordAudioCache() {
    if (buffer_ != nullptr) {
        heap_caps_free(buffer_);
    }
}

bool WakeWordAudioCache::Initialize(size_t sample_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_ != nullptr) {
        return capacity_ == sample_count;
    }
    if (sample_count == 0) {
        return false;
    }

    buffer_ = static_cast<int16_t*>(heap_caps_malloc(
        sample_count * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (buffer_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes in PSRAM",
            static_cast<unsigned>(sample_count * sizeof(int16_t)));
        return false;
    }

    capacity_ = sample_count;
    ESP_LOGI(TAG, "Allocated %u bytes in PSRAM",
        static_cast<unsigned>(capacity_ * sizeof(int16_t)));
    return true;
}

void WakeWordAudioCache::Store(const int16_t* data, size_t samples) {
    if (data == nullptr || samples == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_ == nullptr) {
        return;
    }

    if (samples >= capacity_) {
        data += samples - capacity_;
        samples = capacity_;
        std::memcpy(buffer_, data, samples * sizeof(int16_t));
        size_ = capacity_;
        write_position_ = 0;
        return;
    }

    const size_t first = std::min(samples, capacity_ - write_position_);
    std::memcpy(buffer_ + write_position_, data, first * sizeof(int16_t));
    if (samples > first) {
        std::memcpy(buffer_, data + first, (samples - first) * sizeof(int16_t));
    }
    write_position_ = (write_position_ + samples) % capacity_;
    size_ = std::min(capacity_, size_ + samples);
}

size_t WakeWordAudioCache::Read(size_t offset, int16_t* output, size_t samples) const {
    if (output == nullptr || samples == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_ == nullptr || offset >= size_) {
        return 0;
    }

    samples = std::min(samples, size_ - offset);
    const size_t oldest = (write_position_ + capacity_ - size_) % capacity_;
    const size_t read_position = (oldest + offset) % capacity_;
    const size_t first = std::min(samples, capacity_ - read_position);
    std::memcpy(output, buffer_ + read_position, first * sizeof(int16_t));
    if (samples > first) {
        std::memcpy(output + first, buffer_, (samples - first) * sizeof(int16_t));
    }
    return samples;
}

size_t WakeWordAudioCache::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

void WakeWordAudioCache::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_ = 0;
    write_position_ = 0;
}
