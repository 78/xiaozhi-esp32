#ifndef _STEPPER_POLLING_H_
#define _STEPPER_POLLING_H_

#include <cstdint>
#include <string>
#include <atomic>
#include <functional>
#include <esp_timer.h>
#include <memory>

class Mqtt;  // Forward declaration

/**
 * Stepper polling module for monitoring step count changes via MQTT
 * and triggering voice announcements
 */
class StepperPolling {
public:
    static StepperPolling& GetInstance() {
        static StepperPolling instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    StepperPolling(const StepperPolling&) = delete;
    StepperPolling& operator=(const StepperPolling&) = delete;

    /**
     * Handle incoming MQTT message from stepper/count topic
     * Expected format: plain integer string (e.g., "50")
     */
    void OnMqttMessage(const std::string& payload);

    /**
     * Check if step count change should trigger announcement
     * Called periodically from main application loop
     * Returns true if trigger condition met
     */
    bool CheckAndTrigger();

    /**
     * Get current step count (thread-safe)
     */
    int32_t GetCurrentCount() const { return current_count_.load(); }

    /**
     * Get last triggered step count (thread-safe)
     */
    int32_t GetLastTriggeredCount() const { return last_triggered_count_.load(); }

    /**
     * Register callback to be invoked when step count change triggers
     * Called with (step_count, delta)
     */
    void RegisterTriggerCallback(
        std::function<void(int32_t, int32_t)> callback) {
        trigger_callback_ = callback;
    }

    /**
     * Reset state for testing or reinitialization
     */
    void Reset();

    /**
     * Initialize HiveMQ MQTT connection (call after network is ready)
     */
    void InitializeHiveMqttAsync();

private:
    StepperPolling();
    ~StepperPolling();

    static constexpr const char* TAG = "StepperPolling";
    static constexpr int32_t MIN_DELTA = 5;          // Minimum step change to trigger
    static constexpr uint32_t DEBOUNCE_MS = 100;     // Debounce window in milliseconds

    std::atomic<int32_t> current_count_{0};           // Latest received step count
    std::atomic<int32_t> last_triggered_count_{0};    // Last count that triggered
    int64_t last_update_time_us_{0};                  // Last message timestamp
    int64_t last_trigger_time_us_{0};                 // Last trigger timestamp

    std::function<void(int32_t, int32_t)> trigger_callback_;

    // HiveMQ independent MQTT connection
    std::unique_ptr<Mqtt> hivemq_mqtt_;
};

#endif // _STEPPER_POLLING_H_
