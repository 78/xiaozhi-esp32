#include "debug_trace.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <arpa/inet.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "system_info.h"

namespace {
constexpr char kTag[] = "RealtimeTrace";
#if CONFIG_SPIRAM_USE_MALLOC
constexpr size_t kMaxQueuedBytes = 512 * 1024;
#else
constexpr size_t kMaxQueuedBytes = 128 * 1024;
#endif
constexpr size_t kChunkSize = 8192;
constexpr uint32_t kTaskStackBytes = 8192;

void Write16(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(value >> 8);
    bytes.push_back(value);
}

void Write32(std::vector<uint8_t>& bytes, uint32_t value) {
    bytes.push_back(value >> 24);
    bytes.push_back(value >> 16);
    bytes.push_back(value >> 8);
    bytes.push_back(value);
}

std::string RandomId() {
    char value[17];
    snprintf(value, sizeof(value), "%08lx%08lx", static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    return value;
}
}  // namespace

DebugTrace::DebugTrace() : boot_id_(RandomId()) {}

DebugTrace& DebugTrace::Instance() {
    static DebugTrace trace;
    return trace;
}

void DebugTrace::Configure(const AliyunRealtimeConfig& config) {
    if (!config.debug_enabled || !ValidateAliyunRealtimeConfig(config))
        return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (started_)
            return;
        enabled_ = true;
        level_ = config.debug_level;
        address_ = config.debug_address;
        started_ = true;
    }
    if (xTaskCreate(RunTask, "rt_trace", kTaskStackBytes, this, 2, nullptr) != pdPASS) {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        started_ = false;
        ESP_LOGE(kTag, "Failed to start trace sender");
    }
}

void DebugTrace::Open(const char* mode) {
    if (!enabled_)
        return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        trace_id_ = RandomId();
        mode_ = mode;
    }
    Queue(2, "session_open", "", nullptr, 0);
}

void DebugTrace::SetMode(const char* mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = mode;
}

void DebugTrace::Close() {
    if (!enabled_)
        return;
    Queue(3, "session_close", "", nullptr, 0);
    std::lock_guard<std::mutex> lock(mutex_);
    trace_id_.clear();
}

void DebugTrace::Event(const char* type, const char* detail) {
    if (enabled_)
        Queue(9, type, detail, nullptr, 0);
}

void DebugTrace::WebSocketTx(const char* data, size_t length, bool success) {
    if (!enabled_)
        return;
    const std::string message_id = RandomId();
    if (level_ >= 1) {
        const size_t count = (length + kChunkSize - 1) / kChunkSize;
        for (size_t part = 0; part < count && part < 128; ++part) {
            const size_t offset = part * kChunkSize;
            Queue(5, "ws_tx", "", reinterpret_cast<const uint8_t*>(data + offset),
                  std::min(kChunkSize, length - offset), 0, part, count, length,
                  message_id.c_str());
        }
    }
    Queue(6, "ws_tx_result", success ? "ok" : "failed", nullptr, 0, 0, 0, 0, 0, message_id.c_str());
}

void DebugTrace::WebSocketRx(const char* data, size_t length) {
    if (!enabled_ || level_ < 1)
        return;
    const std::string message_id = RandomId();
    const size_t count = (length + kChunkSize - 1) / kChunkSize;
    for (size_t part = 0; part < count && part < 128; ++part) {
        const size_t offset = part * kChunkSize;
        Queue(7, "ws_rx", "", reinterpret_cast<const uint8_t*>(data + offset),
              std::min(kChunkSize, length - offset), 0, part, count, length, message_id.c_str());
    }
}

void DebugTrace::Pcm(bool uplink, const uint8_t* data, size_t length, int sample_rate) {
    if (!enabled_ || level_ < 2)
        return;
    for (size_t offset = 0; offset < length; offset += kChunkSize) {
        Queue(uplink ? 4 : 8, uplink ? "pcm_uplink" : "pcm_playback", "", data + offset,
              std::min(kChunkSize, length - offset), sample_rate);
    }
}

