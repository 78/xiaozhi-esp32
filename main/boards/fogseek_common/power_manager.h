#ifndef _FOGSEEK_POWER_MANAGER_H_
#define _FOGSEEK_POWER_MANAGER_H_

#include "boards/common/adc_battery_monitor.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <functional>

// 电源管理引脚配置结构体
typedef struct
{
    int hold_gpio;
    int charging_gpio;
    int charge_done_gpio;
    int adc_gpio;  // 添加ADC引脚
} power_pin_config_t;

class FogSeekPowerManager
{
public:
    // 电源状态枚举 (强类型枚举)
    enum class PowerState
    {
        USB_POWER_CHARGING,   // USB供电充电中
        USB_POWER_DONE,       // USB供电充电完成
        USB_POWER_NO_BATTERY, // USB供电无电池
        BATTERY_POWER,        // 电池供电
        LOW_BATTERY,          // 低电量状态
        NO_POWER              // 无电源
    };

    // 电源状态变化回调函数类型
    using PowerStateCallback = std::function<void(PowerState)>;

    FogSeekPowerManager();
    ~FogSeekPowerManager();

    // 初始化电源管理器
    void Initialize(const power_pin_config_t *pin_config);

    // 电源控制
    void PowerOn();
    void PowerOff();

    // 状态查询
    bool IsPowerOn() const { return pwr_hold_state_; }
    PowerState GetPowerState() const { return power_state_; }
    bool IsBatteryPowered() const { return power_state_ == PowerState::BATTERY_POWER ||
                                           power_state_ == PowerState::LOW_BATTERY; }
    bool IsUsbPowered() const
    {
        return power_state_ == PowerState::USB_POWER_CHARGING ||
               power_state_ == PowerState::USB_POWER_DONE ||
               power_state_ == PowerState::USB_POWER_NO_BATTERY;
    }

    // 电池相关
    uint8_t ReadBatteryLevel();

    // 回调设置
    void SetPowerStateCallback(PowerStateCallback callback) { power_state_callback_ = callback; }

private:
    // 私有成员变量
    static const char *TAG;                                    // 日志标签
    bool pwr_hold_state_ = false;                              // 电源保持状态
    PowerState power_state_ = PowerState::NO_POWER;            // 电源状态
    bool low_battery_warning_ = false;                         // 低电量警告标志
    bool low_battery_shutdown_ = false;                        // 低电量关机标志
    uint8_t battery_level_ = 0;                                // 电池电量百分比
    esp_timer_handle_t battery_check_timer_ = nullptr;         // 电池检查定时器
    AdcBatteryMonitor *battery_monitor_ = nullptr;             // 电池监控器
    PowerStateCallback power_state_callback_ = nullptr;        // 电源状态回调函数
    power_pin_config_t pin_config_ = {};                       // 引脚配置

    // 私有方法
    void UpdatePowerState();                                   // 更新电源状态
    void CheckLowBattery();                                    // 低电量检查
    static void BatteryCheckTimerCallback(void *arg);          // 电池检查定时器回调
};

#endif