#pragma once

#include "led/gpio_led.h"
#include <esp_timer.h>
#include <mutex>
#include <atomic>

class PwmLedController {
public:
    explicit PwmLedController(GpioLed* led);
    ~PwmLedController();

    bool IsReady() const;

    // 亮度按百分比 0-100 设置
    void SetBrightnessPercent(uint8_t percent);

    // 开/关
    void TurnOn();
    void TurnOff();

    // 闪烁一次（默认 100ms 周期）
    void BlinkOnce(int interval_ms = 100);

    // 持续闪烁（interval_ms 周期，毫秒）
    void StartContinuousBlink(int interval_ms);

    // 停止闪烁（若在闪烁）
    void StopBlink();

    void StartBreathing(int interval_ms = 30, uint8_t min_percent = 5, uint8_t max_percent = 100);
    void StopBreathing();
    bool IsBreathing() const;

    // 查询最近设置的亮度（百分比）
    uint8_t LastBrightnessPercent() const;

private:
    GpioLed* led_ = nullptr;
    std::mutex mutex_;
    esp_timer_handle_t blink_timer_ = nullptr;
    std::atomic<bool> blinking_{false};
    std::atomic<int> blink_counter_{0}; // >0 表示一次闪烁剩余次数，-1 表示持续闪烁
    std::atomic<int> interval_ms_{100};
    std::atomic<bool> on_phase_{true};  // 交替 on/off
    std::atomic<uint8_t> last_brightness_{50};

    static void BlinkTimerCallback(void* arg);
    void HandleBlinkTick();

    esp_timer_handle_t breath_timer_ = nullptr;
    std::atomic<bool> breathing_{false};
    std::atomic<int> breath_interval_ms_{30};
    std::atomic<uint8_t> breath_min_{5};
    std::atomic<uint8_t> breath_max_{100};
    std::atomic<bool> breath_up_{true};
    std::atomic<uint8_t> breath_current_{5};
    static void BreathTimerCallback(void* arg);
    void HandleBreathTick();
};
