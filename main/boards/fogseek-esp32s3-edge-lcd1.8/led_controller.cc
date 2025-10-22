#include "led_controller.h"
#include "power_manager.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_timer.h>

// 构造函数 - 创建LED闪烁定时器
LedController::LedController()
{
    esp_timer_create_args_t blink_timer_args = {
        .callback = &LedController::BlinkTimerCallback,
        .arg = this,
        .name = "led_blink_timer"};
    ESP_ERROR_CHECK(esp_timer_create(&blink_timer_args, &led_blink_timer_));
}

// 析构函数 - 清理定时器
LedController::~LedController()
{
    if (led_blink_timer_)
    {
        esp_timer_stop(led_blink_timer_);
        esp_timer_delete(led_blink_timer_);
    }
}

// 初始化LED GPIO
void LedController::InitializeLeds(PowerManager &power_manager)
{
    gpio_config_t led_conf = {};
    led_conf.intr_type = GPIO_INTR_DISABLE;
    led_conf.mode = GPIO_MODE_OUTPUT;
    led_conf.pin_bit_mask = (1ULL << LED_GREEN_GPIO) | (1ULL << LED_RED_GPIO);
    led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&led_conf);

    // 初始化时根据电源状态设置LED
    UpdateBatteryStatus(power_manager);

    ESP_LOGI(TAG, "LEDs initialized");
}

// 设置LED状态 (针对低电平点亮的LED)
void LedController::SetLedState(bool red, bool green)
{
    // 停止闪烁
    StopBlink();

    // 设置LED状态 (低电平点亮)
    gpio_set_level(LED_RED_GPIO, red ? 1 : 0);
    gpio_set_level(LED_GREEN_GPIO, green ? 1 : 0);

    // 保存当前状态
    red_led_state_ = red;
    green_led_state_ = green;
}

// 开始闪烁
void LedController::StartBlink(int interval_ms, bool red, bool green)
{
    // 停止当前闪烁
    StopBlink();

    // 设置闪烁参数
    blink_interval_ms_ = interval_ms;
    blink_red_ = red;
    blink_green_ = green;
    blink_counter_ = 0;

    // 设置初始状态为熄灭
    gpio_set_level(LED_RED_GPIO, 1);
    gpio_set_level(LED_GREEN_GPIO, 1);

    // 启动定时器
    if (led_blink_timer_)
    {
        esp_timer_start_periodic(led_blink_timer_, interval_ms * 1000);
    }
}

// 停止闪烁
void LedController::StopBlink()
{
    if (led_blink_timer_)
    {
        esp_timer_stop(led_blink_timer_);
    }

    // 恢复到之前的状态
    gpio_set_level(LED_RED_GPIO, red_led_state_ ? 0 : 1);
    gpio_set_level(LED_GREEN_GPIO, green_led_state_ ? 0 : 1);
}

// 处理设备状态变化的LED指示
void LedController::HandleDeviceState(DeviceState current_state, PowerManager &power_manager)
{
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
void LedController::UpdateBatteryStatus(PowerManager &power_manager)
{
    if ((power_manager.GetPowerState() == PowerManager::BATTERY_POWER ||
         power_manager.GetPowerState() == PowerManager::LOW_BATTERY) &&
        !is_power_on_)
    {
        // 如果是电池供电且设备未开机，熄灭两个LED灯并直接返回
        SetLedState(false, false);
        return;
    }

    // 根据电源状态更新LED显示
    switch (power_manager.GetPowerState())
    {
    case PowerManager::BATTERY_POWER:
        // 电池供电时绿色LED常亮
        SetLedState(false, true);
        break;

    case PowerManager::USB_POWER_CHARGING:
        // USB充电中时红色常亮
        SetLedState(true, false);
        break;

    case PowerManager::USB_POWER_DONE:
        // USB充电完成时绿色LED常亮
        SetLedState(false, true);
        break;

    case PowerManager::USB_POWER_NO_BATTERY:
        // USB供电无电池时绿色LED常亮
        SetLedState(false, true);
        break;

    case PowerManager::LOW_BATTERY:
        // 低电量警告时红色LED快闪表示警告状态
        StartBlink(200, true, false); // 200ms间隔快闪烁
        break;

    case PowerManager::NO_POWER:
    default:
        // 熄灭LED
        SetLedState(false, false);
        break;
    }
}

// LED闪烁定时器回调函数
void LedController::BlinkTimerCallback(void *arg)
{
    LedController *self = static_cast<LedController *>(arg);
    if (self->is_power_on_)
    {
        self->blink_counter_++;

        // 奇数次点亮，偶数次熄灭
        if (self->blink_counter_ & 1)
        {
            // 点亮
            gpio_set_level(LED_RED_GPIO, self->blink_red_ ? 0 : 1);
            gpio_set_level(LED_GREEN_GPIO, self->blink_green_ ? 0 : 1);
        }
        else
        {
            // 熄灭
            gpio_set_level(LED_RED_GPIO, 1);
            gpio_set_level(LED_GREEN_GPIO, 1);
        }
    }
}