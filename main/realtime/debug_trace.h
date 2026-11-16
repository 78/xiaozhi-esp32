#ifndef ALIYUN_REALTIME_DEBUG_TRACE_H
#define ALIYUN_REALTIME_DEBUG_TRACE_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

#include "config.h"

class DebugTrace {
public:
    static DebugTrace& Instance();
    void Configure(const AliyunRealtimeConfig& config);
    void Open(const char* mode);
    void SetMode(const char* mode);
    void Close();
    void Gap(const char* detail);
    void Event(const char* type, const char* detail = "");
    void WebSocketTx(const char* data, size_t length, bool success);
    void WebSocketRx(const char* data, size_t length);
    bool IsInputPcmEnabled() const { return input_enabled_.load() && trace_active_.load(); }
    void PcmInput(const int16_t* data, size_t samples, int sample_rate);
    std::string BootId();
    std::string TraceId();
    void Pcm(bool uplink, const uint8_t* data, size_t length, int sample_rate);

private:
    struct Frame {
        Frame* next;
        size_t length;
        uint8_t* Data() { return reinterpret_cast<uint8_t*>(this + 1); }
    };
    struct RawBlock {
        uint32_t generation;
        int rate;
        size_t samples;
        uint64_t index;
        int64_t start_us;
        int16_t data[1024];
    };
    QueueHandle_t raw_queue_ = nullptr;
    std::atomic<bool> input_enabled_{false};
    std::atomic<bool> trace_active_{false};
    std::atomic<uint32_t> raw_dropped_{0};
    std::atomic<uint32_t> raw_generation_{0};
    uint64_t raw_index_ = 0;
    static void RawTask(void* arg);
    DebugTrace();
    static void RunTask(void* arg);
    void Run();
    void Queue(uint16_t kind, const char* type, const char* detail, const uint8_t* payload,
               size_t length, int sample_rate = 0, int part_index = 0, int part_count = 0,
               size_t total_length = 0, const char* message_id = "", uint64_t sample_index = 0,
               int64_t sample_start_us = 0, uint32_t raw_generation = 0);
    bool SendFrame(int socket, const uint8_t* data, size_t length);
    void DropFront();  // mutex_ must be held.

    std::mutex mutex_;
    Frame* queue_head_ = nullptr;
    Frame* queue_tail_ = nullptr;
    size_t queued_frames_ = 0;
    uint32_t allocation_drops_ = 0;
    int64_t last_allocation_warning_us_ = 0;
    std::string address_;
    std::string boot_id_;
    std::string trace_id_;
    std::string mode_;
    uint64_t seq_ = 0;
    size_t queued_bytes_ = 0;
    int level_ = 0;
    bool enabled_ = false;
    bool started_ = false;
};

#endif
