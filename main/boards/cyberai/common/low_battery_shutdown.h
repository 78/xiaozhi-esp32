#pragma once

#include <cstdint>

namespace cyberai {

// Consumes fresh samples from the board's two-second battery worker only.
class LowBatteryShutdown {
public:
    LowBatteryShutdown(int max_percent, int max_voltage_mv, int64_t confirm_ms)
        : max_percent_(max_percent), max_voltage_mv_(max_voltage_mv), confirm_ms_(confirm_ms) {}

    bool Update(bool valid, int percent, int voltage_mv, bool charging, bool discharging,
                int64_t now_ms) {
        if (!valid || charging || !discharging || percent < 0 || percent > max_percent_ ||
            voltage_mv < 2000 || voltage_mv > max_voltage_mv_ || now_ms < 0) {
            Reset();
            return false;
        }

        // A delayed worker cannot establish that the low readings were continuous.
        if (low_since_ms_ < 0 || now_ms < last_sample_ms_ ||
            now_ms - last_sample_ms_ > kMaxSampleGapMs) {
            low_since_ms_ = now_ms;
        }
        last_sample_ms_ = now_ms;
        return now_ms - low_since_ms_ >= confirm_ms_;
    }

    void Reset() {
        low_since_ms_ = -1;
        last_sample_ms_ = -1;
    }

private:
    static constexpr int64_t kMaxSampleGapMs = 3000;
    const int max_percent_;
    const int max_voltage_mv_;
    const int64_t confirm_ms_;
    int64_t low_since_ms_ = -1;
    int64_t last_sample_ms_ = -1;
};

}  // namespace cyberai
