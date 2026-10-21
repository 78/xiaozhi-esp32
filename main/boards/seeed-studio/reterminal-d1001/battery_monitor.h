#ifndef RETERMINAL_D1001_BATTERY_MONITOR_H
#define RETERMINAL_D1001_BATTERY_MONITOR_H

#include <esp_adc/adc_oneshot.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>

class ReTerminalD1001Expander;

// Battery state sampling per the Seeed BSP wiring:
//   - GPIO18 / ADC1_CH2: battery voltage through a 1:2 divider, gated by
//     expander bit 6 (BAT_READ_EN)
//   - GPIO17 / ADC1_CH1: USB VBUS through a 1:2 divider (> 4V = USB present)
//   - GPIO15: BQ25616 STAT, open-drain (low = charging, high = done/idle)
// A 1 Hz task keeps a moving average; the percentage comes from the BSP's
// 21-point discharge table.
class ReTerminalD1001BatteryMonitor {
public:
    explicit ReTerminalD1001BatteryMonitor(ReTerminalD1001Expander* expander);
    ~ReTerminalD1001BatteryMonitor();

    bool Initialize();
    // Returns false until the first sample is ready.
    bool GetLevel(int& level, bool& charging, bool& discharging);

private:
    void SampleLoop();
    int ReadMilliVolts(adc_channel_t channel, adc_cali_handle_t cali);

    ReTerminalD1001Expander* expander_;
    adc_oneshot_unit_handle_t adc_ = nullptr;
    adc_cali_handle_t cali_usb_ = nullptr;
    adc_cali_handle_t cali_bat_ = nullptr;
    TaskHandle_t task_ = nullptr;
    std::atomic<int> level_{-1};
    std::atomic<bool> charging_{false};
    std::atomic<bool> usb_present_{false};
    std::atomic<bool> running_{false};
};

#endif  // RETERMINAL_D1001_BATTERY_MONITOR_H
