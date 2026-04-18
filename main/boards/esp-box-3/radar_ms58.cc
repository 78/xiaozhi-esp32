#include "radar_ms58.h"
#include "config.h"

#include <esp_log.h>
#include <esp_timer.h>

#define TAG "RadarMs58"

RadarMs58::RadarMs58() : out_pin_(SENSOR_RADAR_OUT_PIN), last_motion_at_ms_(-1), ok_(false) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << out_pin_;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;  // active-high signal; idle low
    io_conf.intr_type = GPIO_INTR_DISABLE;
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return;
    }
    ok_ = true;
}

RadarMs58::~RadarMs58() {
    // GPIO is left in input mode; no allocation to free.
}

esp_err_t RadarMs58::Read(bool* present, int64_t* last_motion_at_ms) {
    if (!ok_) {
        return ESP_ERR_INVALID_STATE;
    }

    int level = gpio_get_level(out_pin_);
    bool now_present = (level == 1);

    if (now_present) {
        last_motion_at_ms_ = esp_timer_get_time() / 1000;
    }

    *present = now_present;
    *last_motion_at_ms = last_motion_at_ms_;
    return ESP_OK;
}
