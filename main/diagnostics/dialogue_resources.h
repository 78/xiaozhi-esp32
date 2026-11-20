#ifndef DIALOGUE_RESOURCES_H_
#define DIALOGUE_RESOURCES_H_

#include <cstdint>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "device_state.h"

// Main-task-only diagnostics. No owned buffers, timers, tasks or heap tracing.
class DialogueResources {
public:
    void BeginRequest(DeviceState state, bool channel_open) {
        if (active_) {
            Finish("superseded", state, channel_open);
        }
        ++request_id_;
        active_ = true;
        started_ = stopped_ = aborted_ = false;
        before_ = Capture();
        request_state_ = state;
        sampled_min_free_ = before_.free;
        Print("request", "pending", state, channel_open, before_, false, 0, 0);
    }

    void ResponseStarted() {
        if (active_) {
            started_ = true;
            stopped_ = false;
        }
    }

    void ResponseStopped(bool aborted) {
        if (active_ && started_) {
            stopped_ = true;
            aborted_ = aborted;
        }
    }

    // Called after scheduled work has been destroyed and state events handled.
    // A TTS stop alone is insufficient: the decoder/speaker may still be busy.
    void Poll(DeviceState state, bool playback_idle, bool channel_open, int64_t now_us) {
        if (active_ && now_us - last_sample_us_ >= 1000000) {
            last_sample_us_ = now_us;
            const auto free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (free < sampled_min_free_) {
                sampled_min_free_ = free;
            }
        }
        if (active_) {
            const bool ready = state == kDeviceStateListening || state == kDeviceStateIdle;
            if (stopped_ && playback_idle && ready) {
                Finish(aborted_ ? "interrupted" : "reply_complete", state, channel_open);
            } else if (state == kDeviceStateIdle && !channel_open && playback_idle) {
                Finish("channel_closed", state, channel_open);
            }
        }

        if (state != kDeviceStateIdle || !playback_idle || channel_open) {
            idle_since_us_ = -1;
            idle_printed_ = false;
            return;
        }
        if (idle_since_us_ < 0) {
            idle_since_us_ = now_us;
        }
        if (!idle_printed_ && now_us - idle_since_us_ >= 3000000) {
            const auto snapshot = Capture();
            Print("idle_settled", "not_a_round", state, channel_open, snapshot, have_idle_,
                  have_idle_ ? Delta(idle_.free, snapshot.free) : 0, 0);
            idle_ = snapshot;
            have_idle_ = idle_printed_ = true;
        }
    }

private:
    struct Snapshot {
        size_t free = 0;
        size_t largest = 0;
        size_t minimum = 0;
        size_t allocated = 0;
        size_t allocated_blocks = 0;
        size_t free_blocks = 0;
        size_t total_free = 0;
        size_t psram_free = 0;
        unsigned tasks = 0;
        unsigned stack_hwm = 0;
    };

    static Snapshot Capture() {
        multi_heap_info_t info{};
        heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        return {info.total_free_bytes,
                info.largest_free_block,
                info.minimum_free_bytes,
                info.total_allocated_bytes,
                info.allocated_blocks,
                info.free_blocks,
                heap_caps_get_free_size(MALLOC_CAP_8BIT),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                static_cast<unsigned>(uxTaskGetNumberOfTasks()),
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))};
    }

    static long Delta(size_t before, size_t after) {
        return static_cast<long>(before) - static_cast<long>(after);
    }

    void Print(const char* phase, const char* result, DeviceState state, bool channel_open,
               const Snapshot& s, bool comparable, long previous_used, long net_used) const {
        ESP_LOGD("DialogRes",
                 "request=%lu phase=%s result=%s request_state=%d state=%d channel_open=%d "
                 "internal_free=%u largest_internal=%u boot_min_internal=%u "
                 "internal_allocated=%u allocated_blocks=%u free_blocks=%u "
                 "total_8bit_free=%u psram_free=%u tasks=%u main_stack_hwm_bytes=%u "
                 "net_internal_used=%ld previous_comparable=%d previous_internal_used=%ld "
                 "sampled_min_internal_free=%u",
                 static_cast<unsigned long>(request_id_), phase, result,
                 static_cast<int>(request_state_), static_cast<int>(state), channel_open,
                 static_cast<unsigned>(s.free), static_cast<unsigned>(s.largest),
                 static_cast<unsigned>(s.minimum), static_cast<unsigned>(s.allocated),
                 static_cast<unsigned>(s.allocated_blocks), static_cast<unsigned>(s.free_blocks),
                 static_cast<unsigned>(s.total_free), static_cast<unsigned>(s.psram_free), s.tasks,
                 s.stack_hwm, net_used, comparable, previous_used,
                 active_ ? static_cast<unsigned>(sampled_min_free_) : 0);
    }

    void Finish(const char* result, DeviceState state, bool channel_open) {
        const auto snapshot = Capture();
        const bool complete =
            std::strcmp(result, "reply_complete") == 0 && started_ && stopped_ && !aborted_;
        const bool comparable = complete && have_previous_ && previous_state_ == state &&
                                previous_channel_open_ == channel_open;
        Print("round_end", result, state, channel_open, snapshot, comparable,
              comparable ? Delta(previous_.free, snapshot.free) : 0,
              Delta(before_.free, snapshot.free));
        // Incomplete/interrupted requests must not poison the normal-round baseline.
        if (complete) {
            previous_ = snapshot;
            previous_state_ = state;
            previous_channel_open_ = channel_open;
            have_previous_ = true;
        }
        active_ = false;
    }

    uint32_t request_id_ = 0;
    bool active_ = false;
    bool started_ = false;
    bool stopped_ = false;
    bool aborted_ = false;
    bool have_previous_ = false;
    bool previous_channel_open_ = false;
    bool have_idle_ = false;
    bool idle_printed_ = false;
    int64_t idle_since_us_ = -1;
    int64_t last_sample_us_ = 0;
    size_t sampled_min_free_ = 0;
    DeviceState request_state_ = kDeviceStateUnknown;
    DeviceState previous_state_ = kDeviceStateUnknown;
    Snapshot before_{};
    Snapshot previous_{};
    Snapshot idle_{};
};

#endif  // DIALOGUE_RESOURCES_H_
