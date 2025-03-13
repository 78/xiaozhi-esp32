#include "backlight.h"
#include "settings.h"

#include <esp_log.h>
#include <driver/ledc.h>

#define TAG "Backlight" // 日志标签

// Backlight构造函数
Backlight::Backlight() {
    // 创建背光渐变定时器
    const esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            auto self = static_cast<Backlight*>(arg);
            self->OnTransitionTimer(); // 定时器回调函数
        },
        .arg = this, // 回调函数参数
        .dispatch_method = ESP_TIMER_TASK, // 定时器调度方式
        .name = "backlight_timer", // 定时器名称
        .skip_unhandled_events = true, // 跳过未处理的事件
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &transition_timer_)); // 创建定时器
}

// Backlight析构函数
Backlight::~Backlight() {
    if (transition_timer_ != nullptr) {
        esp_timer_stop(transition_timer_); // 停止定时器
        esp_timer_delete(transition_timer_); // 删除定时器
    }
}

// 恢复背光亮度
void Backlight::RestoreBrightness() {
    // 从设置中加载亮度值
    Settings settings("display");
    SetBrightness(settings.GetInt("brightness", 75)); // 默认亮度为75
}

// 设置背光亮度
void Backlight::SetBrightness(uint8_t brightness, bool permanent) {
    if (brightness > 100) {
        brightness = 100; // 亮度最大为100
    }

    if (brightness_ == brightness) {
        return; // 如果亮度未变化，直接返回
    }

    if (permanent) {
        // 如果需要永久保存亮度值
        Settings settings("display", true);
        settings.SetInt("brightness", brightness); // 保存亮度值
    }

    target_brightness_ = brightness; // 设置目标亮度
    step_ = (target_brightness_ > brightness_) ? 1 : -1; // 计算亮度变化步长

    if (transition_timer_ != nullptr) {
        // 启动定时器，每5ms更新一次亮度
        esp_timer_start_periodic(transition_timer_, 5 * 1000);
    }
    ESP_LOGI(TAG, "Set brightness to %d", brightness); // 日志：设置亮度
}

// 定时器回调函数，用于渐变调整亮度
void Backlight::OnTransitionTimer() {
    if (brightness_ == target_brightness_) {
        esp_timer_stop(transition_timer_); // 如果达到目标亮度，停止定时器
        return;
    }

    brightness_ += step_; // 调整亮度
    SetBrightnessImpl(brightness_); // 设置实际亮度

    if (brightness_ == target_brightness_) {
        esp_timer_stop(transition_timer_); // 如果达到目标亮度，停止定时器
    }
}

// PwmBacklight构造函数
PwmBacklight::PwmBacklight(gpio_num_t pin, bool output_invert) : Backlight() {
    // 配置LEDC定时器
    const ledc_timer_config_t backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, // 低速模式
        .duty_resolution = LEDC_TIMER_10_BIT, // 10位分辨率
        .timer_num = LEDC_TIMER_0, // 定时器0
        .freq_hz = 20000, // PWM频率为20kHz，防止电感啸叫
        .clk_cfg = LEDC_AUTO_CLK, // 自动选择时钟源
        .deconfigure = false // 不取消配置
    };
    ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer)); // 配置定时器

    // 配置LEDC通道用于PWM背光控制
    const ledc_channel_config_t backlight_channel = {
        .gpio_num = pin, // GPIO引脚
        .speed_mode = LEDC_LOW_SPEED_MODE, // 低速模式
        .channel = LEDC_CHANNEL_0, // 通道0
        .intr_type = LEDC_INTR_DISABLE, // 禁用中断
        .timer_sel = LEDC_TIMER_0, // 使用定时器0
        .duty = 0, // 初始占空比为0
        .hpoint = 0, // 初始高电平点为0
        .flags = {
            .output_invert = output_invert, // 输出是否反转
        }
    };
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel)); // 配置通道
}

// PwmBacklight析构函数
PwmBacklight::~PwmBacklight() {
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0); // 停止LEDC通道
}

// 设置实际背光亮度
void PwmBacklight::SetBrightnessImpl(uint8_t brightness) {
    // LEDC分辨率为10位，因此100% = 1023
    uint32_t duty_cycle = (1023 * brightness) / 100; // 计算占空比
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_cycle); // 设置占空比
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0); // 更新占空比
}