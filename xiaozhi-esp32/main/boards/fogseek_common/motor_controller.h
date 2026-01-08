class MotorController {
public:
    // 舵机控制相关函数
    void SetServoAngle(int angle);  // 设置舵机角度（0-180度）
    void InitServo(int gpio_num);   // 初始化舵机PWM输出

private:
    ledc_channel_t servo_channel_ = LEDC_CHANNEL_0;
    ledc_timer_t servo_timer_ = LEDC_TIMER_0;
    int servo_gpio_ = -1;

    // 内部辅助函数
    uint32_t AngleToPulseWidth(int angle);
};