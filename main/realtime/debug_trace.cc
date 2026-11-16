#include "debug_trace.h"

#include <esp_app_desc.h>
#include <esp_heap_caps.h>
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
#if CONFIG_SPIRAM
constexpr size_t kMaxQueuedBytes = 512 * 1024;
constexpr uint32_t kFrameCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
#else
constexpr size_t kMaxQueuedBytes = 32 * 1024;
constexpr uint32_t kFrameCaps = MALLOC_CAP_8BIT;
#endif
constexpr size_t kMaxQueuedFrames = 256;
constexpr size_t kChunkSize = 8192;
constexpr uint32_t kTaskStackBytes = 8192;

void Write16(uint8_t* bytes, uint16_t value) {
    bytes[0] = value >> 8;
    bytes[1] = value;
}

void Write32(uint8_t* bytes, uint32_t value) {
    bytes[0] = value >> 24;
    bytes[1] = value >> 16;
    bytes[2] = value >> 8;
    bytes[3] = value;
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
        if (config.debug_input_pcm && config.debug_level >= 2) {
            raw_queue_ = xQueueCreate(8, sizeof(RawBlock));
            if (raw_queue_ && xTaskCreate(RawTask, "rt_raw", 4096, this, 1, nullptr) == pdPASS)
                input_enabled_.store(true);
        }
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
        raw_generation_.fetch_add(1);
        trace_id_ = RandomId();
        mode_ = mode;
    }
    Queue(2, "session_open", "", nullptr, 0);
    trace_active_.store(true);
}

void DebugTrace::SetMode(const char* mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = mode;
}

void DebugTrace::Close() {
    if (!enabled_)
        return;
    trace_active_.store(false);
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
                       size_t total_length, const char* message_id, uint64_t sample_index,
                       int64_t sample_start_us, uint32_t raw_generation) {
    if (length > kChunkSize || (length && !payload))
        return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_ || trace_id_.empty() || (kind == 14 && raw_generation != raw_generation_.load()))
        return;
    cJSON* meta = cJSON_CreateObject();
    if (!meta) {
        ++allocation_drops_;
        return;
    }
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
    if (sample_start_us > 0) {
        char index[24];
        snprintf(index, sizeof(index), "%llu", static_cast<unsigned long long>(sample_index));
        cJSON_AddStringToObject(meta, "sample_index", index);
        cJSON_AddNumberToObject(meta, "sample_start_us", static_cast<double>(sample_start_us));
    }
    if (part_count > 1) {
        cJSON_AddNumberToObject(meta, "part_index", part_index);
        cJSON_AddNumberToObject(meta, "part_count", part_count);
        cJSON_AddNumberToObject(meta, "total_length", total_length);
    }
    char* json = cJSON_PrintUnformatted(meta);
    if (!json) {
        ++allocation_drops_;
        cJSON_Delete(meta);
        return;
    }
    size_t meta_length = strlen(json);
    if (meta_length > 4096) {
        cJSON_free(json);
        cJSON_Delete(meta);
        return;
    }
    // Bound memory BEFORE allocating/copying payloads. Diagnostic frames must
    // never throw through the WebSocket/audio callback when memory is scarce.
    size_t frame_size = 12 + meta_length + length;
    if (allocation_drops_ || queued_bytes_ + sizeof(Frame) + frame_size > kMaxQueuedBytes ||
        queued_frames_ >= kMaxQueuedFrames) {
        cJSON_free(json);
        char detail_buffer[96];
        snprintf(detail_buffer, sizeof(detail_buffer),
                 "trace queue overflow or allocation failure; allocation_drops=%lu",
                 static_cast<unsigned long>(allocation_drops_));
        cJSON_ReplaceItemInObjectCaseSensitive(meta, "event_type", cJSON_CreateString("trace_gap"));
        if (cJSON_GetObjectItemCaseSensitive(meta, "detail")) {
            cJSON_ReplaceItemInObjectCaseSensitive(meta, "detail",
                                                   cJSON_CreateString(detail_buffer));
        } else {
            cJSON_AddStringToObject(meta, "detail", detail_buffer);
        }
        cJSON_DeleteItemFromObjectCaseSensitive(meta, "part_index");
        cJSON_DeleteItemFromObjectCaseSensitive(meta, "part_count");
        cJSON_DeleteItemFromObjectCaseSensitive(meta, "total_length");
        json = cJSON_PrintUnformatted(meta);
        if (!json) {
            ++allocation_drops_;
            cJSON_Delete(meta);
            return;
        }
        meta_length = strlen(json);
        kind = 10;
        length = 0;
        frame_size = 12 + meta_length;
        while (queue_head_ && (queued_bytes_ + sizeof(Frame) + frame_size > kMaxQueuedBytes ||
                               queued_frames_ >= kMaxQueuedFrames)) {
            DropFront();
        }
    }
    cJSON_Delete(meta);
    auto* frame = static_cast<Frame*>(heap_caps_malloc(sizeof(Frame) + frame_size, kFrameCaps));
    if (!frame) {
        cJSON_free(json);
        ++allocation_drops_;
        const int64_t now = esp_timer_get_time();
        if (now - last_allocation_warning_us_ >= 5000000) {
            last_allocation_warning_us_ = now;
            ESP_LOGW(kTag, "Dropping trace frame: allocation failed, size=%u queued=%u",
                     static_cast<unsigned>(frame_size), static_cast<unsigned>(queued_bytes_));
        }
        return;
    }
    frame->next = nullptr;
    frame->length = frame_size;
    Write32(frame->Data(), frame_size - 4);
    Write16(frame->Data() + 4, 1);
    Write16(frame->Data() + 6, kind);
    Write32(frame->Data() + 8, meta_length);
    memcpy(frame->Data() + 12, json, meta_length);
    cJSON_free(json);
    if (length)
        memcpy(frame->Data() + 12 + meta_length, payload, length);
    if (queue_tail_)
        queue_tail_->next = frame;
    else
        queue_head_ = frame;
    queue_tail_ = frame;
    queued_bytes_ += sizeof(Frame) + frame_size;
    ++queued_frames_;
    allocation_drops_ = 0;
}