void DebugTrace::Queue(uint16_t kind, const char* type, const char* detail, const uint8_t* payload,
                       size_t length, int sample_rate, int part_index, int part_count,
                       size_t total_length, const char* message_id) {
    if (length > kChunkSize)
        return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_ || trace_id_.empty())
        return;
    cJSON* meta = cJSON_CreateObject();
    if (!meta)
        return;
    cJSON_AddStringToObject(meta, "boot_id", boot_id_.c_str());
    cJSON_AddStringToObject(meta, "trace_session_id", trace_id_.c_str());
    cJSON_AddStringToObject(meta, "mode", mode_.c_str());
    cJSON_AddStringToObject(meta, "event_type", type);
    if (*detail)
        cJSON_AddStringToObject(meta, "detail", detail);
    if (*message_id)
        cJSON_AddStringToObject(meta, "message_id", message_id);
    char sequence[24];
    snprintf(sequence, sizeof(sequence), "%llu", static_cast<unsigned long long>(++seq_));
    cJSON_AddStringToObject(meta, "seq", sequence);
    cJSON_AddNumberToObject(meta, "mono_us", static_cast<double>(esp_timer_get_time()));
    if (sample_rate > 0) {
        cJSON_AddNumberToObject(meta, "sample_rate", sample_rate);
        cJSON_AddNumberToObject(meta, "channels", 1);
        cJSON_AddNumberToObject(meta, "bits_per_sample", 16);
        cJSON_AddStringToObject(meta, "sample_format", "signed_int");
        cJSON_AddStringToObject(meta, "byte_order", "little");
    }
    if (part_count > 1) {
        cJSON_AddNumberToObject(meta, "part_index", part_index);
        cJSON_AddNumberToObject(meta, "part_count", part_count);
        cJSON_AddNumberToObject(meta, "total_length", total_length);
    }
    char* json = cJSON_PrintUnformatted(meta);
    if (!json) {
        cJSON_Delete(meta);
        return;
    }
    size_t meta_length = strlen(json);
    if (meta_length > 4096) {
        cJSON_free(json);
        cJSON_Delete(meta);
        return;
    }
    std::vector<uint8_t> frame;
    frame.reserve(12 + meta_length + length);
    Write32(frame, 8 + meta_length + length);
    Write16(frame, 1);
    Write16(frame, kind);
    Write32(frame, meta_length);
    frame.insert(frame.end(), json, json + meta_length);
    cJSON_free(json);
    if (length)
        frame.insert(frame.end(), payload, payload + length);
    if (queued_bytes_ + frame.size() > kMaxQueuedBytes) {
        cJSON_ReplaceItemInObjectCaseSensitive(meta, "event_type", cJSON_CreateString("trace_gap"));
        if (cJSON_GetObjectItemCaseSensitive(meta, "detail")) {
            cJSON_ReplaceItemInObjectCaseSensitive(meta, "detail",
                                                   cJSON_CreateString("trace queue overflow"));
        } else {
            cJSON_AddStringToObject(meta, "detail", "trace queue overflow");
        }
        cJSON_DeleteItemFromObjectCaseSensitive(meta, "part_index");
        cJSON_DeleteItemFromObjectCaseSensitive(meta, "part_count");
        cJSON_DeleteItemFromObjectCaseSensitive(meta, "total_length");
        json = cJSON_PrintUnformatted(meta);
        if (!json) {
            cJSON_Delete(meta);
            return;
        }
        meta_length = strlen(json);
        frame.clear();
        Write32(frame, 8 + meta_length);
        Write16(frame, 1);
        Write16(frame, 10);
        Write32(frame, meta_length);
        frame.insert(frame.end(), json, json + meta_length);
        cJSON_free(json);
        while (queued_bytes_ + frame.size() > kMaxQueuedBytes && !queue_.empty()) {
            queued_bytes_ -= queue_.front().bytes.size();
            queue_.pop_front();
        }
    }
    cJSON_Delete(meta);
    queued_bytes_ += frame.size();
    queue_.push_back({std::move(frame)});
}

bool DebugTrace::SendFrame(int socket, const std::vector<uint8_t>& frame) {
    size_t offset = 0;
    while (offset < frame.size()) {
        int sent = send(socket, frame.data() + offset, frame.size() - offset, 0);
        if (sent <= 0)
            return false;
        offset += sent;
    }
    return true;
}

void DebugTrace::RunTask(void* arg) {
    static_cast<DebugTrace*>(arg)->Run();
    vTaskDelete(nullptr);
}

void DebugTrace::Run() {
    for (;;) {
        auto host_port = address_.substr(strlen("tcp://"));
        auto colon = host_port.rfind(':');
        sockaddr_in destination{};
        destination.sin_family = AF_INET;
        destination.sin_port = htons(atoi(host_port.c_str() + colon + 1));
        inet_pton(AF_INET, host_port.substr(0, colon).c_str(), &destination.sin_addr);
        int socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (socket < 0) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        timeval timeout{.tv_sec = 2, .tv_usec = 0};
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (connect(socket, reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) == 0) {
            cJSON* meta = cJSON_CreateObject();
            cJSON_AddStringToObject(meta, "boot_id", boot_id_.c_str());
            cJSON_AddStringToObject(meta, "device_id", SystemInfo::GetMacAddress().c_str());
            cJSON_AddStringToObject(meta, "firmware_version", "dev");
            char* json = cJSON_PrintUnformatted(meta);
            cJSON_Delete(meta);
            if (json) {
                size_t size = strlen(json);
                std::vector<uint8_t> hello;
                Write32(hello, 8 + size);
                Write16(hello, 1);
                Write16(hello, 1);
                Write32(hello, size);
                hello.insert(hello.end(), json, json + size);
                cJSON_free(json);
                if (SendFrame(socket, hello)) {
                    for (;;) {
                        Frame frame;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            if (!queue_.empty()) {
                                frame.bytes.swap(queue_.front().bytes);
                                queued_bytes_ -= frame.bytes.size();
                                queue_.pop_front();
                            }
                        }
                        if (frame.bytes.empty()) {
                            vTaskDelay(pdMS_TO_TICKS(30));
                            continue;
                        }
                        const bool sent = SendFrame(socket, frame.bytes);
                        if (!sent)
                            break;
                    }
                }
            }
        }
        ::close(socket);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
