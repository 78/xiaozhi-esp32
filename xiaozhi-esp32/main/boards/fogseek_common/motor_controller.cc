void MotorController::InitServo(int gpio_num) {
    servo_gpio_ = gpio_num;

    // 配置LEDC定时器
    ledc_timer_config_t timer_conf = {
        .duty_resolution = LEDC_TIMER_10_BIT,     // 10位分辨率
        .freq_hz = 50,                            // 50Hz频率（20ms周期）
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = servo_timer_,
        .clk_div = 0,
    };
    ledc_timer_init(&timer_conf);

    // 配置LEDC通道
    ledc_channel_config_t channel_conf = {
        .channel = servo_channel_,
        .duty = 0,
        .gpio_num = gpio_num,
        .intr_type = LEDC_INTR_DISABLE,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = servo_timer_,
    };
    ledc_channel_init(&channel_conf);
}

uint32_t MotorController::AngleToPulseWidth(int angle) {
    // 角度映射到脉宽：0.5ms ~ 2.5ms 对应 0° ~ 180°
    // 线性关系：pulse_width = 500 + (angle / 180.0) * 2000
    return 500 + (angle / 180.0) * 2000;  // 单位：微秒
}

void MotorController::SetServoAngle(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    uint32_t pulse_width_us = AngleToPulseWidth(angle);
    uint32_t duty = pulse_width_us * (1 << LEDC_TIMER_10_BIT) / 20000;  // 20ms周期

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, servo_channel_, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, servo_channel_);
}