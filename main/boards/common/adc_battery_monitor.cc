#include "adc_battery_monitor.h"

AdcBatteryMonitor::AdcBatteryMonitor(adc_unit_t adc_unit, adc_channel_t adc_channel, float upper_resistor, float lower_resistor, gpio_num_t charging_pin)
    : charging_pin_(charging_pin) {
    
    // Initialize charging pin
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = 1ULL << charging_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_cfg));

    // Initialize ADC battery estimation
    adc_battery_estimation_t adc_cfg = {
        .internal = {
            .adc_unit = adc_unit,
            .adc_bitwidth = ADC_BITWIDTH_12,
            .adc_atten = ADC_ATTEN_DB_12,
        },
        .adc_channel = adc_channel,
        .upper_resistor = upper_resistor,
        .lower_resistor = lower_resistor
    };
    adc_cfg.charging_detect_cb = [](void *user_data) -> bool {
        AdcBatteryMonitor *self = (AdcBatteryMonitor *)user_data;
        return gpio_get_level(self->charging_pin_) == 1;
    };
    adc_cfg.charging_detect_user_data = this;
    adc_battery_estimation_handle_ = adc_battery_estimation_create(&adc_cfg);

    // Initialize timer
    esp_timer_create_args_t timer_cfg = {
        .callback = [](void *arg) {
            AdcBatteryMonitor *self = (AdcBatteryMonitor *)arg;
            self->CheckBatteryStatus();
        },
        .arg = this,
        .name = "adc_battery_monitor",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_cfg, &timer_handle_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));
}

AdcBatteryMonitor::~AdcBatteryMonitor() {
    if (adc_battery_estimation_handle_) {
        ESP_ERROR_CHECK(adc_battery_estimation_destroy(adc_battery_estimation_handle_));
    }
}

bool AdcBatteryMonitor::IsCharging() {
    bool is_charging = false;
    ESP_ERROR_CHECK(adc_battery_estimation_get_charging_state(adc_battery_estimation_handle_, &is_charging));
    return is_charging;
}

bool AdcBatteryMonitor::IsDischarging() {
    return !IsCharging();
}

uint8_t AdcBatteryMonitor::GetBatteryLevel() {
    float capacity = 0;
    ESP_ERROR_CHECK(adc_battery_estimation_get_capacity(adc_battery_estimation_handle_, &capacity));
    return capacity;
}

void AdcBatteryMonitor::OnChargingStatusChanged(std::function<void(bool)> callback) {
    on_charging_status_changed_ = callback;
}

void AdcBatteryMonitor::CheckBatteryStatus() {
    bool new_charging_status = IsCharging();
    if (new_charging_status != is_charging_) {
        is_charging_ = new_charging_status;
        if (on_charging_status_changed_) {
            on_charging_status_changed_(is_charging_);
        }
    }
}