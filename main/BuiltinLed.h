#ifndef _BUILTIN_LED_H_
#define _BUILTIN_LED_H_

#include "led_strip.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class BuiltinLed {
public:
    BuiltinLed();
    ~BuiltinLed();

    void BlinkOnce();
    void Blink(int times, int interval_ms);
    void TurnOn();
    void TurnOff();
    void SetColor(uint8_t r, uint8_t g, uint8_t b);
    void SetWhite() { SetColor(128, 128, 128); }
    void SetGrey() { SetColor(32, 32, 32); }
    void SetRed() { SetColor(128, 0, 0); }
    void SetGreen() { SetColor(0, 128, 0); }
    void SetBlue() { SetColor(0, 0, 128); }

private:
    SemaphoreHandle_t mutex_;
    TaskHandle_t blink_task_ = nullptr;
    led_strip_handle_t led_strip_ = nullptr;
    uint8_t r_ = 0, g_ = 0, b_ = 0;

    void Configure();
};

#endif // _BUILTIN_LED_H_
