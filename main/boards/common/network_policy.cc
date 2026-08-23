#include "network_policy.h"

#include <algorithm>

const char* ToString(NetworkMode mode) {
    switch (mode) {
        case NetworkMode::Auto:
            return "auto";
        case NetworkMode::Wifi:
            return "wifi";
        case NetworkMode::Cellular:
            return "cellular";
    }
    return "auto";
}

const char* ToString(NetworkTransport transport) {
    switch (transport) {
        case NetworkTransport::None:
            return "none";
        case NetworkTransport::Wifi:
            return "wifi";
        case NetworkTransport::Cellular:
            return "cellular";
    }
    return "none";
}

const char* ToString(NetworkHealth health) {
    switch (health) {
        case NetworkHealth::Down:
            return "down";
        case NetworkHealth::Starting:
            return "starting";
        case NetworkHealth::LinkUp:
            return "link_up";
        case NetworkHealth::InternetReady:
            return "internet_ready";
        case NetworkHealth::Degraded:
            return "degraded";
    }
    return "down";
}

const char* ToString(NetworkSwitchReason reason) {
    switch (reason) {
        case NetworkSwitchReason::None:
            return "none";
        case NetworkSwitchReason::Startup:
            return "startup";
        case NetworkSwitchReason::ManualOverride:
            return "manual_override";
        case NetworkSwitchReason::WifiFailed:
            return "wifi_failed";
        case NetworkSwitchReason::WifiRecovered:
            return "wifi_recovered";
        case NetworkSwitchReason::CellularFailed:
            return "cellular_failed";
        case NetworkSwitchReason::PowerPolicy:
            return "power_policy";
        case NetworkSwitchReason::BothNetworksDown:
            return "both_networks_down";
        case NetworkSwitchReason::SwitchRateLimited:
            return "switch_rate_limited";
    }
    return "none";
}

bool ParseNetworkMode(const std::string& value, NetworkMode& mode) {
    if (value == "auto") {
        mode = NetworkMode::Auto;
        return true;
    }
    if (value == "wifi") {
        mode = NetworkMode::Wifi;
        return true;
    }
    if (value == "cellular" || value == "4g") {
        mode = NetworkMode::Cellular;
        return true;
    }
    return false;
}

NetworkPolicy::NetworkPolicy(NetworkPolicyConfig config) : config_(config) {}

void NetworkPolicy::SetMode(NetworkMode mode, uint64_t now_ms) {
    mode_ = mode;
    candidate_ = NetworkTransport::None;
    wifi_start_deadline_ms_ = now_ms + config_.wifi_start_window_ms;
    ResetRecoveryEvidence();
}

void NetworkPolicy::SetActive(NetworkTransport transport, uint64_t now_ms) {
    active_ = transport;
    active_since_ms_ = now_ms;
    candidate_ = NetworkTransport::None;
    if (transport == NetworkTransport::Cellular) {
        next_wifi_probe_at_ms_ = now_ms + config_.wifi_probe_interval_ms;
    }
    ResetRecoveryEvidence();
}

void NetworkPolicy::ReportHealth(NetworkTransport transport, NetworkHealth health,
                                 uint64_t now_ms) {
    if (transport == NetworkTransport::None) {
        return;
    }

    auto& state = StateFor(transport);
    state.value = health;
    if (IsFailure(health)) {
        if (state.consecutive_failures == 0) {
            state.first_failure_at_ms = now_ms;
        }
        ++state.consecutive_failures;
        if (transport == NetworkTransport::Wifi) {
            ResetRecoveryEvidence();
        }
    } else if (IsHealthy(health)) {
        state.consecutive_failures = 0;
        state.first_failure_at_ms = 0;
        if (transport == NetworkTransport::Wifi && active_ == NetworkTransport::Cellular &&
            health == NetworkHealth::InternetReady) {
            if (wifi_recovery_successes_ == 0 ||
                now_ms - last_wifi_recovery_success_at_ms_ >=
                    config_.wifi_recovery_success_gap_ms) {
                ++wifi_recovery_successes_;
                last_wifi_recovery_success_at_ms_ = now_ms;
            }
        }
    }
}

