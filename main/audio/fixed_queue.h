#ifndef FIXED_QUEUE_H
#define FIXED_QUEUE_H

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include <esp_system.h>

template <typename T, size_t Capacity>
class FixedQueue {
    static_assert(Capacity > 0);

public:
    FixedQueue() = default;
    FixedQueue(const FixedQueue&) = delete;
    FixedQueue& operator=(const FixedQueue&) = delete;

    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Capacity; }
    size_t size() const { return size_; }
    static constexpr size_t capacity() { return Capacity; }

    T& front() {
        if (empty()) {
            esp_system_abort("FixedQueue::front() called on an empty queue");
        }
        return *storage_[head_];
    }

    const T& front() const {
        if (empty()) {
            esp_system_abort("FixedQueue::front() called on an empty queue");
        }
        return *storage_[head_];
    }

    bool push_back(T value) {
        if (full()) {
            return false;
        }
        storage_[(head_ + size_) % Capacity].emplace(std::move(value));
        ++size_;
        return true;
    }

    void pop_front() {
        if (empty()) {
            esp_system_abort("FixedQueue::pop_front() called on an empty queue");
        }
        storage_[head_].reset();
        head_ = (head_ + 1) % Capacity;
        --size_;
    }

    void clear() {
        while (!empty()) {
            pop_front();
        }
        head_ = 0;
    }

    void swap(FixedQueue& other) {
        storage_.swap(other.storage_);
        std::swap(head_, other.head_);
        std::swap(size_, other.size_);
    }

private:
    std::array<std::optional<T>, Capacity> storage_;
    size_t head_ = 0;
    size_t size_ = 0;
};

#endif  // FIXED_QUEUE_H
