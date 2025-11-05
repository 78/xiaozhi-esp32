#include "led_controller.h"
#include "power_manager.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <rom/ets_sys.h>

// 定义TAG常量
const char *FogSeekLedController::TAG = "FogSeekLedController";

// 构造函数 - 创建LED闪烁定时器
FogSeekLedController::FogSeekLedController() : led_blink_timer_(nullptr),
                                               red_led_state_(false),
                                               green_led_state_(false),
                                               is_power_on_(false),
                                               blink_interval_ms_(0),
                                               blink_counter_(0),
                                               blink_red_(false),
                                               blink_green_(false),
                                               cold_light_(nullptr),
                                               warm_light_(nullptr),
                                               cold_light_state_(false),
                                               warm_light_state_(false)
{
    esp_timer_create_args_t blink_timer_args = {};
    blink_timer_args.callback = &FogSeekLedController::BlinkTimerCallback;
    blink_timer_args.arg = this;
    blink_timer_args.name = "led_blink_timer";
    ESP_ERROR_CHECK(esp_timer_create(&blink_timer_args, &led_blink_timer_));
}

// 析构函数 - 清理定时器
FogSeekLedController::~FogSeekLedController()
{
    if (led_blink_timer_)
    {
        esp_timer_stop(led_blink_timer_);
        esp_timer_delete(led_blink_timer_);
        led_blink_timer_ = nullptr;
    }

    // 删除冷暖色灯
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

// 初始化LED GPIO
void FogSeekLedController::InitializeLeds(FogSeekPowerManager &power_manager, const led_pin_config_t *pin_config)
{
    // 保存引脚配置
    pin_config_ = *pin_config;

    // 初始化通用LED (红/绿)
    gpio_config_t led_conf = {};
    led_conf.intr_type = GPIO_INTR_DISABLE;
    led_conf.mode = GPIO_MODE_OUTPUT;
    led_conf.pin_bit_mask = (1ULL << pin_config->red_gpio) | (1ULL << pin_config->green_gpio);
    led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&led_conf);

    // 如果配置了冷暖色灯GPIO，则初始化冷暖色灯
    if (pin_config->cold_light_gpio >= 0 || pin_config->warm_light_gpio >= 0)
    {
        InitializeColdWarmLeds(pin_config->cold_light_gpio, pin_config->warm_light_gpio);
    }

    // 主要针对插USB的情况，初始化时根据电源状态设置LED
    UpdateBatteryStatus(power_manager);

    ESP_LOGI(TAG, "LEDs initialized");
}
// 初始化冷暖色灯
void FogSeekLedController::InitializeColdWarmLeds(int cold_gpio, int warm_gpio)
{
    // 初始化冷暖色灯（使用PWM控制）
    if (cold_gpio >= 0)
    {
        // 为冷色灯和暖色灯分配不同的LEDC通道，避免冲突
        cold_light_ = new GpioLed(static_cast<gpio_num_t>(cold_gpio), 0, LEDC_TIMER_1, LEDC_CHANNEL_0);
        cold_light_->TurnOff();
    }

    if (warm_gpio >= 0)
    {
        warm_light_ = new GpioLed(static_cast<gpio_num_t>(warm_gpio), 0, LEDC_TIMER_1, LEDC_CHANNEL_1);
        warm_light_->TurnOff();
    }
}

// 设置LED状态
void FogSeekLedController::SetLedState(bool red, bool green)
{
    // 停止闪烁
    StopBlink();

    // 设置LED状态 (直接使用布尔值控制电平)
    gpio_set_level((gpio_num_t)pin_config_.red_gpio, red);
    gpio_set_level((gpio_num_t)pin_config_.green_gpio, green);

    // 保存当前状态
    red_led_state_ = red;
    green_led_state_ = green;
}

