#include "adc_battery_monitor.h"

#include <cstdlib>

AdcBatteryMonitor::AdcBatteryMonitor(adc_unit_t adc_unit, adc_channel_t adc_channel,
                                     float upper_resistor, float lower_resistor,
                                     gpio_num_t charging_pin)
    : charging_pin_(charging_pin), voltage_adc_channel_(adc_channel) {
    // Initialize charging pin (only if it's not NC)
    if (charging_pin_ != GPIO_NUM_NC) {
        gpio_config_t gpio_cfg = {
            .pin_bit_mask = 1ULL << charging_pin,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
    }

    voltage_divider_ratio_ = lower_resistor / (upper_resistor + lower_resistor);

    adc_oneshot_unit_init_cfg_t unit_cfg = {};
    unit_cfg.unit_id = adc_unit;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &voltage_adc_handle_));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(voltage_adc_handle_, adc_channel, &chan_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = adc_unit,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &voltage_adc_cali_handle_) == ESP_OK) {
        voltage_adc_cali_enabled_ = true;
    } else {
        voltage_adc_cali_handle_ = nullptr;
    }

    adc_battery_estimation_t adc_cfg = {.external =
                                            {
                                                .adc_handle = voltage_adc_handle_,
                                                .adc_cali_handle = voltage_adc_cali_handle_,
                                            },
                                        .adc_channel = adc_channel,
                                        .upper_resistor = upper_resistor,
                                        .lower_resistor = lower_resistor};

    if (charging_pin_ != GPIO_NUM_NC) {
        adc_cfg.charging_detect_cb = [](void* user_data) -> bool {
            AdcBatteryMonitor* self = (AdcBatteryMonitor*)user_data;
            return gpio_get_level(self->charging_pin_) == 1;
        };
        adc_cfg.charging_detect_user_data = this;
    } else {
        adc_cfg.charging_detect_cb = nullptr;
        adc_cfg.charging_detect_user_data = nullptr;
    }
    adc_battery_estimation_handle_ = adc_battery_estimation_create(&adc_cfg);

    // Initialize timer
    esp_timer_create_args_t timer_cfg = {
        .callback =
            [](void* arg) {
                AdcBatteryMonitor* self = (AdcBatteryMonitor*)arg;
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

    if (voltage_adc_cali_enabled_ && voltage_adc_cali_handle_) {
        adc_cali_delete_scheme_curve_fitting(voltage_adc_cali_handle_);
    }
    if (voltage_adc_handle_) {
        adc_oneshot_del_unit(voltage_adc_handle_);
    }

    if (timer_handle_) {
        esp_timer_stop(timer_handle_);
        esp_timer_delete(timer_handle_);
    }
}

bool AdcBatteryMonitor::IsCharging() {
    return charge_state_ == ChargeState::kCharging || charge_state_ == ChargeState::kFull;
}

bool AdcBatteryMonitor::IsDischarging() { return charge_state_ == ChargeState::kDischarging; }

uint8_t AdcBatteryMonitor::GetBatteryLevel() {
    // 如果句柄无效，返回默认值
    if (adc_battery_estimation_handle_ == nullptr) {
        return 100;
    }

    float capacity = 0;
    esp_err_t err = adc_battery_estimation_get_capacity(adc_battery_estimation_handle_, &capacity);
    if (err != ESP_OK) {
        return 100;  // 出错时返回默认值
    }
    return (uint8_t)capacity;
}

int AdcBatteryMonitor::GetVoltageMv() {
    if (voltage_adc_handle_ == nullptr) {
        return -1;
    }
    int adc_raw = 0;
    esp_err_t err = adc_oneshot_read(voltage_adc_handle_, voltage_adc_channel_, &adc_raw);
    if (err != ESP_OK) {
        return -1;
    }
    int pin_mv = 0;
    if (voltage_adc_cali_enabled_) {
        err = adc_cali_raw_to_voltage(voltage_adc_cali_handle_, adc_raw, &pin_mv);
        if (err != ESP_OK) {
            return -1;
        }
    } else {
        pin_mv = (adc_raw * 3300) / 4095;
    }
    if (voltage_divider_ratio_ <= 0.0f) {
        return -1;
    }
    return (int)(pin_mv / voltage_divider_ratio_);
}

void AdcBatteryMonitor::OnChargingStatusChanged(std::function<void(bool)> callback) {
    on_charging_status_changed_ = callback;
}

void AdcBatteryMonitor::CheckBatteryStatus() {
    MaybeSampleVoltage();

    bool new_charging_status = IsCharging();
    if (new_charging_status != is_charging_) {
        is_charging_ = new_charging_status;
        if (on_charging_status_changed_) {
            on_charging_status_changed_(is_charging_);
        }
    }
}

void AdcBatteryMonitor::MaybeSampleVoltage() {
    int voltage_mv = GetVoltageMv();
    if (voltage_mv < 0) {
        return;
    }

    int64_t now_ms = esp_timer_get_time() / 1000;
    if (last_sample_ms_ != 0 && (now_ms - last_sample_ms_) < kSampleIntervalMs) {
        return;
    }

    if (sample_count_ > 0) {
        int prev_mv = GetNewestSampleMv();
        if (std::abs(voltage_mv - prev_mv) > 30) {
            last_sample_ms_ = now_ms;
            return;
        }
    }

    sample_buffer_[sample_buffer_index_] = {voltage_mv, now_ms};
    sample_buffer_index_ = (sample_buffer_index_ + 1) % kSampleBufferSize;
    if (sample_count_ < kSampleBufferSize) {
        sample_count_++;
    }
    last_sample_ms_ = now_ms;

    if (sample_count_ >= kSampleBufferSize) {
        int median_mv = GetMedianSampleMv();
        if (median_mv > peak_voltage_mv_) {
            peak_voltage_mv_ = median_mv;
        }
        UpdateChargeState();
    }
}

void AdcBatteryMonitor::UpdateChargeState() {
    int delta_mv = ComputeAverageDeltaMv();
    int median_mv = GetMedianSampleMv();
    int64_t now_ms = esp_timer_get_time() / 1000;

    if (median_mv >= kFullLikelyThresholdMv) {
        if (high_voltage_since_ms_ == 0) {
            high_voltage_since_ms_ = now_ms;
        }
    } else {
        high_voltage_since_ms_ = 0;
    }

    switch (charge_state_) {
        case ChargeState::kIdle:
            if (delta_mv > kRisingDeltaMv) {
                charge_state_ = ChargeState::kCharging;
            } else if (delta_mv < kFallingDeltaMv) {
                charge_state_ = ChargeState::kDischarging;
            } else if (high_voltage_since_ms_ != 0 &&
                       (now_ms - high_voltage_since_ms_) >= kFullLikelySustainedMs) {
                charge_state_ = ChargeState::kFull;
            }
            break;

        case ChargeState::kCharging:
            if (median_mv >= kFullThresholdMv && std::abs(delta_mv) <= 2) {
                charge_state_ = ChargeState::kFull;
            } else if (std::abs(delta_mv) <= 2) {
                charge_state_ = ChargeState::kIdle;
            } else if (delta_mv < kFallingDeltaMv) {
                charge_state_ = ChargeState::kDischarging;
            }
            break;

        case ChargeState::kFull:
            if (median_mv <= peak_voltage_mv_ - kPeakDropMv) {
                charge_state_ = ChargeState::kDischarging;
            }
            break;

        case ChargeState::kDischarging:
            if (delta_mv > kRisingDeltaMv) {
                charge_state_ = ChargeState::kCharging;
            } else if (std::abs(delta_mv) <= 2) {
                charge_state_ = ChargeState::kIdle;
            }
            break;
    }
}

int AdcBatteryMonitor::ComputeAverageDeltaMv() const {
    if (sample_count_ < kSampleBufferSize) {
        return 0;
    }
    int newest = GetNewestSampleMv();
    int oldest = GetOldestSampleMv();
    return (newest - oldest) / (kSampleBufferSize - 1);
}

int AdcBatteryMonitor::GetNewestSampleMv() const {
    if (sample_count_ == 0) {
        return 0;
    }
    int idx = (sample_buffer_index_ - 1 + kSampleBufferSize) % kSampleBufferSize;
    return sample_buffer_[idx].voltage_mv;
}

int AdcBatteryMonitor::GetOldestSampleMv() const {
    if (sample_count_ == 0) {
        return 0;
    }
    if (sample_count_ < kSampleBufferSize) {
        return sample_buffer_[0].voltage_mv;
    }
    return sample_buffer_[sample_buffer_index_].voltage_mv;
}

int AdcBatteryMonitor::GetMedianSampleMv() const {
    int n = sample_count_;
    if (n == 0) {
        return 0;
    }
    int sorted[kSampleBufferSize];
    for (int i = 0; i < n; i++) {
        sorted[i] = sample_buffer_[i].voltage_mv;
    }
    for (int i = 1; i < n; i++) {
        int key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    return sorted[n / 2];
}