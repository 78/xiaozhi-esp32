#ifndef _MOTOR_CONTROLLER_H_
#define _MOTOR_CONTROLLER_H_

#include <driver/gpio.h>
#include <driver/ledc.h>

class FogSeekServoController
{
public:
    FogSeekServoController();
    ~FogSeekServoController();

    // 初始化舵机控制器
    void Initialize(gpio_num_t servo_gpio);

    // 设置舵机角度 (0-180度)
    void SetAngle(uint16_t angle);

    // 获取当前角度
    uint16_t GetAngle() const;

private:
    gpio_num_t servo_gpio_;
    ledc_channel_t channel_;
    ledc_timer_t timer_;
    uint16_t current_angle_;
    bool initialized_;
};

#endif // _MOTOR_CONTROLLER_H_