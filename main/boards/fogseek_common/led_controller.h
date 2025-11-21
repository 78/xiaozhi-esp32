#ifndef _FOGSEEK_LED_CONTROLLER_H_
#define _FOGSEEK_LED_CONTROLLER_H_

#include "device_state.h"
#include <driver/gpio.h>
#include <esp_timer.h>

// 添加对GpioLed的支持
#include "led/gpio_led.h"

/**
 * @file led_controller.h
 * @brief FogSeek设备LED控制器
 *
 * 该模块负责管理设备上的LED指示灯（包括独立红/绿LED和RGB LED）。
 * 具体电平行为由底层驱动和硬件配置决定。
 */

// LED引脚配置结构体
typedef struct
{
    int red_gpio;
    int green_gpio;
    int rgb_gpio = -1;        // RGB灯带GPIO，默认为-1表示不使用
    int cold_light_gpio = -1; // 冷色灯GPIO，默认为-1表示不使用
    int warm_light_gpio = -1; // 暖色灯GPIO，默认为-1表示不使用
} led_pin_config_t;

// 前向声明
class FogSeekPowerManager;

class FogSeekLedController
{
public:
    FogSeekLedController();
    ~FogSeekLedController();

    // 初始化LED GPIO
    void InitializeLeds(FogSeekPowerManager &power_manager, const led_pin_config_t *pin_config);

    // 特殊功能LED初始化（可选）
    void InitializeColdWarmLeds(int cold_gpio, int warm_gpio);

    // 控制LED状态
    void SetLedState(bool red, bool green);
    void SetPowerState(bool is_on) { is_power_on_ = is_on; }
    void SetPrePowerOnState(bool is_pre_power_on) { is_pre_power_on_ = is_pre_power_on; }

    // 闪烁控制
    void StartBlink(int interval_ms, bool red, bool green);
    void StopBlink();

    // 设备状态处理
    void HandleDeviceState(DeviceState current_state, FogSeekPowerManager &power_manager);

    // 电池状态更新
    void UpdateBatteryStatus(FogSeekPowerManager &power_manager);

    // 冷暖色灯控制
    void SetColdLight(bool state);
    void SetWarmLight(bool state);
    void SetColdLightBrightness(int brightness);
    void SetWarmLightBrightness(int brightness);
    bool IsColdLightOn() const { return cold_light_state_; }
    bool IsWarmLightOn() const { return warm_light_state_; }

    // 添加获取冷暖灯实例的方法，用于MCP工具初始化
    GpioLed *GetColdLight() const { return cold_light_; }
    GpioLed *GetWarmLight() const { return warm_light_; }

    // 定时器回调函数
    static void BlinkTimerCallback(void *arg);

protected:
    static const char *TAG;

    esp_timer_handle_t led_blink_timer_ = nullptr;
    bool red_led_state_ = false;
    bool green_led_state_ = false;
    bool is_power_on_ = false;
    bool is_pre_power_on_ = false; // 预开机状态标志位

    // 闪烁控制参数
    int blink_interval_ms_ = 0; // 闪烁间隔（毫秒）
    int blink_counter_ = 0;     // 闪烁计数器
    bool blink_red_ = false;    // 闪烁时红色LED状态
    bool blink_green_ = false;  // 闪烁时绿色LED状态

    // 冷暖色灯控制
    GpioLed *cold_light_ = nullptr;
    GpioLed *warm_light_ = nullptr;
    bool cold_light_state_ = false;
    bool warm_light_state_ = false;

    // LED引脚配置
    led_pin_config_t pin_config_;

    // 禁止拷贝
    FogSeekLedController(const FogSeekLedController &) = delete;
    FogSeekLedController &operator=(const FogSeekLedController &) = delete;
};

#endif