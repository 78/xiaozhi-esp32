#pragma once
#include <mutex>
#include <functional>
#include <driver/gpio.h>
#include <esp_log.h>

class PowerController {
public:
    enum class PowerState {
        ACTIVE,
        LIGHT_SLEEP, 
        DEEP_SLEEP,
        SHUTDOWN
    };

    static PowerController& Instance() {
        static PowerController instance;
        return instance;
    }

    void SetState(PowerState newState) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentState_ != newState) {
            ESP_LOGI("PowerCtrl", "State change: %d -> %d", 
                    static_cast<int>(currentState_), 
                    static_cast<int>(newState));
            
            currentState_ = newState;
            if (stateChangeCallback_) {
                stateChangeCallback_(newState);
            }
        }
    }

    PowerState GetState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentState_;
    }

    void OnStateChange(std::function<void(PowerState)> callback) {
        stateChangeCallback_ = callback;
    }

private:
    PowerController() = default;
    ~PowerController() = default;

    PowerState currentState_ = PowerState::ACTIVE;
    std::function<void(PowerState)> stateChangeCallback_;
    mutable std::mutex mutex_;
};