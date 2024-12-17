#ifndef _LED_H_
#define _LED_H_

#include <led_strip.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>


#define DEFAULT_BRIGHTNESS 4
#define HIGH_BRIGHTNESS 16
#define LOW_BRIGHTNESS 2

class Led {
public:
    Led(gpio_num_t gpio, uint8_t max_leds);
    ~Led();

    led_strip_handle_t led_strip() { return led_strip_; }
    uint8_t max_leds() { return max_leds_; }

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
    uint8_t max_leds_ = -1;
    led_strip_handle_t led_strip_ = nullptr;
    uint8_t r_ = 0, g_ = 0, b_ = 0;
};

#endif // _LED_H_
