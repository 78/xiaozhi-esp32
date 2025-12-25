#ifndef _FOGSEEK_LED_CONTROLLER_H_
#define _FOGSEEK_LED_CONTROLLER_H_

#include <driver/gpio.h>
#include <esp_timer.h>

#include "device_state.h"
#include "led/gpio_led.h"
#include "led/led.h"
#include "power_manager.h"

#include <memory>

// 前向声明FogSeekLedController类，以便在GreenLed中使用
class FogSeekLedController;

// LED引脚配置结构体
typedef struct
{
    int red_gpio;             // 红色LED GPIO引脚
    int green_gpio;           // 绿色LED GPIO引脚
    int rgb_gpio = -1;        // RGB灯带GPIO，默认为-1表示不使用
    int cold_light_gpio = -1; // 冷色灯GPIO，默认为-1表示不使用
    int warm_light_gpio = -1; // 暖色灯GPIO，默认为-1表示不使用
} led_pin_config_t;

/**
 * @class RedLed
 * @brief 红色LED类，负责电源状态显示
 */
class RedLed : public GpioLed
{
public:
    RedLed(gpio_num_t gpio);
    RedLed(gpio_num_t gpio, int output_invert, ledc_timer_t timer_num, ledc_channel_t channel);
    ~RedLed();

    // 空实现，因为红灯不响应设备状态变化
    void OnStateChanged() override;

    // 响应电源状态变化
    void UpdateBatteryStatus(FogSeekPowerManager::PowerState state);

private:
    static const char *TAG;
};

/**
 * @class GreenLed
 * @brief 绿色LED类，负责设备状态显示
 */
class GreenLed : public GpioLed
{
public:
    GreenLed(gpio_num_t gpio);
    GreenLed(gpio_num_t gpio, int output_invert, ledc_timer_t timer_num, ledc_channel_t channel);
    ~GreenLed();

    // 响应设备状态变化
    void OnStateChanged() override;

    // 设置是否忽略设备状态变化
    void SetIgnoreStateChanges(bool ignore) { ignore_state_changes_ = ignore; }
    bool IsIgnoringStateChanges() const { return ignore_state_changes_; }

private:
    static const char *TAG;
    bool ignore_state_changes_ = false; // 是否忽略设备状态变化
};

/**
 * @class FogSeekLedController
 * @brief 雾岸设备LED控制器类
 *
 * 该类是LED系统的主控制器，负责管理红绿灯和其他LED设备。
 * 内部使用RedLed和GreenLed类分别控制红灯和绿灯，这些是
 * 内部实现细节，外部代码应通过本类的公共接口进行操作。
 */
class FogSeekLedController
{
public:
    FogSeekLedController();
    ~FogSeekLedController();

    // 初始化LED GPIO
    void InitializeLeds(FogSeekPowerManager &power_manager, const led_pin_config_t *pin_config);

    // 更新LED状态
    void UpdateLedStatus(FogSeekPowerManager &power_manager);

    // 冷暖色灯控制
    void SetColdLight(bool state);
    void SetWarmLight(bool state);
    void SetColdLightBrightness(int brightness);
    void SetWarmLightBrightness(int brightness);
    bool IsColdLightOn() const { return cold_light_state_; }
    bool IsWarmLightOn() const { return warm_light_state_; }

    // 获取LED实例的方法
    RedLed *GetRedLed() const { return red_led_; }
    GreenLed *GetGreenLed() const { return green_led_; }
    GpioLed *GetColdLight() const { return cold_light_; }
    GpioLed *GetWarmLight() const { return warm_light_; }

private:
    static const char *TAG; // 日志标签

    bool red_led_state_ = false;   // 红色LED当前状态
    bool green_led_state_ = false; // 绿色LED当前状态

    RedLed *red_led_ = nullptr;     // 红色LED控制器实例
    GreenLed *green_led_ = nullptr; // 绿色LED控制器实例

    // 冷暖色灯控制
    GpioLed *cold_light_ = nullptr; // 冷色灯控制器实例
    GpioLed *warm_light_ = nullptr; // 暖色灯控制器实例
    bool cold_light_state_ = false; // 冷色灯当前状态
    bool warm_light_state_ = false; // 暖色灯当前状态

    led_pin_config_t pin_config_; // LED引脚配置

    // 禁止拷贝构造和赋值操作
    FogSeekLedController(const FogSeekLedController &) = delete;
    FogSeekLedController &operator=(const FogSeekLedController &) = delete;
};

#endif