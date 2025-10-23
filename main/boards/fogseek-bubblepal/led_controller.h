#ifndef _LED_CONTROLLER_H_
#define _LED_CONTROLLER_H_

#include "config.h"
#include "device_state.h"
#include "power_manager.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>

class LedController
{
public:
    LedController();
    ~LedController();

    // 初始化LED GPIO
    void InitializeLeds(PowerManager& power_manager);

    // 控制LED状态
    void SetLedState(bool red, bool green);
    void SetPowerState(bool is_on) { is_power_on_ = is_on; }

    // 闪烁控制
    void StartBlink(int interval_ms, bool red, bool green);
    void StopBlink();
    
    // 设备状态处理
    void HandleDeviceState(DeviceState current_state, PowerManager& power_manager);
    
    // 电池状态更新
    void UpdateBatteryStatus(PowerManager& power_manager);
    
    // RGB LED控制
    void SetRgbColor(uint8_t r, uint8_t g, uint8_t b);
    void TurnOnRgb();
    void TurnOffRgb();
    
    // 定时器回调函数
    static void BlinkTimerCallback(void *arg);

private:
    static constexpr const char *TAG = "LedController";

    esp_timer_handle_t led_blink_timer_ = nullptr;
    bool red_led_state_ = false;
    bool green_led_state_ = false;
    bool is_power_on_ = false;
    
    // 闪烁控制参数
    int blink_interval_ms_ = 0;  // 闪烁间隔（毫秒）
    int blink_counter_ = 0;      // 闪烁计数器
    bool blink_red_ = false;     // 闪烁时红色LED状态
    bool blink_green_ = false;   // 闪烁时绿色LED状态

    // 禁止拷贝
    LedController(const LedController &) = delete;
    LedController &operator=(const LedController &) = delete;
};

#endif