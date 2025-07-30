#ifndef POWER_MANAGER_H_
#define POWER_MANAGER_H_

#include <functional>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_adc/adc_oneshot.h>

class PowerManager
{
private:
    gpio_num_t bat_charg_gpio_; // CHRG 引脚（充电中状态指示端）
    gpio_num_t bat_done_gpio_;  // STDBY 引脚（电池充电完成指示端）

    std::function<void(bool)> on_charging_status_changed_;
    std::function<void(bool)> on_charge_done_status_changed_;
    std::function<void(bool)> on_low_battery_status_changed_;

    std::vector<uint16_t> adc_values_;
    uint32_t battery_level_ = 0;
    bool is_charging_ = false;
    bool is_charge_done_ = false;
    bool is_low_battery_ = false;
    int ticks_ = 0;
    const int kBatteryAdcInterval = 3; // 1秒/单位
    const int kBatteryAdcDataCount = 5;
    const int kLowBatteryLevel = 20;

    esp_timer_handle_t timer_handle_;
    adc_oneshot_unit_handle_t adc_handle_;

    void CheckChargeStatus();
    void ReadBatteryAdcData();
    void CheckBatteryStatus();

    // 统一回调注册函数模板
    template <typename CallbackType>
    void RegisterCallback(CallbackType &member_callback, const CallbackType &callback)
    {
        member_callback = callback;
    }

public:
    PowerManager(gpio_num_t bat_charg_gpio, gpio_num_t bat_done_gpio);
    ~PowerManager();

    bool IsCharging();
    bool IsChargeDone();
    uint8_t GetBatteryLevel();

    void OnChargingStatusChanged(std::function<void(bool)> callback);
    void OnChargeDoneStatusChanged(std::function<void(bool)> callback);
    void OnLowBatteryStatusChanged(std::function<void(bool)> callback);
};

#endif