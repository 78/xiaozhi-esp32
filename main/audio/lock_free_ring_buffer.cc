#include "lock_free_ring_buffer.h"

#include <cstring>
#include <algorithm>
#include <esp_heap_caps.h>
#include <esp_log.h>

#define TAG "RingBuf"

LockFreeRingBuffer::LockFreeRingBuffer(size_t capacity)
    : capacity_(capacity) {
    buffer_ = (int16_t*)heap_caps_malloc(capacity * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!buffer_) {
        ESP_LOGE(TAG, "Failed to allocate %zu samples in PSRAM", capacity);
    }
}

LockFreeRingBuffer::~LockFreeRingBuffer() {
    if (buffer_) {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
    }
}

void LockFreeRingBuffer::Write(const int16_t* data, size_t count) {
    if (!buffer_ || count == 0) return;

    // SPSC contract: producer owns write_pos_ only, consumer owns read_pos_ only.
    // When the buffer is full we silently overwrite the oldest samples. The consumer
    // will still read valid (if stale) data because we never touch read_pos_ here.
    size_t w = write_pos_.load(std::memory_order_relaxed);

    for (size_t i = 0; i < count; i++) {
        buffer_[w] = data[i];
        w = (w + 1) % capacity_;
    }

    write_pos_.store(w, std::memory_order_release);
}

size_t LockFreeRingBuffer::Read(int16_t* data, size_t count) {
    if (!buffer_ || count == 0) return 0;

    size_t r = read_pos_.load(std::memory_order_relaxed);
    size_t w = write_pos_.load(std::memory_order_acquire);

    size_t available = (w >= r) ? (w - r) : (capacity_ - r + w);

    // Not enough data: zero-fill everything and return without consuming.
    // This avoids PCM discontinuity from mixing partial data with silence.
    if (available < count) {
        memset(data, 0, count * sizeof(int16_t));
        ESP_LOGE(TAG, "Ring buffer underflow: requested %zu samples, available %zu", count, available);
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        data[i] = buffer_[r];
        r = (r + 1) % capacity_;
    }

    read_pos_.store(r, std::memory_order_release);
    return count;
}

size_t LockFreeRingBuffer::Available() const {
    size_t w = write_pos_.load(std::memory_order_acquire);
    size_t r = read_pos_.load(std::memory_order_acquire);
    return (w >= r) ? (w - r) : (capacity_ - r + w);
}

void LockFreeRingBuffer::Reset() {
    write_pos_.store(0, std::memory_order_relaxed);
    read_pos_.store(0, std::memory_order_relaxed);
}