// 开始闪烁
void FogSeekLedController::StartBlink(int interval_ms, bool red, bool green)
{
    // 停止当前闪烁
    StopBlink();

    // 设置闪烁参数
    blink_interval_ms_ = interval_ms;
    blink_red_ = red;
    blink_green_ = green;
    blink_counter_ = 0;

    // 设置初始状态为熄灭
    gpio_set_level((gpio_num_t)pin_config_.red_gpio, 1);
    gpio_set_level((gpio_num_t)pin_config_.green_gpio, 1);

    // 启动定时器
    if (led_blink_timer_)
    {
        esp_timer_start_periodic(led_blink_timer_, interval_ms * 1000);
    }
}

// 停止闪烁
void FogSeekLedController::StopBlink()
{
    if (led_blink_timer_)
    {
        esp_timer_stop(led_blink_timer_);
    }

    // 恢复到之前的状态
    gpio_set_level((gpio_num_t)pin_config_.red_gpio, red_led_state_ ? 0 : 1);
    gpio_set_level((gpio_num_t)pin_config_.green_gpio, green_led_state_ ? 0 : 1);
}

// 处理设备状态变化的LED指示
void FogSeekLedController::HandleDeviceState(DeviceState current_state, FogSeekPowerManager &power_manager)
{
    // 如果设备未开机，则不处理设备状态
    if (!is_power_on_)
    {
        return;
    }

    switch (current_state)
    {
    case DeviceState::kDeviceStateIdle:
        // 空闲状态时根据电源状态更新LED
        UpdateBatteryStatus(power_manager);
        break;

    case DeviceState::kDeviceStateListening:
        // 监听状态时两个LED同时亮起表示正在监听
        SetLedState(true, true);
        break;

    case DeviceState::kDeviceStateSpeaking:
        // 说话状态时两个LED同时慢闪烁表示正在说话
        StartBlink(500, true, true); // 500ms间隔慢闪烁
        break;

    default:
        ESP_LOGW(TAG, "Unknown device state: %d", static_cast<int>(current_state));
        break;
    }
}

// 根据电池/电源状态更新LED
void FogSeekLedController::UpdateBatteryStatus(FogSeekPowerManager &power_manager)
{
    if ((power_manager.IsBatteryPowered()) && !is_power_on_)
    {
        // 针对电池供电的设计，短按会初始化但不亮灯，长按开机再正常判断LED灯逻辑
        SetLedState(false, false);
        return;
    }

    // 根据电源状态更新LED显示
    switch (power_manager.GetPowerState())
    {
    case FogSeekPowerManager::PowerState::BATTERY_POWER:
        // 电池供电时绿色LED常亮
        SetLedState(false, true);
        break;

    case FogSeekPowerManager::PowerState::USB_POWER_CHARGING:
        // USB充电中时红灯慢闪
        StartBlink(800, true, false);
        break;

    case FogSeekPowerManager::PowerState::USB_POWER_DONE:
        // USB充电完成时绿色LED常亮
        SetLedState(false, true);
        break;

    case FogSeekPowerManager::PowerState::USB_POWER_NO_BATTERY:
        // USB供电无电池时绿色LED常亮
        SetLedState(false, true);
        break;

    case FogSeekPowerManager::PowerState::LOW_BATTERY:
        // 低电量警告时红色LED快闪表示警告状态
        StartBlink(200, true, false); // 200ms间隔快闪烁
        break;

    case FogSeekPowerManager::PowerState::NO_POWER:
    default:
        // 熄灭LED
        SetLedState(false, false);
        break;
    }
}

// 定时器回调函数
void FogSeekLedController::BlinkTimerCallback(void *arg)
{
    FogSeekLedController *led_controller = static_cast<FogSeekLedController *>(arg);

    // 切换LED状态
    bool red_state = led_controller->blink_red_ && (led_controller->blink_counter_ % 2 == 0);
    bool green_state = led_controller->blink_green_ && (led_controller->blink_counter_ % 2 == 0);

    // 设置LED状态
    gpio_set_level((gpio_num_t)led_controller->pin_config_.red_gpio, red_state);
    gpio_set_level((gpio_num_t)led_controller->pin_config_.green_gpio, green_state);

    // 更新计数器
    led_controller->blink_counter_++;
}

// 冷色灯控制
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

// 暖色灯控制
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

// 设置冷色灯亮度
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

// 设置暖色灯亮度
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