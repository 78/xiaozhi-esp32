#include "power_manager.h"
#include "adc_battery_monitor.h"
#include <esp_log.h>
#include <esp_timer.h>
#include "application.h"
#include "assets/lang_config.h"
#include "led_controller.h"

#define TAG "PowerManager"

// 构造函数 - 初始化电源控制引脚
PowerManager::PowerManager()
{
    // 配置电源保持引脚为输出
    gpio_config_t pwr_conf = {};
    pwr_conf.intr_type = GPIO_INTR_DISABLE;
    pwr_conf.mode = GPIO_MODE_OUTPUT;
    pwr_conf.pin_bit_mask = (1ULL << PWR_HOLD_GPIO);
    pwr_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pwr_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&pwr_conf);
    gpio_set_level(PWR_HOLD_GPIO, 0); // 初始化为关机状态
}

// 析构函数 - 清理定时器和资源
PowerManager::~PowerManager()
{
    if (battery_check_timer_)
    {
        esp_timer_stop(battery_check_timer_);
        esp_timer_delete(battery_check_timer_);
    }

    // 释放battery_monitor_资源
    if (battery_monitor_)
    {
        delete battery_monitor_;
        battery_monitor_ = nullptr;
    }
}

// 初始化电源管理器
void PowerManager::Initialize()
{
    // 初始化充电检测引脚（CHRG引脚）
    gpio_config_t charge_conf = {};
    charge_conf.intr_type = GPIO_INTR_DISABLE;
    charge_conf.mode = GPIO_MODE_INPUT;
    charge_conf.pin_bit_mask = (1ULL << PWR_CHARGING_GPIO);
    charge_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    charge_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&charge_conf);

    // 初始化充电完成检测引脚（STDBY引脚）
    gpio_config_t charge_done_conf = {};
    charge_done_conf.intr_type = GPIO_INTR_DISABLE;
    charge_done_conf.mode = GPIO_MODE_INPUT;
    charge_done_conf.pin_bit_mask = (1ULL << PWR_CHARGE_DONE_GPIO);
    charge_done_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    charge_done_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&charge_done_conf);

    // 初始化电池监控器，ADC初始化后，STDBY引脚无需初始化
    // 注意：由于BATTERY_ADC_GPIO连接到ADC2，在WiFi启用时无法使用ADC2
    // 所以暂时不初始化battery_monitor_，在ReadBatteryLevel中返回固定值
    // 实际使用时应根据硬件连接选择正确的ADC单元和通道
    // 示例：使用ADC1的通道0 (对应GPIO_NUM_16)
    // battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_0, 2.0f, 1.0f, PWR_CHARGE_DONE_GPIO);

    // 更新初始电源状态
    UpdatePowerState();

    // 创建电池检查定时器
    esp_timer_create_args_t timer_args = {
        .callback = &PowerManager::BatteryCheckTimerCallback,
        .arg = this,
        .name = "battery_check_timer"};
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &battery_check_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(battery_check_timer_, 30 * 1000 * 1000)); // 每30秒检查一次
}

// 开机
void PowerManager::PowerOn()
{
    pwr_hold_state_ = true;
    gpio_set_level(PWR_HOLD_GPIO, 1);
    ESP_LOGI(TAG, "Power ON");
}

// 关机
void PowerManager::PowerOff()
{
    pwr_hold_state_ = false;
    gpio_set_level(PWR_HOLD_GPIO, 0);
    ESP_LOGI(TAG, "Power OFF");
}

// 读取电池电量百分比
uint8_t PowerManager::ReadBatteryLevel()
{
    return 80; // 返回固定值80%，模拟正常电池电量
    // return battery_monitor_->GetBatteryLevel();
}

// 更新电源状态
void PowerManager::UpdatePowerState()
{
    // 读取电池电量
    battery_level_ = ReadBatteryLevel();

    bool is_charging = gpio_get_level(PWR_CHARGING_GPIO) == 0;
    bool is_charge_done = gpio_get_level(PWR_CHARGE_DONE_GPIO) == 0;
    bool battery_detected = battery_level_ > 5; // 简单的电池检测方法

    PowerState previous_state = power_state_; // 保存之前的状态

    // 根据充电引脚状态和电池状态判断电源状态
    if (is_charging && battery_detected)
    {
        power_state_ = USB_POWER_CHARGING;
    }
    else if (is_charge_done && battery_detected)
    {
        power_state_ = USB_POWER_DONE;
    }
    else if (is_charging && !battery_detected)
    {
        // 有USB供电但未检测到电池
        power_state_ = USB_POWER_NO_BATTERY;
    }
    else if (is_charge_done && !battery_detected)
    {
        // 充电完成但无电池（理论上不应该出现）
        power_state_ = USB_POWER_NO_BATTERY;
    }
    else if (battery_detected && !low_battery_warning_)
    {
        power_state_ = BATTERY_POWER;
    }
    else if (battery_detected && low_battery_warning_)
    {
        power_state_ = LOW_BATTERY;
    }
    else
    {
        power_state_ = NO_POWER;
    }

    // 如果电源状态发生变化，调用回调函数通知LED控制器更新状态
    if (previous_state != power_state_ && power_state_callback_)
    {
        power_state_callback_(power_state_);
    }

    ESP_LOGD(TAG, "Battery level: %d%%, Power state: %d", battery_level_, power_state_);
}

// 检查低电量
void PowerManager::CheckLowBattery()
{
    // 读取最新的电池电量
    battery_level_ = ReadBatteryLevel();

    if (power_state_ == BATTERY_POWER || power_state_ == LOW_BATTERY)
    {
        // 低于10%自动关机
        if (battery_level_ < 10 && !low_battery_shutdown_)
        {
            ESP_LOGW(TAG, "Critical battery level (%d%%), shutting down to protect battery", battery_level_);
            low_battery_shutdown_ = true;

            auto &app = Application::GetInstance();
            app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);

            PowerOff(); // 使用PowerOff函数替代直接GPIO操作
            ESP_LOGI(TAG, "Device shut down due to critical battery level");
            return;
        }
        // 低于20%警告
        else if (battery_level_ < 20 && battery_level_ >= 10 && !low_battery_warning_)
        {
            ESP_LOGW(TAG, "Low battery warning (%d%%)", battery_level_);
            low_battery_warning_ = true;

            auto &app = Application::GetInstance();
            app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
        }
        // 电量恢复到20%以上时重置警告标志
        else if (battery_level_ >= 20)
        {
            low_battery_warning_ = false;
        }
    }
    else if (power_state_ == USB_POWER_NO_BATTERY)
    {
        // 有USB供电但无电池，不需要检查低电量
        low_battery_warning_ = false;
        low_battery_shutdown_ = false;
        ESP_LOGI(TAG, "USB powered with no battery, skipping low battery check");
    }
    else
    {
        // USB供电时重置标志
        low_battery_warning_ = false;
        low_battery_shutdown_ = false;
    }

    // 更新电源状态
    UpdatePowerState();
}

// 电池检查定时器回调函数
void PowerManager::BatteryCheckTimerCallback(void *arg)
{
    PowerManager *self = static_cast<PowerManager *>(arg);
    self->CheckLowBattery();
}