void DebugTrace::DropFront() {
    Frame* frame = queue_head_;
    queue_head_ = frame->next;
    if (!queue_head_)
        queue_tail_ = nullptr;
    queued_bytes_ -= sizeof(Frame) + frame->length;
    --queued_frames_;
    heap_caps_free(frame);
}

bool DebugTrace::SendFrame(int socket, const uint8_t* data, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        int sent = send(socket, data + offset, length - offset, 0);
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
            cJSON_AddStringToObject(meta, "firmware_version", esp_app_get_description()->version);
            char* json = cJSON_PrintUnformatted(meta);
            cJSON_Delete(meta);
            if (json) {
                size_t size = strlen(json);
                uint8_t hello[12];
                Write32(hello, 8 + size);
                Write16(hello + 4, 1);
                Write16(hello + 6, 1);
                Write32(hello + 8, size);
                const bool hello_sent =
                    SendFrame(socket, hello, sizeof(hello)) &&
                    SendFrame(socket, reinterpret_cast<const uint8_t*>(json), size);
                cJSON_free(json);
                if (hello_sent) {
                    for (;;) {
                        Frame* frame = nullptr;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            if (queue_head_) {
                                frame = queue_head_;
                                queue_head_ = frame->next;
                                if (!queue_head_)
                                    queue_tail_ = nullptr;
                                queued_bytes_ -= sizeof(Frame) + frame->length;
                                --queued_frames_;
                            }
                        }
                        if (!frame) {
                            vTaskDelay(pdMS_TO_TICKS(30));
                            continue;
                        }
                        const bool sent = SendFrame(socket, frame->Data(), frame->length);
                        heap_caps_free(frame);
                        if (!sent) {
                            Queue(10, "trace_gap", "diagnostic socket write failed", nullptr, 0);
                            break;
                        }
                    }
                }
            }
        }
        ::close(socket);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

std::string DebugTrace::BootId() {
    std::lock_guard<std::mutex> lock(mutex_);
    return boot_id_;
}
std::string DebugTrace::TraceId() {
    std::lock_guard<std::mutex> lock(mutex_);
    return trace_id_;
}

void DebugTrace::PcmInput(const int16_t* data, size_t samples, int sample_rate) {
    if (!IsInputPcmEnabled() || !data || !samples || sample_rate <= 0)
        return;
    const int64_t start =
        esp_timer_get_time() - static_cast<int64_t>(samples) * 1000000 / sample_rate;
    for (size_t offset = 0; offset < samples;) {
        RawBlock block;
        block.generation = raw_generation_.load();
        block.rate = sample_rate;
        block.samples = std::min(samples - offset, size_t(1024));
        block.index = raw_index_;
        block.start_us = start + static_cast<int64_t>(offset) * 1000000 / sample_rate;
        std::copy_n(data + offset, block.samples, block.data);
        if (xQueueSend(raw_queue_, &block, 0) != pdTRUE)
            raw_dropped_.fetch_add(1);
        offset += block.samples;
        raw_index_ += block.samples;
    }
}

void DebugTrace::RawTask(void* arg) {
    auto* trace = static_cast<DebugTrace*>(arg);
    RawBlock block;
    while (true) {
        if (xQueueReceive(trace->raw_queue_, &block, portMAX_DELAY) != pdTRUE)
            continue;
        if (!trace->trace_active_.load() || block.generation != trace->raw_generation_.load())
            continue;
        const auto dropped = trace->raw_dropped_.exchange(0);
        if (dropped) {
            char detail[64];
            snprintf(detail, sizeof(detail), "raw_dropped_blocks=%lu",
                     static_cast<unsigned long>(dropped));
            trace->Queue(10, "trace_gap", detail, nullptr, 0);
        }
        trace->Queue(14, "pcm_input", "ES8311 before software processing",
                     reinterpret_cast<const uint8_t*>(block.data), block.samples * sizeof(int16_t),
                     block.rate, 0, 0, 0, "", block.index, block.start_us, block.generation);
    }
}

void DebugTrace::Gap(const char* detail) { Queue(10, "trace_gap", detail, nullptr, 0); }
