#ifndef _POWER_MANAGER_H_
#define _POWER_MANAGER_H_

#include "config.h"
#include "adc_battery_monitor.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <functional>

class PowerManager
{
public:
    // 电源状态枚举
    enum PowerState
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

    PowerManager();
    ~PowerManager();

    // 初始化电源管理器
    void Initialize();

    // 电源控制
    void PowerOn();
    void PowerOff();

    // 状态查询
    bool IsPowerOn() const { return pwr_hold_state_; }
    PowerState GetPowerState() const { return power_state_; }
    bool IsBatteryPowered() const { return power_state_ == BATTERY_POWER; }
    uint8_t GetBatteryLevel() const { return battery_level_; } // 获取电池电量

    // 设置电源状态变化回调
    void SetPowerStateCallback(PowerStateCallback callback) { power_state_callback_ = callback; }

    // 更新电源状态
    void UpdatePowerState();

    // 低电量检查
    void CheckLowBattery();

private:
    bool pwr_hold_state_ = false;       // 电源保持状态
    PowerState power_state_ = NO_POWER; // 当前电源状态
    bool low_battery_warning_ = false;  // 低电量警告标志
    bool low_battery_shutdown_ = false; // 低电量关机标志
    uint8_t battery_level_ = 0;         // 电池电量百分比

    esp_timer_handle_t battery_check_timer_ = nullptr; // 电池检查定时器
    AdcBatteryMonitor *battery_monitor_ = nullptr;     // 电池监控器

    PowerStateCallback power_state_callback_; // 电源状态变化回调

    // 电池检查定时器回调函数
    static void BatteryCheckTimerCallback(void *arg);

    // 读取电池电量
    uint8_t ReadBatteryLevel();

    // 禁止拷贝构造和赋值
    PowerManager(const PowerManager &) = delete;
    PowerManager &operator=(const PowerManager &) = delete;
};

#endif