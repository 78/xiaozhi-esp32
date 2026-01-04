#include "led_controller.h"
#include "power_manager.h"
#include "../../application.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <rom/ets_sys.h>
#include <memory>

/// 日志标签
const char *FogSeekLedController::TAG = "FogSeekLedController";
const char *RedLed::TAG = "RedLed";
const char *GreenLed::TAG = "GreenLed";

// ==================== RedLed Implementation ====================
RedLed::RedLed(gpio_num_t gpio, int output_invert, ledc_timer_t timer_num, ledc_channel_t channel) : GpioLed(gpio, output_invert, timer_num, channel) {}

RedLed::~RedLed() {}

void RedLed::OnStateChanged()
{
    // 红灯不响应设备状态变化，因此为空实现
}

void RedLed::UpdateBatteryStatus(FogSeekPowerManager::PowerState state)
{
    switch (state)
    {
    case FogSeekPowerManager::PowerState::USB_POWER_CHARGING:
        // USB供电充电中：红灯呼吸效果
        StartFadeTask();
        break;

    case FogSeekPowerManager::PowerState::USB_POWER_DONE:
        // USB供电充电完成：红灯常亮
        TurnOn();
        break;

    case FogSeekPowerManager::PowerState::USB_POWER_NO_BATTERY:
        // USB供电无电池：红灯熄灭
        TurnOff();
        break;

    case FogSeekPowerManager::PowerState::BATTERY_POWER:
        // 电池供电：红灯熄灭
        TurnOff();
        break;

    case FogSeekPowerManager::PowerState::LOW_BATTERY:
        // 低电量状态：红灯100ms间隔连续闪烁
        SetBrightness(100);
        StartContinuousBlink(100);
        break;

    case FogSeekPowerManager::PowerState::NO_POWER:
        // 无电源：红灯熄灭
        TurnOff();
        break;

    default:
        TurnOff();
        break;
    }

    ESP_LOGD(TAG, "Red LED updated for power state: %d", static_cast<int>(state));
}

// ==================== GreenLed Implementation ====================
GreenLed::GreenLed(gpio_num_t gpio, int output_invert, ledc_timer_t timer_num, ledc_channel_t channel) : GpioLed(gpio, output_invert, timer_num, channel) {}
GreenLed::~GreenLed() {}

void GreenLed::OnStateChanged()
{
    if (ignore_state_changes_)
    {
        TurnOff();
        return;
    }
    auto &app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    switch (device_state)
    {
    case kDeviceStateIdle: // 空闲状态：绿灯呼吸效果
        StartFadeTask();
        break;

    case kDeviceStateListening: // 监听状态：绿灯常亮
        TurnOn();
        break;

    case kDeviceStateSpeaking: // 说话状态：绿灯1000ms间隔连续闪烁
        StartContinuousBlink(800);
        break;

    case kDeviceStateStarting:        // 启动状态
    case kDeviceStateWifiConfiguring: // WiFi配置状态
    case kDeviceStateConnecting:      // 连接状态的处理
    case kDeviceStateUpgrading:       // 升级状态
    case kDeviceStateActivating:      // 激活状态
    case kDeviceStateAudioTesting:    // 音频测试状态
        StartContinuousBlink(200);
        break;

    case kDeviceStateFatalError: // 致命错误状态
        StartContinuousBlink(100);
        break;

    case kDeviceStateUnknown: // 未知状态的处理
        TurnOff();
        break;

    default:
        ESP_LOGE(TAG, "Unknown gpio led event: %d", static_cast<int>(device_state));
        return;
    }

    ESP_LOGD(TAG, "Green LED updated for device state: %d", static_cast<int>(device_state));
}

// ==================== FogSeekLedController Implementation ====================

/**
 * @brief 构造函数 - 初始化LED控制器
 */
FogSeekLedController::FogSeekLedController() : red_led_state_(false),
                                               green_led_state_(false),
                                               red_led_(nullptr),
                                               green_led_(nullptr),
                                               cold_light_(nullptr),
                                               warm_light_(nullptr),
                                               cold_light_state_(false),
                                               warm_light_state_(false)
{
}

/**
 * @brief 析构函数 - 清理资源
 */
FogSeekLedController::~FogSeekLedController()
{
    // 删除红灯控制器
    if (red_led_)
    {
        delete red_led_;
        red_led_ = nullptr;
    }

    // 删除绿灯控制器
    if (green_led_)
    {
        delete green_led_;
        green_led_ = nullptr;
    }

    // 删除冷暖色灯控制器
    if (cold_light_)
    {
        delete cold_light_;
        cold_light_ = nullptr;
    }

    if (warm_light_)
    {
        delete warm_light_;
        warm_light_ = nullptr;
    }
}

/**
 * @brief 初始化LED GPIO
 *
 * @param power_manager 电源管理器引用
 * @param pin_config LED引脚配置
 */
