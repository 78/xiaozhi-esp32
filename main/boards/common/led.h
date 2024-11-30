#ifndef _LED_H_
#define _LED_H_

#include <led_strip.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>

#define BLINK_INFINITE -1
#define BLINK_TASK_STOPPED_BIT BIT0
#define BLINK_TASK_RUNNING_BIT BIT1

#define DEFAULT_BRIGHTNESS 4
#define HIGH_BRIGHTNESS 16
#define LOW_BRIGHTNESS 2

class Led {
public:
    Led(gpio_num_t gpio);
    ~Led();

    void BlinkOnce();
    void Blink(int times, int interval_ms);
    void StartContinuousBlink(int interval_ms);
    void TurnOn();
    void TurnOff();
    void SetColor(uint8_t r, uint8_t g, uint8_t b);
    void SetWhite(uint8_t brightness = DEFAULT_BRIGHTNESS) { SetColor(brightness, brightness, brightness); }
    void SetGrey(uint8_t brightness = DEFAULT_BRIGHTNESS) { SetColor(brightness, brightness, brightness); }
    void SetRed(uint8_t brightness = DEFAULT_BRIGHTNESS) { SetColor(brightness, 0, 0); }
    void SetGreen(uint8_t brightness = DEFAULT_BRIGHTNESS) { SetColor(0, brightness, 0); }
    void SetBlue(uint8_t brightness = DEFAULT_BRIGHTNESS) { SetColor(0, 0, brightness); }

private:
    std::mutex mutex_;
    TaskHandle_t blink_task_ = nullptr;
    led_strip_handle_t led_strip_ = nullptr;
    uint8_t r_ = 0, g_ = 0, b_ = 0;
    int blink_counter_ = 0;
    int blink_interval_ms_ = 0;
    esp_timer_handle_t blink_timer_ = nullptr;

    void StartBlinkTask(int times, int interval_ms);
    void OnBlinkTimer();
};

#endif // _LED_H_
