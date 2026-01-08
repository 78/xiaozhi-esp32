#include "servo_controller.h"
#include <esp_log.h>

#define TAG "FogSeekServoController"

FogSeekServoController::FogSeekServoController() : servo_gpio_(GPIO_NUM_NC), channel_(LEDC_CHANNEL_0),
                                                   timer_(LEDC_TIMER_0), current_angle_(90), initialized_(false) {}

FogSeekServoController::~FogSeekServoController()
{
    if (initialized_)
    {
        ledc_stop(LEDC_LOW_SPEED_MODE, channel_, 0);
    }
}

void FogSeekServoController::Initialize(gpio_num_t servo_gpio)
{
    servo_gpio_ = servo_gpio;

    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT, // 13位分辨率
        .timer_num = timer_,
        .freq_hz = 50, // 50Hz PWM频率，周期20ms
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 配置LEDC通道
    ledc_channel_config_t ledc_channel = {
        .gpio_num = servo_gpio_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_,
        .duty = 0,
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // 设置初始角度
    SetAngle(current_angle_);
    initialized_ = true;

    ESP_LOGI(TAG, "Servo controller initialized on GPIO %d", servo_gpio_);
}

void FogSeekServoController::SetAngle(uint16_t angle)
{
    if (!initialized_)
    {
        ESP_LOGE(TAG, "Servo controller not initialized");
        return;
    }

    // 限制角度范围
    if (angle > 180)
    {
        angle = 180;
    }

    current_angle_ = angle;

    // 计算PWM占空比
    // 通常舵机的控制脉冲范围是500-2500微秒，对应0-180度
    // 对应LEDC的duty值约为262-1310 (基于13位分辨率和20ms周期)
    uint32_t duty = (uint32_t)(((angle / 180.0) * (1310 - 262)) + 262);

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_));
}

uint16_t FogSeekServoController::GetAngle() const
{
    return current_angle_;
}