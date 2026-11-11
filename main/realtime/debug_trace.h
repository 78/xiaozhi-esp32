#ifndef ALIYUN_REALTIME_DEBUG_TRACE_H
#define ALIYUN_REALTIME_DEBUG_TRACE_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "config.h"

class DebugTrace {
public:
    static DebugTrace& Instance();
    void Configure(const AliyunRealtimeConfig& config);
    void Open(const char* mode);
    void SetMode(const char* mode);
    void Close();
    void Event(const char* type, const char* detail = "");
    void WebSocketTx(const char* data, size_t length, bool success);
    void WebSocketRx(const char* data, size_t length);
    void Pcm(bool uplink, const uint8_t* data, size_t length, int sample_rate);

private:
    struct Frame {
        std::vector<uint8_t> bytes;
    };
    DebugTrace();
    static void RunTask(void* arg);
    void Run();
    void Queue(uint16_t kind, const char* type, const char* detail, const uint8_t* payload,
               size_t length, int sample_rate = 0, int part_index = 0, int part_count = 0,
               size_t total_length = 0, const char* message_id = "");
    bool SendFrame(int socket, const std::vector<uint8_t>& frame);

    std::mutex mutex_;
    std::deque<Frame> queue_;
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
