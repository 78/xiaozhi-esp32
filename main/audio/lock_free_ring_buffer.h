#ifndef LOCK_FREE_RING_BUFFER_H
#define LOCK_FREE_RING_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <atomic>

/**
 * Lock-free single-producer single-consumer (SPSC) ring buffer for int16_t samples.
 * 
 * - Producer: writes samples (e.g., OnAudioData callback thread)
 * - Consumer: reads samples (e.g., SendAudio main/protocol thread)
 * 
 * Uses atomic load/store with acquire/release ordering for thread safety
 * without mutexes. When the buffer is full, oldest data is overwritten.
 * 
 * Memory is allocated from PSRAM (MALLOC_CAP_SPIRAM).
 */
class LockFreeRingBuffer {
public:
    /**
     * Create a ring buffer with the given capacity in samples.
     * @param capacity Number of int16_t samples the buffer can hold.
     */
    explicit LockFreeRingBuffer(size_t capacity);
    ~LockFreeRingBuffer();

    // Non-copyable, non-movable
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;

    /**
     * Write samples into the ring buffer (producer side).
     * If the buffer is full, oldest samples are silently discarded.
     * @param data Pointer to samples to write.
     * @param count Number of samples to write.
     */
    void Write(const int16_t* data, size_t count);

    /**
     * Read samples from the ring buffer (consumer side).
     * If fewer samples are available than requested, the output is zero-padded.
     * @param data Output buffer to read into.
     * @param count Number of samples requested.
     * @return Number of samples actually read (rest is zero-filled).
     */
    size_t Read(int16_t* data, size_t count);

    /**
     * Get the number of samples currently available for reading.
     */
    size_t Available() const;

    /**
     * Reset the buffer (discard all data). Not thread-safe — call only when idle.
     */
    void Reset();

    /**
     * Check if buffer was allocated successfully.
     */
    bool IsValid() const { return buffer_ != nullptr; }

private:
    int16_t* buffer_;
    size_t capacity_;
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};
};

#endif // LOCK_FREE_RING_BUFFER_H
