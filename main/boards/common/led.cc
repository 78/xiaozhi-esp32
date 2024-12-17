#include "led.h"
#include "board.h"

#include <cstring>
#include <esp_log.h>

#define TAG "Led"

Led::Led(gpio_num_t gpio, uint8_t max_leds) {
    if (gpio == GPIO_NUM_NC) {
        ESP_LOGI(TAG, "Builtin LED not connected");
        return;
    }

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio;
    strip_config.max_leds = max_leds;
    strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz

    max_leds_ = max_leds;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    
    led_strip_clear(led_strip_);
    SetGrey();
}

Led::~Led() {
    if (led_strip_ != nullptr) {
        led_strip_del(led_strip_);
    }
}

void Led::SetColor(uint8_t r, uint8_t g, uint8_t b) {
    r_ = r;
    g_ = g;
    b_ = b;
}

void Led::TurnOn() {
    if (led_strip_ == nullptr) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (int i = 0; i < max_leds_; i++) {
        led_strip_set_pixel(led_strip_, i, r_, g_, b_);
    }
    led_strip_refresh(led_strip_);
}

void Led::TurnOff() {
    if (led_strip_ == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    led_strip_clear(led_strip_);
}
