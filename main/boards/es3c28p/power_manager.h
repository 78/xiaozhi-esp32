#pragma once
#include <vector>
#include <functional>

#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>

// ES3C28P battery monitor
// GPIO9 = ADC1 channel 8, voltage divider on board
// No charging detection pin exposed on this board

class PowerManager {
private:
    esp_timer_handle_t timer_handle_;
    std::function<void(bool)> on_charging_status_changed_;
    std::function<void(bool)> on_low_battery_status_changed_;

    std::vector<uint16_t> adc_values_;
    uint32_t battery_level_ = 0;
    bool is_low_battery_ = false;
    int ticks_ = 0;
    const int kBatteryAdcInterval = 60;
    const int kBatteryAdcDataCount = 3;
    const int kLowBatteryLevel = 20;

    adc_oneshot_unit_handle_t adc_handle_;
    adc_channel_t adc_channel_;

    void CheckBatteryStatus() {
        if (adc_values_.size() < kBatteryAdcDataCount) {
            ReadBatteryAdcData();
            return;
        }
        ticks_++;
        if (ticks_ % kBatteryAdcInterval == 0) {
            ReadBatteryAdcData();
        }
    }

    void ReadBatteryAdcData() {
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, adc_channel_, &adc_value));

        adc_values_.push_back(adc_value);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        uint32_t average_adc = 0;
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();

        // ADC levels for 3.7V LiPo via resistor divider
        // Adjust these values based on your actual hardware measurements
        const struct {
            uint16_t adc;
            uint8_t level;
        } levels[] = {
            {1980, 0},
            {2081, 20},
            {2163, 40},
            {2250, 60},
            {2340, 80},
            {2480, 100}
        };

        if (average_adc < levels[0].adc) {
            battery_level_ = 0;
        } else if (average_adc >= levels[5].adc) {
            battery_level_ = 100;
        } else {
            for (int i = 0; i < 5; i++) {
                if (average_adc >= levels[i].adc && average_adc < levels[i+1].adc) {
                    float ratio = static_cast<float>(average_adc - levels[i].adc) / 
                                  (levels[i+1].adc - levels[i].adc);
                    battery_level_ = levels[i].level + ratio * (levels[i+1].level - levels[i].level);
                    break;
                }
            }
        }

        if (adc_values_.size() >= kBatteryAdcDataCount) {
            bool new_low_battery = battery_level_ <= kLowBatteryLevel;
            if (new_low_battery != is_low_battery_) {
                is_low_battery_ = new_low_battery;
                if (on_low_battery_status_changed_) {
                    on_low_battery_status_changed_(is_low_battery_);
                }
            }
        }

        ESP_LOGI("PowerManager", "ADC: %d avg: %ld level: %ld%%", 
                 adc_value, average_adc, battery_level_);
    }

public:
    PowerManager(adc_channel_t adc_channel) : adc_channel_(adc_channel) {
        // Timer for periodic battery checks
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                static_cast<PowerManager*>(arg)->CheckBatteryStatus();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_check_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 100000)); // 100ms

        // ADC init
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));

        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, adc_channel_, &chan_config));
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (adc_handle_) {
            adc_oneshot_del_unit(adc_handle_);
        }
    }

    bool IsCharging() { return false; }   // No charging pin on ES3C28P
    bool IsDischarging() { return true; }
    uint8_t GetBatteryLevel() { return battery_level_; }

    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) {
        on_low_battery_status_changed_ = callback;
    }

    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = callback;
    }
};
