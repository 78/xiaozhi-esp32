#include "board_power_bsp.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

void BoardPowerBsp::PowerLedTask(void* arg) {
    auto* power = static_cast<BoardPowerBsp*>(arg);
    for (;;) {
        esp_io_expander_set_level(power->io_expander_, power->ledPin_, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_io_expander_set_level(power->io_expander_, power->ledPin_, 0);
        vTaskDelay(pdMS_TO_TICKS(4800));
    }
}

BoardPowerBsp::BoardPowerBsp(esp_io_expander_handle_t io_expander, uint32_t epdPowerPin,
                             uint32_t audioPowerPin, uint32_t vbatPowerPin, uint32_t paCtrlPin,
                             uint32_t ledPin)
    : io_expander_(io_expander),
      epdPowerPin_(epdPowerPin),
      audioPowerPin_(audioPowerPin),
      vbatPowerPin_(vbatPowerPin),
      paCtrlPin_(paCtrlPin),
      ledPin_(ledPin) {
    uint32_t output_pins = epdPowerPin_ | audioPowerPin_ | vbatPowerPin_ | paCtrlPin_ | ledPin_;
    uint32_t power_pins = epdPowerPin_ | audioPowerPin_ | vbatPowerPin_;
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_io_expander_set_dir(io_expander_, output_pins, IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_io_expander_set_level(io_expander_, power_pins, 1));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_io_expander_set_level(io_expander_, paCtrlPin_ | ledPin_, 0));
    xTaskCreatePinnedToCore(PowerLedTask, "PowerLedTask", 3 * 1024, this, 2, NULL, 0);
}

BoardPowerBsp::~BoardPowerBsp() {}

void BoardPowerBsp::PowerEpdOn() { esp_io_expander_set_level(io_expander_, epdPowerPin_, 1); }

void BoardPowerBsp::PowerEpdOff() { esp_io_expander_set_level(io_expander_, epdPowerPin_, 0); }

void BoardPowerBsp::PowerAudioOn() {
    esp_io_expander_set_level(io_expander_, audioPowerPin_, 1);
    esp_io_expander_set_level(io_expander_, paCtrlPin_, 1);
}

void BoardPowerBsp::PowerAudioOff() {
    esp_io_expander_set_level(io_expander_, paCtrlPin_, 0);
    esp_io_expander_set_level(io_expander_, audioPowerPin_, 0);
}

void BoardPowerBsp::VbatPowerOn() { esp_io_expander_set_level(io_expander_, vbatPowerPin_, 1); }

void BoardPowerBsp::VbatPowerOff() { esp_io_expander_set_level(io_expander_, vbatPowerPin_, 0); }
