#include "battery_monitor.h"

#include "reterminal_d1001_expander.h"

#include <driver/gpio.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

#include <algorithm>

static const char* TAG = "BatteryMon";

namespace {

constexpr gpio_num_t kChargeStateGpio = GPIO_NUM_15;   // BQ25616 STAT
constexpr adc_channel_t kUsbChannel = ADC_CHANNEL_1;   // GPIO17, VBUS / 2
constexpr adc_channel_t kBatChannel = ADC_CHANNEL_2;   // GPIO18, VBAT / 2
constexpr int kUsbPresentMv = 4000;
constexpr int kAvgWindow = 10;

// BSP 21-point discharge table (battery millivolts at 0%, 5%, ... 100%).
constexpr int kLevelTable[21] = {3262, 3390, 3467, 3554, 3619, 3659, 3686,
                                 3710, 3731, 3752, 3774, 3797, 3827, 3855,
                                 3880, 3901, 3915, 3934, 3958, 3978, 4047};

int VoltageToPercent(int mv) {
    if (mv <= kLevelTable[0]) {
        return 0;
    }
    if (mv >= kLevelTable[20]) {
        return 100;
    }
    for (int i = 1; i < 21; ++i) {
        if (mv < kLevelTable[i]) {
            int lo = kLevelTable[i - 1], hi = kLevelTable[i];
            return (i - 1) * 5 + 5 * (mv - lo) / (hi - lo);
        }
    }
    return 100;
}

bool MakeCali(adc_channel_t channel, adc_cali_handle_t* out) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t config = {
        .unit_id = ADC_UNIT_1,
        .chan = channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    return adc_cali_create_scheme_curve_fitting(&config, out) == ESP_OK;
#else
    return false;
#endif
}

}  // namespace

ReTerminalD1001BatteryMonitor::ReTerminalD1001BatteryMonitor(ReTerminalD1001Expander* expander)
    : expander_(expander) {}

ReTerminalD1001BatteryMonitor::~ReTerminalD1001BatteryMonitor() {
    running_ = false;
}

bool ReTerminalD1001BatteryMonitor::Initialize() {
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    if (adc_oneshot_new_unit(&unit_cfg, &adc_) != ESP_OK) {
        ESP_LOGW(TAG, "ADC1 unavailable, battery status disabled");
        return false;
    }
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_, kUsbChannel, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_, kBatChannel, &chan_cfg));
    MakeCali(kUsbChannel, &cali_usb_);
    MakeCali(kBatChannel, &cali_bat_);

    // BQ25616 STAT is open-drain.
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << kChargeStateGpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    // Enable the battery voltage divider (expander bit 6). Kept on: the
    // divider drain is negligible next to the running system.
    expander_->SetBatteryReadEnable(true);

    // The PCA9535 powers up with all outputs high, which holds CHG_ENBn
    // deasserted: the charger was never enabled and the battery only ever
    // drained. Enable it; the BQ25616 handles CC/CV termination itself.
    expander_->SetBatteryChargeEnable(true);

    running_ = true;
    xTaskCreate(
        [](void* arg) {
            static_cast<ReTerminalD1001BatteryMonitor*>(arg)->SampleLoop();
            vTaskDelete(nullptr);
        },
        "battery_mon", 3072, this, 2, &task_);
    return true;
}

int ReTerminalD1001BatteryMonitor::ReadMilliVolts(adc_channel_t channel, adc_cali_handle_t cali) {
    int raw = 0;
    if (adc_oneshot_read(adc_, channel, &raw) != ESP_OK) {
        return -1;
    }
    int mv = 0;
    if (cali != nullptr && adc_cali_raw_to_voltage(cali, raw, &mv) == ESP_OK) {
        return mv * 2;  // 1:2 divider on both rails
    }
    // Uncalibrated fallback: 12-bit full scale ~= 3100 mV at 12 dB.
    return raw * 3100 / 4095 * 2;
}

void ReTerminalD1001BatteryMonitor::SampleLoop() {
    int window[kAvgWindow] = {0};
    int index = 0, count = 0;
    while (running_) {
        int usb_mv = ReadMilliVolts(kUsbChannel, cali_usb_);
        int bat_mv = ReadMilliVolts(kBatChannel, cali_bat_);
        if (bat_mv > 0) {
            window[index] = bat_mv;
            index = (index + 1) % kAvgWindow;
            count = std::min(count + 1, kAvgWindow);
            long sum = 0;
            for (int i = 0; i < count; ++i) {
                sum += window[i];
            }
            int avg = static_cast<int>(sum / count);
            level_.store(VoltageToPercent(avg));
        }
        bool usb = usb_mv > kUsbPresentMv;
        usb_present_.store(usb);
        // STAT low while USB present = actively charging.
        int stat = gpio_get_level(kChargeStateGpio);
        charging_.store(usb && stat == 0);
        static int log_tick = 0;
        if (++log_tick >= 30) {
            log_tick = 0;
            ESP_LOGI(TAG, "bat=%dmV level=%d%% usb=%dmV stat=%d charging=%d", bat_mv,
                     level_.load(), usb_mv, stat, (int)charging_.load());
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool ReTerminalD1001BatteryMonitor::GetLevel(int& level, bool& charging, bool& discharging) {
    int current = level_.load();
    if (current < 0) {
        return false;
    }
    level = current;
    charging = charging_.load();
    discharging = !usb_present_.load();
    return true;
}
