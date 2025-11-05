#include "power_manager.h"
#include "adc_battery_monitor.h"
#include <esp_log.h>
#include <esp_timer.h>
#include "application.h"
#include "assets/lang_config.h"

const char *FogSeekPowerManager::TAG = "FogSeekPowerManager";

// 构造函数 - 初始化电源控制引脚
FogSeekPowerManager::FogSeekPowerManager() : pwr_hold_state_(false),
                                             power_state_(PowerState::NO_POWER),
                                             low_battery_warning_(false),
                                             low_battery_shutdown_(false),
                                             battery_level_(0),
                                             battery_check_timer_(nullptr),
                                             battery_monitor_(nullptr)
{
}

// 析构函数 - 清理定时器和资源
FogSeekPowerManager::~FogSeekPowerManager()
{
    if (battery_check_timer_)
    {
        esp_timer_stop(battery_check_timer_);
        esp_timer_delete(battery_check_timer_);
        battery_check_timer_ = nullptr;
    }

    // 释放battery_monitor_资源
    if (battery_monitor_)
    {
        delete battery_monitor_;
        battery_monitor_ = nullptr;
    }
}

// 初始化电源管理器
void FogSeekPowerManager::Initialize(const power_pin_config_t *pin_config)
{
    // 保存引脚配置
    pin_config_ = *pin_config;

    // 配置电源保持引脚为输出
    gpio_config_t pwr_conf = {};
    pwr_conf.intr_type = GPIO_INTR_DISABLE;
    pwr_conf.mode = GPIO_MODE_OUTPUT;
    pwr_conf.pin_bit_mask = (1ULL << pin_config->hold_gpio);
    pwr_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pwr_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&pwr_conf);
    gpio_set_level((gpio_num_t)pin_config->hold_gpio, 0); // 初始化为关机状态

    // 初始化充电状态引脚（done引脚通过ADC函数初始化）
    gpio_config_t charge_conf = {};
    charge_conf.intr_type = GPIO_INTR_DISABLE;
    charge_conf.mode = GPIO_MODE_INPUT;
    charge_conf.pin_bit_mask = (1ULL << pin_config->charging_gpio);
    charge_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    charge_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&charge_conf);

    // 检查并配置ADC引脚
    adc_channel_t adc_channel;
    if (pin_config->adc_gpio >= 1 && pin_config->adc_gpio <= 10)
    {
        // GPIO1-10属于ADC_UNIT_1，通道为n-1
        adc_channel = (adc_channel_t)(pin_config->adc_gpio - 1);
        ESP_LOGI(TAG, "Configured ADC pin: GPIO%d, Channel: ADC_CHANNEL_%d", pin_config->adc_gpio, adc_channel);
    }
    else
    {
        // 如果传入的引脚不是GPIO1-10，则报错警告并使用默认的ADC_CHANNEL_9
        ESP_LOGW(TAG, "Invalid ADC pin: GPIO%d. Valid range is GPIO1-GPIO10 for ADC_UNIT_1", pin_config->adc_gpio);
        adc_channel = ADC_CHANNEL_9;
    }

    battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, adc_channel, 2.0f, 1.0f, (gpio_num_t)pin_config->charge_done_gpio);

    // 更新初始电源状态
    UpdatePowerState();

    // 创建电池检查定时器
    esp_timer_create_args_t timer_args = {
        .callback = &FogSeekPowerManager::BatteryCheckTimerCallback,
        .arg = this,
        .name = "battery_check_timer"};
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &battery_check_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(battery_check_timer_, 30 * 1000 * 1000)); // 每30秒检查一次
}

// 开机
void FogSeekPowerManager::PowerOn()
{
    pwr_hold_state_ = true;
    gpio_set_level((gpio_num_t)pin_config_.hold_gpio, 1);
    ESP_LOGI(TAG, "Power ON");
}

