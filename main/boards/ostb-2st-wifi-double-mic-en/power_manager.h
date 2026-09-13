#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_timer.h>

// OSTB hardware configuration. GPIO47 is the charge-present input; battery
// voltage is sampled through ADC2 channel 6.
constexpr gpio_num_t OSTB_CHARGE_DETECT_GPIO = GPIO_NUM_47;
// Vendor constructor passes unit_id = 1, which is ADC_UNIT_2 in ESP-IDF.
constexpr adc_unit_t OSTB_BATTERY_ADC_UNIT = ADC_UNIT_2;
constexpr adc_channel_t OSTB_BATTERY_ADC_CHANNEL = ADC_CHANNEL_6;
constexpr int64_t OSTB_BATTERY_SAMPLE_PERIOD_US = 1'000'000;

class PowerManager {
public:
    using StatusCallback = std::function<void(bool)>;

    explicit PowerManager(gpio_num_t charge_detect_gpio = OSTB_CHARGE_DETECT_GPIO)
        : charge_detect_gpio_(charge_detect_gpio) {
        ConfigureChargeDetect();
        CreateBatteryTimer();
        ConfigureBatteryAdc();
    }

    ~PowerManager() {
        if (timer_handle_ != nullptr) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (adc_handle_ != nullptr) {
            adc_oneshot_del_unit(adc_handle_);
        }
    }

    PowerManager(const PowerManager&) = delete;
    PowerManager& operator=(const PowerManager&) = delete;

    bool IsCharging() const { return is_charging_; }
    bool IsDischarging() const { return !is_charging_; }
    uint8_t GetBatteryLevel() const { return battery_level_; }

    void OnChargingStatusChanged(StatusCallback callback) {
        charging_callback_ = std::move(callback);
    }

    void OnLowBatteryStatusChanged(StatusCallback callback) {
        low_battery_callback_ = std::move(callback);
    }

private:
    struct CalibrationPoint {
        uint16_t adc;
        uint8_t percent;
    };

    // Battery voltage calibration for the OSTB divider (six ADC/percent pairs).
    static constexpr CalibrationPoint kCalibration[] = {
        {1985, 0}, {2048, 20}, {2172, 40},
        {2296, 60}, {2420, 80}, {2544, 100},
    };

    static constexpr size_t kSampleCount = 3;
    static constexpr uint8_t kLowBatteryPercent = 20;
    static constexpr const char* kTag = "PowerManager";

    esp_timer_handle_t timer_handle_ = nullptr;
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    gpio_num_t charge_detect_gpio_;
    std::vector<uint16_t> samples_;
    StatusCallback charging_callback_;
    StatusCallback low_battery_callback_;
    uint8_t battery_level_ = 0;
    bool is_charging_ = false;
    bool is_low_battery_ = false;

    void ConfigureChargeDetect() {
        gpio_config_t config{};
        config.intr_type = GPIO_INTR_DISABLE;
        config.mode = GPIO_MODE_INPUT;
        config.pin_bit_mask = 1ULL << charge_detect_gpio_;
        config.pull_up_en = GPIO_PULLUP_ENABLE;
        ESP_ERROR_CHECK(gpio_config(&config));
    }

    void CreateBatteryTimer() {
        esp_timer_create_args_t args{};
        args.callback = [](void* context) {
            static_cast<PowerManager*>(context)->CheckBatteryStatus();
        };
        args.arg = this;
        args.dispatch_method = ESP_TIMER_TASK;
        args.name = "battery_check_timer";
        args.skip_unhandled_events = true;
        ESP_ERROR_CHECK(esp_timer_create(&args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, OSTB_BATTERY_SAMPLE_PERIOD_US));
    }

    void ConfigureBatteryAdc() {
        adc_oneshot_unit_init_cfg_t unit_config{};
        unit_config.unit_id = OSTB_BATTERY_ADC_UNIT;
        unit_config.ulp_mode = ADC_ULP_MODE_DISABLE;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc_handle_));

        adc_oneshot_chan_cfg_t channel_config{};
        channel_config.atten = ADC_ATTEN_DB_12;
        channel_config.bitwidth = ADC_BITWIDTH_12;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(
            adc_handle_, OSTB_BATTERY_ADC_CHANNEL, &channel_config));
    }

    void CheckBatteryStatus() {
        const bool charging = gpio_get_level(charge_detect_gpio_) == 0;
        if (charging != is_charging_) {
            is_charging_ = charging;
            if (charging_callback_) {
                charging_callback_(is_charging_);
            }
        }
        ReadBatteryAdcData();
    }

    void ReadBatteryAdcData() {
        int raw_adc = 0;
        const esp_err_t error = adc_oneshot_read(
            adc_handle_, OSTB_BATTERY_ADC_CHANNEL, &raw_adc);
        if (error != ESP_OK) {
            ESP_LOGE(kTag, "Battery ADC read failed: %s", esp_err_to_name(error));
            return;
        }

        samples_.push_back(static_cast<uint16_t>(raw_adc));
        if (samples_.size() > kSampleCount) {
            samples_.erase(samples_.begin());
        }

        uint32_t sum = 0;
        for (const uint16_t sample : samples_) {
            sum += sample;
        }
        const uint32_t average = sum / samples_.size();
        battery_level_ = CalculatePercentage(average);

        if (samples_.size() == kSampleCount) {
            const bool low_battery = battery_level_ <= kLowBatteryPercent;
            if (low_battery != is_low_battery_) {
                is_low_battery_ = low_battery;
                if (low_battery_callback_) {
                    low_battery_callback_(is_low_battery_);
                }
            }
        }

        ESP_LOGI(kTag, "ADC value: %d average: %lu level: %u",
                 raw_adc, static_cast<unsigned long>(average), battery_level_);
    }

    static uint8_t CalculatePercentage(uint32_t adc) {
        if (adc < kCalibration[0].adc) {
            return 0;
        }
        if (adc >= kCalibration[5].adc) {
            return 100;
        }
        for (size_t index = 0; index < 5; ++index) {
            const auto& lower = kCalibration[index];
            const auto& upper = kCalibration[index + 1];
            if (adc < upper.adc) {
                const uint32_t span = upper.adc - lower.adc;
                const uint32_t position = adc - lower.adc;
                return static_cast<uint8_t>(lower.percent +
                    (position * (upper.percent - lower.percent)) / span);
            }
        }
        return 100;
    }
};
