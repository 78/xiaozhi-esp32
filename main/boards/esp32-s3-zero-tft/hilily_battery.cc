#include "hilily_battery.h"

#if CONFIG_HILILY_BATTERY_MONITOR
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>
#include <algorithm>

namespace {
constexpr const char* TAG = "HiLilyBattery";
constexpr adc_channel_t PIN_CHANNEL = ADC_CHANNEL_9;  // ESP32-S3 GPIO10.
constexpr float SOC_VOLTAGE[] = {3000,3300,3500,3600,3700,3800,3900,4000,4100,4200};
constexpr int SOC_PERCENT[] = {0,10,20,35,50,65,75,85,95,100};
}

HiLilyBattery::HiLilyBattery() {
    adc_oneshot_unit_init_cfg_t config = {};
    config.unit_id = ADC_UNIT_1;
    if (adc_oneshot_new_unit(&config, &adc_) != ESP_OK) return;
    adc_oneshot_chan_cfg_t channel = {};
    channel.bitwidth = ADC_BITWIDTH_DEFAULT;
    channel.atten = ADC_ATTEN_DB_12;
    if (adc_oneshot_config_channel(adc_, PIN_CHANNEL, &channel) != ESP_OK) return;

    adc_cali_curve_fitting_config_t curve = {};
    curve.unit_id = ADC_UNIT_1;
    curve.chan = PIN_CHANNEL;
    curve.atten = ADC_ATTEN_DB_12;
    curve.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting(&curve, &calibration_) != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration unavailable; hide battery level");
        return;
    }
    ready_ = true;
}

HiLilyBattery::~HiLilyBattery() {
    if (calibration_) adc_cali_delete_scheme_curve_fitting(calibration_);
    if (adc_) adc_oneshot_del_unit(adc_);
}

bool HiLilyBattery::ReadLevel(int& percentage) {
    if (!ready_) return false;
    int total_mv = 0;
    for (int i = 0; i < 8; ++i) {
        int raw = 0, mv = 0;
        if (adc_oneshot_read(adc_, PIN_CHANNEL, &raw) != ESP_OK ||
            adc_cali_raw_to_voltage(calibration_, raw, &mv) != ESP_OK) return false;
        total_mv += mv;
    }
    // Two matched 100k resistors: ADC voltage = single-cell VBAT / 2.
    const float cell_mv = total_mv / 8.0f * 2.0f;
    if (cell_mv < 2000 || cell_mv > 4400) {
        ESP_LOGW(TAG, "Battery divider out of range: %.0fmV", cell_mv);
        return false;
    }
    constexpr size_t N = sizeof(SOC_PERCENT)/sizeof(SOC_PERCENT[0]);
    if (cell_mv <= SOC_VOLTAGE[0]) { percentage = 0; return true; }
    for (size_t i = 1; i < N; ++i) {
        if (cell_mv <= SOC_VOLTAGE[i]) {
            const float portion = (cell_mv - SOC_VOLTAGE[i-1]) /
                                  (SOC_VOLTAGE[i] - SOC_VOLTAGE[i-1]);
            percentage = SOC_PERCENT[i-1] +
                         static_cast<int>(portion * (SOC_PERCENT[i] - SOC_PERCENT[i-1]) + 0.5f);
            return true;
        }
    }
    percentage = 100;
    return true;
}
#endif
