#ifndef ADC_BATTERY_MONITOR_H
#define ADC_BATTERY_MONITOR_H

#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_timer.h>
#include <adc_battery_estimation.h>
#include <functional>

class AdcBatteryMonitor {
public:
    AdcBatteryMonitor(adc_unit_t adc_unit, adc_channel_t adc_channel, float upper_resistor,
                      float lower_resistor, gpio_num_t charging_pin = GPIO_NUM_NC);
    ~AdcBatteryMonitor();

    bool IsCharging();
    bool IsDischarging();
    uint8_t GetBatteryLevel();
    int GetVoltageMv();

    void OnChargingStatusChanged(std::function<void(bool)> callback);

private:
    enum class ChargeState {
        kIdle,
        kCharging,
        kFull,
        kDischarging,
    };

    struct VoltageSample {
        int voltage_mv;
        int64_t timestamp_ms;
    };

    static constexpr int kSampleBufferSize = 5;
    static constexpr int kSampleIntervalMs = 5000;
    static constexpr int kFullThresholdMv = 4180;
    static constexpr int kFullLikelyThresholdMv = 4150;
    static constexpr int kFullLikelySustainedMs = 60000;
    static constexpr int kRisingDeltaMv = 5;
    static constexpr int kFallingDeltaMv = -5;
    static constexpr int kPeakDropMv = 30;

    gpio_num_t charging_pin_;
    adc_channel_t voltage_adc_channel_;
    adc_battery_estimation_handle_t adc_battery_estimation_handle_ = nullptr;
    adc_oneshot_unit_handle_t voltage_adc_handle_ = nullptr;
    adc_cali_handle_t voltage_adc_cali_handle_ = nullptr;
    bool voltage_adc_cali_enabled_ = false;
    float voltage_divider_ratio_ = 0.0f;
    esp_timer_handle_t timer_handle_ = nullptr;
    bool is_charging_ = false;
    std::function<void(bool)> on_charging_status_changed_;

    ChargeState charge_state_ = ChargeState::kIdle;
    VoltageSample sample_buffer_[kSampleBufferSize] = {};
    int sample_count_ = 0;
    int sample_buffer_index_ = 0;
    int64_t last_sample_ms_ = 0;
    int peak_voltage_mv_ = 0;
    int64_t high_voltage_since_ms_ = 0;

    void CheckBatteryStatus();
    void MaybeSampleVoltage();
    void UpdateChargeState();
    int ComputeAverageDeltaMv() const;
    int GetOldestSampleMv() const;
    int GetNewestSampleMv() const;
    int GetMedianSampleMv() const;
};

#endif  // ADC_BATTERY_MONITOR_H
