#ifndef NETWORK_POLICY_H
#define NETWORK_POLICY_H

#include <cstdint>
#include <deque>

#include "network_controller_types.h"

struct NetworkPolicyConfig {
    uint64_t wifi_start_window_ms = 20'000;
    uint64_t active_failure_window_ms = 15'000;
    int active_failure_count = 3;
    uint64_t cellular_min_dwell_ms = 120'000;
    uint64_t wifi_probe_interval_ms = 60'000;
    uint64_t wifi_recovery_success_gap_ms = 30'000;
    int wifi_recovery_success_count = 2;
    uint64_t switch_window_ms = 600'000;
    int max_switches_per_window = 2;
    uint64_t switch_cooldown_ms = 300'000;
};

struct NetworkDecision {
    bool switch_requested = false;
    NetworkTransport target = NetworkTransport::None;
    NetworkSwitchReason reason = NetworkSwitchReason::None;
};

class NetworkPolicy {
public:
    explicit NetworkPolicy(NetworkPolicyConfig config = {});

    void SetMode(NetworkMode mode, uint64_t now_ms);
    void SetActive(NetworkTransport transport, uint64_t now_ms);
    void ReportHealth(NetworkTransport transport, NetworkHealth health, uint64_t now_ms);
    NetworkDecision Evaluate(uint64_t now_ms);
    void RecordSwitch(NetworkTransport transport, NetworkSwitchReason reason, uint64_t now_ms);

    NetworkStatusSnapshot GetSnapshot() const;
    uint64_t NextWifiProbeAtMs() const { return next_wifi_probe_at_ms_; }
    void MarkWifiProbeStarted(uint64_t now_ms);

    static bool IsCurrentGeneration(uint32_t callback_generation, uint32_t current_generation) {
        return callback_generation == current_generation;
    }

    static bool ShouldPowerCellular(bool external_power, NetworkMode mode,
                                    NetworkTransport active);

private:
    struct HealthState {
        NetworkHealth value = NetworkHealth::Down;
        int consecutive_failures = 0;
        uint64_t first_failure_at_ms = 0;
    };

    NetworkPolicyConfig config_;
    NetworkMode mode_ = NetworkMode::Auto;
    NetworkTransport active_ = NetworkTransport::None;
    NetworkTransport candidate_ = NetworkTransport::None;
    NetworkSwitchReason last_switch_reason_ = NetworkSwitchReason::None;
    HealthState wifi_;
    HealthState cellular_;
    uint64_t active_since_ms_ = 0;
    uint64_t wifi_start_deadline_ms_ = 0;
    uint64_t next_wifi_probe_at_ms_ = 0;
    int wifi_recovery_successes_ = 0;
    uint64_t last_wifi_recovery_success_at_ms_ = 0;
    std::deque<uint64_t> switch_times_ms_;
    uint64_t cooldown_until_ms_ = 0;
    uint32_t generation_ = 0;

    HealthState& StateFor(NetworkTransport transport);
    const HealthState& StateFor(NetworkTransport transport) const;
    static bool IsHealthy(NetworkHealth health);
    static bool IsFailure(NetworkHealth health);
    bool FailureConfirmed(const HealthState& state, uint64_t now_ms) const;
    bool SwitchAllowed(uint64_t now_ms);
    void ResetRecoveryEvidence();
};

#endif  // NETWORK_POLICY_H