NetworkDecision NetworkPolicy::Evaluate(uint64_t now_ms) {
    NetworkTransport desired = NetworkTransport::None;
    NetworkSwitchReason reason = NetworkSwitchReason::None;

    if (mode_ == NetworkMode::Wifi) {
        if (active_ != NetworkTransport::Wifi && IsHealthy(wifi_.value)) {
            desired = NetworkTransport::Wifi;
            reason = NetworkSwitchReason::ManualOverride;
        }
    } else if (mode_ == NetworkMode::Cellular) {
        if (active_ != NetworkTransport::Cellular && IsHealthy(cellular_.value)) {
            desired = NetworkTransport::Cellular;
            reason = NetworkSwitchReason::ManualOverride;
        }
    } else if (active_ == NetworkTransport::None) {
        if (IsHealthy(wifi_.value)) {
            desired = NetworkTransport::Wifi;
            reason = NetworkSwitchReason::Startup;
        } else if (now_ms >= wifi_start_deadline_ms_ && IsHealthy(cellular_.value)) {
            desired = NetworkTransport::Cellular;
            reason = NetworkSwitchReason::Startup;
        }
    } else if (active_ == NetworkTransport::Wifi) {
        if (FailureConfirmed(wifi_, now_ms) &&
            cellular_.value == NetworkHealth::InternetReady) {
            desired = NetworkTransport::Cellular;
            reason = NetworkSwitchReason::WifiFailed;
        }
    } else if (active_ == NetworkTransport::Cellular) {
        if (FailureConfirmed(cellular_, now_ms) && wifi_.value == NetworkHealth::InternetReady) {
            desired = NetworkTransport::Wifi;
            reason = NetworkSwitchReason::CellularFailed;
        } else if (now_ms - active_since_ms_ >= config_.cellular_min_dwell_ms &&
                   wifi_recovery_successes_ >= config_.wifi_recovery_success_count &&
                   wifi_.value == NetworkHealth::InternetReady) {
            desired = NetworkTransport::Wifi;
            reason = NetworkSwitchReason::WifiRecovered;
        }
    }

    if (desired == NetworkTransport::None || desired == active_) {
        candidate_ = NetworkTransport::None;
        return {};
    }
    candidate_ = desired;
    if (!SwitchAllowed(now_ms)) {
        last_switch_reason_ = NetworkSwitchReason::SwitchRateLimited;
        return {};
    }
    return {.switch_requested = true, .target = desired, .reason = reason};
}

void NetworkPolicy::RecordSwitch(NetworkTransport transport, NetworkSwitchReason reason,
                                 uint64_t now_ms) {
    switch_times_ms_.push_back(now_ms);
    active_ = transport;
    active_since_ms_ = now_ms;
    candidate_ = NetworkTransport::None;
    last_switch_reason_ = reason;
    ++generation_;
    if (transport == NetworkTransport::Cellular) {
        next_wifi_probe_at_ms_ = now_ms + config_.wifi_probe_interval_ms;
    }
    ResetRecoveryEvidence();
}

NetworkStatusSnapshot NetworkPolicy::GetSnapshot() const {
    const bool wifi_ready = wifi_.value == NetworkHealth::InternetReady;
    const bool cellular_ready = cellular_.value == NetworkHealth::InternetReady;
    return {
        .mode = mode_,
        .active = active_,
        .candidate = candidate_,
        .wifi_health = wifi_.value,
        .cellular_health = cellular_.value,
        .last_switch_reason = last_switch_reason_,
        .generation = generation_,
        .offline = !wifi_ready && !cellular_ready,
        .switch_rate_limited = cooldown_until_ms_ != 0,
    };
}

void NetworkPolicy::MarkWifiProbeStarted(uint64_t now_ms) {
    next_wifi_probe_at_ms_ = now_ms + config_.wifi_probe_interval_ms;
}

bool NetworkPolicy::ShouldPowerCellular(bool external_power, NetworkMode mode,
                                        NetworkTransport active) {
    if (mode == NetworkMode::Cellular || active == NetworkTransport::Cellular) {
        return true;
    }
    return external_power;
}

NetworkPolicy::HealthState& NetworkPolicy::StateFor(NetworkTransport transport) {
    return transport == NetworkTransport::Wifi ? wifi_ : cellular_;
}

const NetworkPolicy::HealthState& NetworkPolicy::StateFor(NetworkTransport transport) const {
    return transport == NetworkTransport::Wifi ? wifi_ : cellular_;
}

bool NetworkPolicy::IsHealthy(NetworkHealth health) {
    return health == NetworkHealth::LinkUp || health == NetworkHealth::InternetReady;
}

bool NetworkPolicy::IsFailure(NetworkHealth health) {
    return health == NetworkHealth::Down || health == NetworkHealth::Degraded;
}

bool NetworkPolicy::FailureConfirmed(const HealthState& state, uint64_t now_ms) const {
    return state.consecutive_failures >= config_.active_failure_count &&
           now_ms - state.first_failure_at_ms >= config_.active_failure_window_ms;
}

bool NetworkPolicy::SwitchAllowed(uint64_t now_ms) {
    if (cooldown_until_ms_ != 0) {
        if (now_ms < cooldown_until_ms_) {
            return false;
        }
        cooldown_until_ms_ = 0;
        switch_times_ms_.clear();
    }

    while (!switch_times_ms_.empty() &&
           now_ms - switch_times_ms_.front() > config_.switch_window_ms) {
        switch_times_ms_.pop_front();
    }
    if (static_cast<int>(switch_times_ms_.size()) >= config_.max_switches_per_window) {
        cooldown_until_ms_ = now_ms + config_.switch_cooldown_ms;
        return false;
    }
    return true;
}

void NetworkPolicy::ResetRecoveryEvidence() {
    wifi_recovery_successes_ = 0;
    last_wifi_recovery_success_at_ms_ = 0;
}
