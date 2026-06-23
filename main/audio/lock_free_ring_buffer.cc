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

    size_t w = write_pos_.load(std::memory_order_relaxed);
    size_t r = read_pos_.load(std::memory_order_acquire);

    for (size_t i = 0; i < count; i++) {
        buffer_[w] = data[i];
        w = (w + 1) % capacity_;

        // If write catches up to read, advance read (discard oldest)
        if (w == r) {
            r = (r + 1) % capacity_;
        }
    }

    write_pos_.store(w, std::memory_order_release);
    // If we overwrote data, update read position
    // Check if we wrapped around and need to push read forward
    size_t current_r = read_pos_.load(std::memory_order_relaxed);
    size_t avail = (w >= current_r) ? (w - current_r) : (capacity_ - current_r + w);
    if (avail > capacity_ - 1) {
        // This shouldn't happen with per-sample advance above, but safety check
        read_pos_.store((w + 1) % capacity_, std::memory_order_release);
    }
}

size_t LockFreeRingBuffer::Read(int16_t* data, size_t count) {
    if (!buffer_ || count == 0) return 0;

    size_t r = read_pos_.load(std::memory_order_relaxed);
    size_t w = write_pos_.load(std::memory_order_acquire);

    size_t available = (w >= r) ? (w - r) : (capacity_ - r + w);
    size_t to_read = std::min(count, available);

    for (size_t i = 0; i < to_read; i++) {
        data[i] = buffer_[r];
        r = (r + 1) % capacity_;
    }

    // Zero-fill remaining if not enough data
    if (to_read < count) {
        memset(data + to_read, 0, (count - to_read) * sizeof(int16_t));
    }

    read_pos_.store(r, std::memory_order_release);
    return to_read;
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
