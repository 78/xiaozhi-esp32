#ifndef _GPIO_LED_H_
#define _GPIO_LED_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "led.h"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>

class GpioLed : public Led {
 public:
    GpioLed(gpio_num_t gpio);
    GpioLed(gpio_num_t gpio, int output_invert);
    GpioLed(gpio_num_t gpio, int output_invert, ledc_timer_t timer_num, ledc_channel_t channel);
    virtual ~GpioLed();

    void OnStateChanged() override;
    void TurnOn();
    void TurnOff();
    void SetBrightness(uint8_t brightness);

 private:
    std::mutex mutex_;
    TaskHandle_t blink_task_ = nullptr;
    ledc_channel_config_t ledc_channel_ = {0};
    bool ledc_initialized_ = false;
    uint32_t duty_ = 0;
    int blink_counter_ = 0;
    int blink_interval_ms_ = 0;
    esp_timer_handle_t blink_timer_ = nullptr;
    bool fade_up_ = true;

    void StartBlinkTask(int times, int interval_ms);
    void OnBlinkTimer();

    void BlinkOnce();
    void Blink(int times, int interval_ms);
    void StartContinuousBlink(int interval_ms);
    void StartFadeTask();
    void OnFadeEnd();
    static bool IRAM_ATTR FadeCallback(const ledc_cb_param_t *param, void *user_arg);
};

#endif  // _GPIO_LED_H_
