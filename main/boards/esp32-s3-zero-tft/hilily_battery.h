#ifndef HILILY_BATTERY_H
#define HILILY_BATTERY_H

#if CONFIG_HILILY_BATTERY_MONITOR
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_oneshot.h>

class HiLilyBattery {
public:
    HiLilyBattery();
    ~HiLilyBattery();
    bool ReadLevel(int& percentage);

private:
    bool ready_ = false;
    adc_oneshot_unit_handle_t adc_ = nullptr;
    adc_cali_handle_t calibration_ = nullptr;
};
#endif
#endif  // HILILY_BATTERY_H
