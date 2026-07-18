#include "charge_status.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include "config.h"

void ChargeStatus::Init(gpio_num_t detect_gpio, gpio_num_t full_gpio, int64_t now_ms) {
    detect_gpio_ = detect_gpio;
    full_gpio_ = full_gpio;

    gpio_config_t cfg = {};
    cfg.intr_type = GPIO_INTR_DISABLE;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pin_bit_mask = (1ULL << detect_gpio_) | (1ULL << full_gpio_);
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&cfg));

    UpdateSnapshot(State::kNoPower, false, false, false);
    Tick(now_ms);
}

void ChargeStatus::OnStateChanged(std::function<void(const Snapshot&)> cb) {
    on_state_changed_ = cb;
}

void ChargeStatus::Tick(int64_t now_ms) {
    const bool detect_high = gpio_get_level(detect_gpio_) == CHARGE_DETECT_CHARGING_LEVEL;
    const bool full_high = gpio_get_level(full_gpio_) == 1;

    if (detect_high) {
        last_power_present_ms_ = now_ms;
        last_detect_seen_ms_ = now_ms;
        if (detect_high_start_ms_ < 0) {
            detect_high_start_ms_ = now_ms;
        }
    } else {
        detect_high_start_ms_ = -1;
    }

    if (full_high) {
        last_power_present_ms_ = now_ms;
        last_full_seen_ms_ = now_ms;
        if (full_high_start_ms_ < 0) {
            full_high_start_ms_ = now_ms;
        }
    } else {
        full_high_start_ms_ = -1;
    }

    const bool power_present = (last_power_present_ms_ >= 0) &&
        ((now_ms - last_power_present_ms_) <= kPowerPresentHoldMs);

    const bool detect_stable = (detect_high_start_ms_ >= 0) &&
        ((now_ms - detect_high_start_ms_) >= kStableHighMs);
    const bool full_stable = (full_high_start_ms_ >= 0) &&
        ((now_ms - full_high_start_ms_) >= kStableHighMs);

    const bool alt_seen = power_present &&
        (last_detect_seen_ms_ >= 0) && (last_full_seen_ms_ >= 0) &&
        ((now_ms - last_detect_seen_ms_) <= kAltWindowMs) &&
        ((now_ms - last_full_seen_ms_) <= kAltWindowMs);

    const bool no_battery = alt_seen && !detect_stable && !full_stable;

    State state = State::kNoPower;
    if (!power_present) {
        state = State::kNoPower;
    } else if (full_stable && !no_battery) {
        state = State::kFull;
    } else if (detect_stable || no_battery) {
        state = no_battery ? State::kNoBattery : State::kCharging;
    } else {
        state = State::kCharging;
    }

    UpdateSnapshot(state, power_present, state == State::kFull, no_battery);
}

void ChargeStatus::UpdateSnapshot(State state, bool power_present, bool full, bool no_battery) {
    const bool charging = (state == State::kCharging || state == State::kNoBattery);
    const uint32_t packed = Pack(state, power_present, charging, full, no_battery);
    const uint32_t old = snapshot_.exchange(packed, std::memory_order_relaxed);
    if (old != packed && on_state_changed_) {
        on_state_changed_(Unpack(packed));
    }
}

uint32_t ChargeStatus::Pack(State state, bool power_present, bool charging, bool full, bool no_battery) {
    return (uint32_t)state |
        ((uint32_t)power_present << 8) |
        ((uint32_t)charging << 9) |
        ((uint32_t)full << 10) |
        ((uint32_t)no_battery << 11);
}

ChargeStatus::Snapshot ChargeStatus::Unpack(uint32_t v) {
    Snapshot s{};
    s.state = static_cast<State>(v & 0xFF);
    s.power_present = (v >> 8) & 0x1;
    s.charging = (v >> 9) & 0x1;
    s.full = (v >> 10) & 0x1;
    s.no_battery = (v >> 11) & 0x1;
    return s;
}

ChargeStatus::Snapshot ChargeStatus::Get() const {
    return Unpack(snapshot_.load(std::memory_order_relaxed));
}
