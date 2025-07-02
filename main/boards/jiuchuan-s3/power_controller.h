/*
 * @FileDesc: 
 * @Author: none
 * @Date: 2025-06-08 23:39:49
 * @LastEditTime: 2025-06-27 11:23:43
 * @LastEditors: Hangon66 2630612613@qq.com
 * @Version: 0.0.1
 * @Usage: 
 * @FilePath: \jiuchuan-xiaozhi-sound\main\boards\jiuchuan-s3\power_controller.h
 */
#pragma once
#include <mutex>
#include <functional>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_log.h>
#include "config.h"
enum class PowerState {
        ACTIVE,
        LIGHT_SLEEP, 
        DEEP_SLEEP,
        SHUTDOWN
    };
    
class PowerController {
public:
    

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
    PowerController(){
        rtc_gpio_init(PWR_EN_GPIO);
        rtc_gpio_set_direction(PWR_EN_GPIO, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level(PWR_EN_GPIO, 1);
    }
    ~PowerController() = default;

    PowerState currentState_ = PowerState::ACTIVE;
    std::function<void(PowerState)> stateChangeCallback_;
    mutable std::mutex mutex_;
};