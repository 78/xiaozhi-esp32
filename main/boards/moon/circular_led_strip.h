#ifndef _SINGLE_LED_H_
#define _SINGLE_LED_H_

#include "../../led/led.h"
#include <driver/gpio.h>
#include <led_strip.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>
#include <vector>

class CircularLedStrip : public Led {
public:
    CircularLedStrip(gpio_num_t gpio);
    virtual ~CircularLedStrip();

    void OnStateChanged() override;

private:
    std::mutex mutex_;
    led_strip_handle_t led_strip_ = nullptr;
    uint8_t r_ = 0, g_ = 0, b_ = 0;
    int blink_counter_ = 0;
    int blink_interval_ms_ = 0;
    esp_timer_handle_t blink_timer_ = nullptr;
    esp_timer_handle_t wave_timer_ = nullptr;  // 新增律动效果定时器
    
    // RGB颜色数组
    const std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> rgb_colors_ = {
        {255, 0, 0},    // 红
        {255, 127, 0},  // 橙
        {255, 255, 0},  // 黄
        {127, 255, 0},  // 黄绿
        {0, 255, 0},    // 绿
        {0, 255, 127}, // 青绿
        {0, 255, 255},  // 青
        {0, 127, 255},  // 天蓝
        {0, 0, 255},    // 蓝
        {127, 0, 255},  // 紫
        {255, 0, 255},  // 粉红
        {255, 0, 127}   // 玫瑰红
    };
    size_t current_color_index_ = 0;

    void StartBlinkTask(int times, int interval_ms);
    void OnBlinkTimer();
    void OnWaveTimer();  // 新增律动效果回调
    
    void StartWaveEffect();  // 启动律动效果
    void StopWaveEffect();   // 停止律动效果
    
    void BlinkOnce();
    void Blink(int times, int interval_ms);
    void StartContinuousBlink(int interval_ms);
    void TurnOn();
    void TurnOff();
    void SetColor(uint8_t r, uint8_t g, uint8_t b);
    void SetPixelColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);  // 单独设置某个LED的颜色
};

#endif // _SINGLE_LED_H_