// 关机
void FogSeekPowerManager::PowerOff()
{
    pwr_hold_state_ = false;
    gpio_set_level((gpio_num_t)pin_config_.hold_gpio, 0);
    ESP_LOGI(TAG, "Power OFF");
}

// 读取电池电量百分比
uint8_t FogSeekPowerManager::ReadBatteryLevel()
{
    return battery_monitor_->GetBatteryLevel();
}

// 更新电源状态
void FogSeekPowerManager::UpdatePowerState()
{
    // 读取电池电量
    battery_level_ = ReadBatteryLevel();

    bool is_charging = gpio_get_level((gpio_num_t)pin_config_.charging_gpio) == 0;
    bool is_charge_done = gpio_get_level((gpio_num_t)pin_config_.charge_done_gpio) == 0;
    bool battery_detected = battery_level_ > 5; // 简单的电池检测方法

    PowerState previous_state = power_state_; // 保存之前的状态

    // 根据充电引脚状态和电池状态判断电源状态
    if (is_charging && battery_detected)
    {
        power_state_ = PowerState::USB_POWER_CHARGING;
    }
    else if (is_charge_done && battery_detected)
    {
        power_state_ = PowerState::USB_POWER_DONE;
    }
    else if (is_charging && !battery_detected) // 无电池时，charging和done会交替变化
    {
        power_state_ = PowerState::USB_POWER_NO_BATTERY;
    }
    else if (is_charge_done && !battery_detected)
    {
        power_state_ = PowerState::USB_POWER_NO_BATTERY;
    }
    else if (battery_detected && !low_battery_warning_)
    {
        power_state_ = PowerState::BATTERY_POWER;
    }
    else if (battery_detected && low_battery_warning_)
    {
        power_state_ = PowerState::LOW_BATTERY;
    }
    else
    {
        power_state_ = PowerState::NO_POWER;
    }

    // 如果电源状态发生变化，调用回调函数通知LED控制器更新状态
    if (previous_state != power_state_ && power_state_callback_)
    {
        power_state_callback_(power_state_);
    }

    ESP_LOGD(TAG, "Battery level: %d%%, Power state: %d", battery_level_, static_cast<int>(power_state_));
}

// 检查低电量
void FogSeekPowerManager::CheckLowBattery()
{
    // 读取最新的电池电量
    battery_level_ = ReadBatteryLevel();

    if (power_state_ == PowerState::BATTERY_POWER || power_state_ == PowerState::LOW_BATTERY)
    {
        // 低于40%自动关机，电池映射表1对应3.72V
        if (battery_level_ < 40 && !low_battery_shutdown_)
        {
            ESP_LOGW(TAG, "Critical battery level (%d%%), shutting down to protect battery", battery_level_);
            low_battery_shutdown_ = true;

            auto &app = Application::GetInstance();
            app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
            vTaskDelay(pdMS_TO_TICKS(500)); // 添加延时确保声音播放完成

            PowerOff(); // 使用PowerOff函数替代直接GPIO操作
            ESP_LOGI(TAG, "Device shut down due to critical battery level");
            return;
        }
        // 低于50%警告，电池映射表1对应3.61V
        else if (battery_level_ < 50 && battery_level_ >= 40 && !low_battery_warning_)
        {
            ESP_LOGW(TAG, "Low battery warning (%d%%)", battery_level_);
            low_battery_warning_ = true;

            auto &app = Application::GetInstance();
            app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
            vTaskDelay(pdMS_TO_TICKS(500)); // 添加延时确保声音播放完成
        }
        // 电量恢复到20%以上时重置警告标志
        else if (battery_level_ >= 20)
        {
            low_battery_warning_ = false;
        }
    }
    else if (power_state_ == PowerState::USB_POWER_NO_BATTERY)
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
void FogSeekPowerManager::BatteryCheckTimerCallback(void *arg)
{
    FogSeekPowerManager *self = static_cast<FogSeekPowerManager *>(arg);
    self->CheckLowBattery();
}