#ifndef _CIRCULAR_STRIP_H_
#define _CIRCULAR_STRIP_H_

#include "led.h"
#include <driver/gpio.h>
#include <led_strip.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>
#include <vector>

struct StripColor {
    uint8_t red = 0, green = 0, blue = 0;
};

class CircularStrip : public Led {
public:
    CircularStrip(gpio_num_t gpio, uint8_t max_leds);
    virtual ~CircularStrip();

    void OnStateChanged() override;

private:
    std::mutex mutex_;
    TaskHandle_t blink_task_ = nullptr;
    led_strip_handle_t led_strip_ = nullptr;
    int max_leds_ = 0;
    std::vector<StripColor> colors_;
    int blink_counter_ = 0;
    int blink_interval_ms_ = 0;
    esp_timer_handle_t strip_timer_ = nullptr;
    std::function<void()> strip_callback_ = nullptr;

    void StartStripTask(int interval_ms, std::function<void()> cb);

    void StaticColor(StripColor color);
    void Blink(StripColor color, int interval_ms);
    void Breathe(StripColor low, StripColor high, int interval_ms);
    void Rainbow(StripColor low, StripColor high, int interval_ms);
    void Scroll(StripColor low, StripColor high, int length, int interval_ms);
    void FadeOut(int interval_ms);
};

#endif // _CIRCULAR_STRIP_H_
