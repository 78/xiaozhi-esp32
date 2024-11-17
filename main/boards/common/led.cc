#include "led.h"
#include "board.h"

#include <cstring>
#include <esp_log.h>

#define TAG "Led"

Led::Led(gpio_num_t gpio) {
    mutex_ = xSemaphoreCreateMutex();
    blink_event_group_ = xEventGroupCreate();
    xEventGroupSetBits(blink_event_group_, BLINK_TASK_STOPPED_BIT);

    if (gpio == GPIO_NUM_NC) {
        ESP_LOGI(TAG, "Builtin LED not connected");
        return;
    }
    
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio;
    strip_config.max_leds = 1;
    strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    led_strip_clear(led_strip_);

    SetGrey();
}

Led::~Led() {
    StopBlinkInternal();
    if (led_strip_ != nullptr) {
        led_strip_del(led_strip_);
    }
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
    }
    if (blink_event_group_ != nullptr) {
        vEventGroupDelete(blink_event_group_);
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
    StopBlinkInternal();
    xSemaphoreTake(mutex_, portMAX_DELAY);
    led_strip_set_pixel(led_strip_, 0, r_, g_, b_);
    led_strip_refresh(led_strip_);
    xSemaphoreGive(mutex_);
}

void Led::TurnOff() {
    if (led_strip_ == nullptr) {
        return;
    }
    StopBlinkInternal();
    xSemaphoreTake(mutex_, portMAX_DELAY);
    led_strip_clear(led_strip_);
    xSemaphoreGive(mutex_);
}

void Led::BlinkOnce() {
    Blink(1, 100);
}

void Led::Blink(int times, int interval_ms) {
    StartBlinkTask(times, interval_ms);
}

void Led::StartContinuousBlink(int interval_ms) {
    StartBlinkTask(BLINK_INFINITE, interval_ms);
}

void Led::StartBlinkTask(int times, int interval_ms) {
    if (led_strip_ == nullptr) {
        return;
    }
    StopBlinkInternal();
    xSemaphoreTake(mutex_, portMAX_DELAY);
    
    blink_times_ = times;
    blink_interval_ms_ = interval_ms;
    should_blink_ = true;

    xEventGroupClearBits(blink_event_group_, BLINK_TASK_STOPPED_BIT);
    xEventGroupSetBits(blink_event_group_, BLINK_TASK_RUNNING_BIT);

    xTaskCreate([](void* obj) {
        auto this_ = static_cast<Led*>(obj);
        int count = 0;
        while (this_->should_blink_ && (this_->blink_times_ == BLINK_INFINITE || count < this_->blink_times_)) {
            xSemaphoreTake(this_->mutex_, portMAX_DELAY);
            led_strip_set_pixel(this_->led_strip_, 0, this_->r_, this_->g_, this_->b_);
            led_strip_refresh(this_->led_strip_);
            xSemaphoreGive(this_->mutex_);

            vTaskDelay(this_->blink_interval_ms_ / portTICK_PERIOD_MS);
            if (!this_->should_blink_) break;

            xSemaphoreTake(this_->mutex_, portMAX_DELAY);
            led_strip_clear(this_->led_strip_);
            xSemaphoreGive(this_->mutex_);

            vTaskDelay(this_->blink_interval_ms_ / portTICK_PERIOD_MS);
            if (this_->blink_times_ != BLINK_INFINITE) count++;
        }
        this_->blink_task_ = nullptr;
        xEventGroupClearBits(this_->blink_event_group_, BLINK_TASK_RUNNING_BIT);
        xEventGroupSetBits(this_->blink_event_group_, BLINK_TASK_STOPPED_BIT);
        vTaskDelete(NULL);
    }, "blink", 2048, this, tskIDLE_PRIORITY, &blink_task_);

    xSemaphoreGive(mutex_);
}

void Led::StopBlinkInternal() {
    should_blink_ = false;
    xEventGroupWaitBits(blink_event_group_, BLINK_TASK_STOPPED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}
