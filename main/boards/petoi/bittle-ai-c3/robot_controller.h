#ifndef PETOI_ROBOT_CONTROLLER_H_
#define PETOI_ROBOT_CONTROLLER_H_

#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>
#include <functional>
#include <string>

#include "application.h"
#include "config.h"
#include "robot_plan.h"

namespace petoi {
enum class RobotResultCode {
    Pending,
    Transmitted,
    TokenObserved,
    Timeout,
    TxError,
    Cancelled,
    Rejected
};
struct RobotResult {
    RobotResultCode code = RobotResultCode::Pending;
    size_t completed = 0;
    size_t requested = 0;
    bool ok() const {
        return code == RobotResultCode::Transmitted || code == RobotResultCode::TokenObserved;
    }
};

inline const char* RobotResultName(RobotResultCode code) {
    switch (code) {
        case RobotResultCode::Pending: return "pending";
        case RobotResultCode::Transmitted: return "transmitted";
        case RobotResultCode::TokenObserved: return "token_observed";
        case RobotResultCode::Timeout: return "timeout";
        case RobotResultCode::TxError: return "uart_error";
        case RobotResultCode::Cancelled: return "cancelled";
        case RobotResultCode::Rejected: return "rejected";
    }
    return "unknown";
}

// Metadata and completions live only on Application's task. A single UART
// worker owns transmission/RX and posts bounded progress messages back to it.
class RobotController {
public:
    using Completion = std::function<void(RobotResult)>;
    esp_err_t Initialize() {
        uart_config_t config{};
        config.baud_rate = ROBOT_UART_BAUD_RATE;
        config.data_bits = UART_DATA_8_BITS;
        config.parity = UART_PARITY_DISABLE;
        config.stop_bits = UART_STOP_BITS_1;
        config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        config.source_clk = UART_SCLK_DEFAULT;
        auto error = uart_param_config(UART_NUM_1, &config);
        if (error != ESP_OK)
            return error;
        error = uart_set_pin(UART_NUM_1, ROBOT_UART_TX_PIN, ROBOT_UART_RX_PIN, UART_PIN_NO_CHANGE,
                             UART_PIN_NO_CHANGE);
        if (error != ESP_OK)
            return error;
        error = uart_driver_install(UART_NUM_1, 1024, 0, 0, nullptr, 0);
        if (error != ESP_OK)
            return error;
        queue_ = xQueueCreate(2, sizeof(Job));
        if (!queue_) {
            uart_driver_delete(UART_NUM_1);
            return ESP_ERR_NO_MEM;
        }
        if (xTaskCreate([](void* self) { static_cast<RobotController*>(self)->Run(); },
                        "petoi_actions", 4096, this, 2, nullptr) != pdPASS) {
            vQueueDelete(queue_);
            queue_ = nullptr;
            uart_driver_delete(UART_NUM_1);
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }

    // A new user request supersedes pending work, never replays a prior action.
    // An optional local plan is queued once; later cloud calls join its result.
    void BeginTurn(const std::string& local_text = {}) {
        const auto generation = ++generation_;
        while (queue_ && xQueueReceive(queue_, &main_job_, 0) == pdTRUE) {
        }
        std::array<Completion, kSlots> cancelled;
        for (size_t i = 0; i < slots_.size(); ++i) {
            cancelled[i] = std::move(slots_[i].completion);
            slots_[i] = Slot{};
        }
        local_ = false;
        local_cursor_ = 0;
        if (!local_text.empty() && BuildRobotPlan(local_text, local_plan_) && queue_) {
            slots_[0].used = true;
            slots_[0].root = 0;
            slots_[0].count = local_plan_.count;
            slots_[0].mask = (1u << local_plan_.count) - 1;
            slots_[0].text = PlanText(local_plan_);
            main_job_ = {local_plan_, generation, Application::GetInstance().GetMcpSessionEpoch(), 0, true};
            if (xQueueSend(queue_, &main_job_, 0) == pdTRUE) {
                local_ = true;
            } else
                slots_[0] = Slot{};
        }
        for (auto& callback : cancelled)
            if (callback)
                callback({RobotResultCode::Cancelled, 0, 0});
    }

    void Submit(const std::string& text, const std::string& request_id, bool direct_transport,
                Completion callback) {
        if (direct_transport)
            BeginTurn();
        RobotPlan plan;
        if (!queue_ || request_id.size() > 96 || !BuildRobotPlan(text, plan)) {
            callback({RobotResultCode::Rejected, 0, 0});
            return;
        }
        const auto canonical = PlanText(plan);
        size_t root = kSlots;
        unsigned mask = 0;
        for (size_t i = 0; i < slots_.size(); ++i) {
            if (!slots_[i].used)
                continue;
            if (slots_[i].id == request_id && slots_[i].text != canonical) {
                callback({RobotResultCode::Rejected, 0, plan.count});
                return;
            }
            if (!slots_[i].id.empty() &&
                (slots_[i].id == request_id || slots_[i].text == canonical)) {
                RobotResult result;
                if (ResultFor(slots_[i], result)) {
                    callback(result);
                    return;
                }
                root = slots_[i].root;
                mask = slots_[i].mask;
                break;
            }
        }
        if (root == kSlots && local_) {
            for (size_t pass = 0; pass < 2 && !mask; ++pass) {
                const size_t begin = pass == 0 ? local_cursor_ : 0;
                for (size_t i = begin; i + plan.count <= local_plan_.count; ++i) {
                    bool same = true;
                    for (size_t j = 0; j < plan.count; ++j)
                        same &= SameAction(plan.frames[j], local_plan_.frames[i + j]);
                    if (same) {
                        root = 0;
                        mask = ((1u << plan.count) - 1) << i;
                        local_cursor_ = i + plan.count;
                        break;
                    }
                }
            }
            if (!mask) {
                callback({RobotResultCode::Rejected, 0, plan.count});
                return;
            }
        }
        // Any cloud call joining a local multi-action request waits for the
        // whole plan, so its result cannot announce completion after step one.
        if (local_ && root == 0 && local_plan_.count > 1)
            mask = (1u << local_plan_.count) - 1;
        size_t index = 0;
        while (index < kSlots && slots_[index].used)
            ++index;
        if (index >= (local_ ? kSlots : kSlots - 1)) {
            callback({RobotResultCode::Rejected, 0, plan.count});
            return;
        }
        auto& slot = slots_[index];
        slot.used = true;
        slot.id = request_id;
        slot.text = canonical;
        slot.completion = std::move(callback);
        slot.root = root == kSlots ? index : root;
        slot.count = plan.count;
        slot.mask = mask ? mask : (1u << plan.count) - 1;
        if (root == kSlots) {
            Job job{plan, generation_.load(), Application::GetInstance().GetMcpSessionEpoch(),
                    static_cast<uint8_t>(index), !direct_transport};
            if (xQueueSend(queue_, &job, 0) != pdTRUE)
                for (size_t i = 0; i < plan.count; ++i)
                    slot.states[i] = RobotResultCode::Rejected;
        }
        FlushCompletions();
    }

    std::string GetStatus() const {
        return "pending=" + std::to_string(queue_ ? uxQueueMessagesWaiting(queue_) : 0) +
               "; transmitting=" + std::to_string(transmitting_.load()) +
               "; waiting_token=" + std::to_string(waiting_.load()) +
               "; action_index=" + std::to_string(action_index_.load()) +
               "; last_result=" + RobotResultName(last_result_) +
               "; UART/token observations do not independently prove physical action completion";
    }

private:
    static constexpr size_t kSlots = 5;  // One local root plus up to four deferred RPCs.
    struct Slot {
        bool used = false;
        size_t root = 0;
        size_t count = 0;
        unsigned mask = 0;
        std::string id;
        std::string text;
        Completion completion;
        std::array<RobotResultCode, kMaxActions> states{};
    };
    struct Job {
        RobotPlan plan;
        uint32_t generation;
        uint32_t epoch;
        uint8_t root;
        bool bound;
    };
    QueueHandle_t queue_ = nullptr;
    std::atomic<uint32_t> generation_{0};
    std::atomic<bool> transmitting_{false}, waiting_{false};
    std::atomic<unsigned> action_index_{0};
    std::array<Slot, kSlots> slots_{};
    RobotPlan local_plan_{};
    Job main_job_{}; // Reused only by the main-task producer, not the UART worker.
    bool local_ = false;
    size_t local_cursor_ = 0;
    RobotResultCode last_result_ = RobotResultCode::Pending;

    bool ResultFor(const Slot& slot, RobotResult& result) const {
        const auto& root = slots_[slot.root];
        result = {RobotResultCode::TokenObserved, 0, 0};
        for (size_t i = 0; i < root.count; ++i)
            if (slot.mask & (1u << i))
                ++result.requested;
        bool pending = false;
        for (size_t i = 0; i < root.count; ++i) {
            if (!(slot.mask & (1u << i)))
                continue;
            const auto code = root.states[i];
            if (code == RobotResultCode::Pending)
                pending = true;
            else if (code == RobotResultCode::Transmitted ||
                     code == RobotResultCode::TokenObserved) {
                ++result.completed;
                if (code == RobotResultCode::Transmitted)
                    result.code = code;
            } else {
                result.code = code;
                return true;
            }
        }
        return !pending;
    }
    void FlushCompletions() {
        for (auto& slot : slots_) {
            RobotResult result;
            if (slot.completion && ResultFor(slot, result)) {
                auto callback = std::move(slot.completion);
                callback(result);
            }
        }
    }
    bool Current(const Job& job) const {
        return job.generation == generation_.load() &&
               (!job.bound || job.epoch == Application::GetInstance().GetMcpSessionEpoch());
    }
    void Progress(const Job& job, size_t index, RobotResultCode result) {
        Application::GetInstance().Schedule([this, generation = job.generation, root = job.root,
                                             index, result]() {
            if (generation != generation_.load())
                return;
            auto& slot = slots_[root];
            slot.states[index] = result;
            last_result_ = result;
            ESP_LOGI("PetoiRobot", "turn=%lu action=%u/%u result=%s",
                     static_cast<unsigned long>(generation), static_cast<unsigned>(index+1),
                     static_cast<unsigned>(slot.count), RobotResultName(result));
            if (result != RobotResultCode::Transmitted && result != RobotResultCode::TokenObserved)
                for (size_t i = index + 1; i < slot.count; ++i)
                    slot.states[i] = RobotResultCode::Cancelled;
            FlushCompletions();
        });
    }
    void Run() {
        Job job;
        for (;;) {
            if (xQueueReceive(queue_, &job, pdMS_TO_TICKS(20)) != pdTRUE)
                continue;
            transmitting_ = true;
            for (size_t i = 0; i < job.plan.count; ++i) {
                action_index_ = i;
                if (!Current(job)) {
                    Progress(job, i, RobotResultCode::Cancelled);
                    break;
                }
                const auto& frame = job.plan.frames[i];
                ESP_LOGD("PetoiRobot", "TX action=%u/%u bytes=%u", static_cast<unsigned>(i + 1),
                         static_cast<unsigned>(job.plan.count),
                         static_cast<unsigned>(frame.length));
                uart_flush_input(UART_NUM_1);
                const int written = uart_write_bytes(UART_NUM_1, frame.data.data(), frame.length);
                if (written != static_cast<int>(frame.length) ||
                    uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(300)) != ESP_OK) {
                    ESP_LOGE("PetoiRobot", "UART TX failed; command is not retried");
                    Progress(job, i, RobotResultCode::TxError);
                    break;
                }
                if (job.plan.count == 1) {
                    Progress(job, i, RobotResultCode::Transmitted);
                    continue;
                }
                waiting_ = true;
                bool token_seen = false;
                const int64_t deadline =
                    esp_timer_get_time() + static_cast<int64_t>(CommandTimeoutMs(frame)) * 1000;
                while (Current(job) && esp_timer_get_time() < deadline && !token_seen) {
                    uint8_t bytes[32];
                    const int count =
                        uart_read_bytes(UART_NUM_1, bytes, sizeof(bytes), pdMS_TO_TICKS(20));
                    for (int n = 0; n < count; ++n)
                        token_seen |= bytes[n] == frame.data[3];
                }
                waiting_ = false;
                const auto result = !Current(job) ? RobotResultCode::Cancelled
                                    : token_seen  ? RobotResultCode::TokenObserved
                                                  : RobotResultCode::Timeout;
                Progress(job, i, result);
                if (result == RobotResultCode::Timeout)
                    ESP_LOGW("PetoiRobot", "Completion token timeout; remaining actions cancelled");
                if (result != RobotResultCode::TokenObserved)
                    break;
            }
            waiting_ = transmitting_ = false;
        }
    }
};
}  // namespace petoi

#endif
