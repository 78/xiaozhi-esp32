#ifndef _Double_Led_LED_H_
#define _Double_Led_LED_H_

#include "led/led.h"
#include <driver/gpio.h>
#include <led_strip.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>

#define LED_MAX_NUM    2

struct Color {
    uint8_t red = 0, green = 0, blue = 0;
};

class LedEye : public Led {
public:
    LedEye(gpio_num_t gpio);
    virtual ~LedEye();
    void OnStateChanged() override;

private:
    std::mutex mutex_;
    TaskHandle_t blink_task_ = nullptr;
    led_strip_handle_t led_strip_ = nullptr;
    struct Color colors[LED_MAX_NUM];
    uint8_t cyc_blink_flag;
    int blink_counter_ = 0;
    int blink_interval_ms_ = 0;
    esp_timer_handle_t blink_timer_ = nullptr;

    void StartBlinkTask(int times, int interval_ms);
    void OnBlinkTimer();

    void BlinkOnce();
    void Blink(int times, int interval_ms);
    void StartContinuousBlink(int interval_ms);
    void StartBlink(int interval_ms);
    void TurnOn();
    void TurnOff();
    void SetSingleColor(uint8_t index,uint8_t red,uint8_t green,uint8_t blue);
    void SetAllColor(uint8_t red,uint8_t green,uint8_t blue);
};

#endif // _LedEye_LED_H_