void FogSeekLedController::InitializeLeds(FogSeekPowerManager &power_manager, const led_pin_config_t *pin_config)
{
    // 保存引脚配置
    pin_config_ = *pin_config;

    // 初始化红灯
    if (pin_config->red_gpio >= 0)
    {
        // 为红灯和绿灯分配不同的LEDC通道，避免冲突
        red_led_ = new RedLed(static_cast<gpio_num_t>(pin_config->red_gpio), 0, LEDC_TIMER_1, LEDC_CHANNEL_1);
    }

    // 初始化绿灯
    if (pin_config->green_gpio >= 0)
    {
        // 为红灯和绿灯分配不同的LEDC通道，避免冲突
        green_led_ = new GreenLed(static_cast<gpio_num_t>(pin_config->green_gpio), 0, LEDC_TIMER_1, LEDC_CHANNEL_2);
    }

    // 如果配置了冷暖色灯GPIO，则初始化冷暖色灯
    if (pin_config->cold_light_gpio >= 0 || pin_config->warm_light_gpio >= 0)
    {
        // 初始化冷暖色灯（使用PWM控制）
        if (pin_config->cold_light_gpio >= 0)
        {
            // 为冷色灯和暖色灯分配不同的LEDC通道，避免冲突
            cold_light_ = new GpioLed(static_cast<gpio_num_t>(pin_config->cold_light_gpio), 0, LEDC_TIMER_1, LEDC_CHANNEL_3);
            cold_light_->TurnOff();
        }

        if (pin_config->warm_light_gpio >= 0)
        {
            warm_light_ = new GpioLed(static_cast<gpio_num_t>(pin_config->warm_light_gpio), 0, LEDC_TIMER_1, LEDC_CHANNEL_4);
            warm_light_->TurnOff();
        }
    }

    UpdateLedStatus(power_manager);
    ESP_LOGI(TAG, "LEDs initialized");
}

/**
 * @brief 统一更新所有LED状态
 *
 * @param power_manager 电源管理器引用
 */
void FogSeekLedController::UpdateLedStatus(FogSeekPowerManager &power_manager)
{
    auto device_power_state = power_manager.GetDevicePowerState();
    auto power_state = power_manager.GetPowerState();

    switch (device_power_state)
    {
    case FogSeekPowerManager::DevicePowerState::CHARGING: // 充电状态，绿灯熄灭，红灯亮度正常，状态根据电源充电状态刷新
        red_led_->SetBrightness(100);
        red_led_->UpdateBatteryStatus(power_state);
        green_led_->TurnOff();
        green_led_->SetIgnoreStateChanges(true); // 设置绿灯忽略状态变化
        break;

    case FogSeekPowerManager::DevicePowerState::POWER_ON: // 开机状态，两个灯都工作（红灯亮度调低，绿灯正常）
        red_led_->SetBrightness(10);
        red_led_->UpdateBatteryStatus(power_state);
        green_led_->SetBrightness(100);
        green_led_->SetIgnoreStateChanges(false); // 恢复绿灯响应状态变化
        green_led_->OnStateChanged();
        break;

    case FogSeekPowerManager::DevicePowerState::POWER_OFF: // 关机状态，两个灯都熄灭
        red_led_->TurnOff();
        green_led_->TurnOff();
        green_led_->SetIgnoreStateChanges(true); // 设置绿灯忽略状态变化
        break;
    default:
        break;
    }
}

/**
 * @brief 控制冷色灯
 *
 * @param state true为开启，false为关闭
 */
void FogSeekLedController::SetColdLight(bool state)
{
    if (cold_light_)
    {
        if (state)
        {
            cold_light_->TurnOn();
            cold_light_state_ = true;
        }
        else
        {
            cold_light_->TurnOff();
            cold_light_state_ = false;
        }
    }
}

/**
 * @brief 控制暖色灯
 *
 * @param state true为开启，false为关闭
 */
void FogSeekLedController::SetWarmLight(bool state)
{
    if (warm_light_)
    {
        if (state)
        {
            warm_light_->TurnOn();
            warm_light_state_ = true;
        }
        else
        {
            warm_light_->TurnOff();
            warm_light_state_ = false;
        }
    }
}

/**
 * @brief 设置冷色灯亮度
 *
 * @param brightness 亮度值 (0-100)
 */
void FogSeekLedController::SetColdLightBrightness(int brightness)
{
    if (cold_light_)
    {
        cold_light_->SetBrightness(brightness);
        cold_light_state_ = (brightness > 0);
        if (brightness > 0)
        {
            cold_light_->TurnOn();
        }
        else
        {
            cold_light_->TurnOff();
        }
    }
}

/**
 * @brief 设置暖色灯亮度
 *
 * @param brightness 亮度值 (0-100)
 */
void FogSeekLedController::SetWarmLightBrightness(int brightness)
{
    if (warm_light_)
    {
        warm_light_->SetBrightness(brightness);
        warm_light_state_ = (brightness > 0);
        if (brightness > 0)
        {
            warm_light_->TurnOn();
        }
        else
        {
            warm_light_->TurnOff();
        }
    }
}
