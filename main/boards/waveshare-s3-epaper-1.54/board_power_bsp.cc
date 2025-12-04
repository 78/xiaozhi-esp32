#include <stdio.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include "board_power_bsp.h"

void BoardPowerBsp::PowerLedTask(void *arg) {
    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = (0x1ULL << GPIO_NUM_3);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));
    for (;;) {
        gpio_set_level(GPIO_NUM_3, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(GPIO_NUM_3, 1);
        // 不需要频繁闪烁，间隔指示效果更好
        vTaskDelay(pdMS_TO_TICKS(4800));
    }
}

BoardPowerBsp::BoardPowerBsp(int epdPowerPin, int audioPowerPin, int vbatPowerPin) : epdPowerPin_(epdPowerPin), audioPowerPin_(audioPowerPin), vbatPowerPin_(vbatPowerPin) {
    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = (0x1ULL << epdPowerPin_) | (0x1ULL << audioPowerPin_) | (0x1ULL << vbatPowerPin_);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));
    xTaskCreatePinnedToCore(PowerLedTask, "PowerLedTask", 3 * 1024, NULL, 2, NULL, 0);
}

BoardPowerBsp::~BoardPowerBsp() {
}

void BoardPowerBsp::PowerEpdOn() {
    gpio_set_level((gpio_num_t) epdPowerPin_, 0);
}

void BoardPowerBsp::PowerEpdOff() {
    gpio_set_level((gpio_num_t) epdPowerPin_, 1);
}

void BoardPowerBsp::PowerAudioOn() {
    gpio_set_level((gpio_num_t) audioPowerPin_, 0);
}

void BoardPowerBsp::PowerAudioOff() {
    gpio_set_level((gpio_num_t) audioPowerPin_, 1);
}

void BoardPowerBsp::VbatPowerOn() {
    gpio_set_level((gpio_num_t) vbatPowerPin_, 1);
}

void BoardPowerBsp::VbatPowerOff() {
    gpio_set_level((gpio_num_t) vbatPowerPin_, 0);
}