#include "BuiltinLed.h"
#include <cstring>
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "builtin_led"

BuiltinLed::BuiltinLed() {
    mutex_ = xSemaphoreCreateMutex();

    Configure();
    SetGreen();
}

BuiltinLed::~BuiltinLed() {
    if (blink_task_ != nullptr) {
        vTaskDelete(blink_task_);
    }
    if (led_strip_ != nullptr) {
        led_strip_del(led_strip_);
    }
    vSemaphoreDelete(mutex_);
}

void BuiltinLed::Configure() {
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config;
    bzero(&strip_config, sizeof(strip_config));
    strip_config.strip_gpio_num = CONFIG_BUILTIN_LED_GPIO;
    strip_config.max_leds = 1;

    led_strip_rmt_config_t rmt_config;
    bzero(&rmt_config, sizeof(rmt_config));
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip_);
}

void BuiltinLed::SetColor(uint8_t r, uint8_t g, uint8_t b) {
    r_ = r;
    g_ = g;
    b_ = b;
}

void BuiltinLed::TurnOn() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    led_strip_set_pixel(led_strip_, 0, r_, g_, b_);
    led_strip_refresh(led_strip_);
    xSemaphoreGive(mutex_);
}

void BuiltinLed::TurnOff() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    led_strip_clear(led_strip_);
    xSemaphoreGive(mutex_);
}

void BuiltinLed::BlinkOnce() {
    Blink(1, 100);
}

void BuiltinLed::Blink(int times, int interval_ms) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    struct BlinkTaskArgs {
        BuiltinLed* self;
        int times;
        int interval_ms;
    };
    auto args = new BlinkTaskArgs {this, times, interval_ms};

    xTaskCreate([](void* obj) {
        auto args = (BlinkTaskArgs*) obj;
        auto this_ = args->self;
        for (int i = 0; i < args->times; i++) {
            this_->TurnOn();
            vTaskDelay(args->interval_ms / portTICK_PERIOD_MS);
            this_->TurnOff();
            vTaskDelay(args->interval_ms / portTICK_PERIOD_MS);
        }
    
        delete args;
        this_->blink_task_ = nullptr;
        vTaskDelete(NULL);
    }, "blink", 4096, args, tskIDLE_PRIORITY, &blink_task_);

    xSemaphoreGive(mutex_);
}
