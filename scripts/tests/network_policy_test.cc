#include <cassert>
#include <cstdint>

#include "network_policy.h"

namespace {

void ConfirmFailure(NetworkPolicy& policy, NetworkTransport transport, uint64_t start_ms) {
    policy.ReportHealth(transport, NetworkHealth::Degraded, start_ms);
    policy.ReportHealth(transport, NetworkHealth::Degraded, start_ms + 8'000);
    policy.ReportHealth(transport, NetworkHealth::Degraded, start_ms + 15'000);
}

void TestWifiFailureUsesReadyCellular() {
    NetworkPolicy policy;
    policy.SetMode(NetworkMode::Auto, 0);
    policy.SetActive(NetworkTransport::Wifi, 0);
    policy.ReportHealth(NetworkTransport::Wifi, NetworkHealth::InternetReady, 0);
    policy.ReportHealth(NetworkTransport::Cellular, NetworkHealth::InternetReady, 0);
    ConfirmFailure(policy, NetworkTransport::Wifi, 1'000);

    auto decision = policy.Evaluate(16'000);
    assert(decision.switch_requested);
    assert(decision.target == NetworkTransport::Cellular);
    assert(decision.reason == NetworkSwitchReason::WifiFailed);
}

void TestAutoStartupWaitsForWifiWindowBeforeUsingCellular() {
    NetworkPolicy policy;
    policy.SetMode(NetworkMode::Auto, 0);
    policy.ReportHealth(NetworkTransport::Cellular, NetworkHealth::InternetReady, 0);

    assert(!policy.Evaluate(19'999).switch_requested);
    const auto decision = policy.Evaluate(20'000);
    assert(decision.switch_requested);
    assert(decision.target == NetworkTransport::Cellular);
    assert(decision.reason == NetworkSwitchReason::Startup);
}

void TestActiveFailureRequiresCountAndDuration() {
    NetworkPolicy too_few;
    too_few.SetMode(NetworkMode::Auto, 0);
    too_few.SetActive(NetworkTransport::Wifi, 0);
    too_few.ReportHealth(NetworkTransport::Cellular, NetworkHealth::InternetReady, 0);
    too_few.ReportHealth(NetworkTransport::Wifi, NetworkHealth::Down, 1'000);
    too_few.ReportHealth(NetworkTransport::Wifi, NetworkHealth::Down, 16'000);
    assert(!too_few.Evaluate(16'000).switch_requested);

    NetworkPolicy too_soon;
    too_soon.SetMode(NetworkMode::Auto, 0);
    too_soon.SetActive(NetworkTransport::Wifi, 0);
    too_soon.ReportHealth(NetworkTransport::Cellular, NetworkHealth::InternetReady, 0);
    too_soon.ReportHealth(NetworkTransport::Wifi, NetworkHealth::Down, 1'000);
    too_soon.ReportHealth(NetworkTransport::Wifi, NetworkHealth::Down, 8'000);
    too_soon.ReportHealth(NetworkTransport::Wifi, NetworkHealth::Down, 15'999);
    assert(!too_soon.Evaluate(15'999).switch_requested);

    too_soon.ReportHealth(NetworkTransport::Wifi, NetworkHealth::Down, 16'000);
    assert(too_soon.Evaluate(16'000).switch_requested);
}

void TestNoSimAndRegistrationDeniedDoNotDropWifi() {
    for (auto cellular_health : {NetworkHealth::Down, NetworkHealth::Degraded}) {
        NetworkPolicy policy;
        policy.SetMode(NetworkMode::Auto, 0);
        policy.SetActive(NetworkTransport::Wifi, 0);
        policy.ReportHealth(NetworkTransport::Cellular, cellular_health, 0);
        ConfirmFailure(policy, NetworkTransport::Wifi, 1'000);
        assert(!policy.Evaluate(16'000).switch_requested);
    }
}

void TestCellularFailuresOpenRetryBreakerUntilPositiveProbe() {
    NetworkPolicy policy;
    policy.SetMode(NetworkMode::Auto, 0);
    policy.RecordCellularStartFailure(0);
    assert(!policy.CanRetryCellular(29'999));
    assert(policy.CanRetryCellular(30'000));
    policy.RecordCellularStartFailure(30'000);
    policy.RecordCellularStartFailure(60'000);
    auto limited = policy.GetSnapshot();
    assert(limited.cellular_start_failures == 3);
    assert(limited.cellular_retry_limited);
    assert(!policy.CanRetryCellular(359'999));
    assert(policy.CanRetryCellular(360'000));

    // Link-up is not enough to close the breaker. The candidate must prove internet access.
    policy.ReportHealth(NetworkTransport::Cellular, NetworkHealth::LinkUp, 360'000);
    assert(policy.GetSnapshot().cellular_retry_limited);
    policy.ReportHealth(NetworkTransport::Cellular, NetworkHealth::InternetReady, 361'000);
    assert(policy.GetSnapshot().cellular_start_failures == 0);
    assert(!policy.GetSnapshot().cellular_retry_limited);
}

void TestNoSimImmediatelyPausesCellularUntilLimitedProbe() {
    NetworkPolicy policy;
    policy.SetMode(NetworkMode::Auto, 0);
    policy.RecordCellularNoSim(1'000);

    const auto missing = policy.GetSnapshot();
    assert(missing.cellular_start_failures == 1);
    assert(missing.cellular_retry_limited);
    assert(missing.cellular_sim_missing);
    assert(!policy.CanRetryCellular(300'999));
    assert(policy.CanRetryCellular(301'000));

    policy.ReportHealth(NetworkTransport::Cellular, NetworkHealth::InternetReady, 301'000);
    const auto recovered = policy.GetSnapshot();
    assert(recovered.cellular_start_failures == 0);
    assert(!recovered.cellular_retry_limited);
    assert(!recovered.cellular_sim_missing);
}

void TestManualCellularModeClearsRetryBreaker() {
    NetworkPolicy policy;
    policy.RecordCellularStartFailure(0);
    policy.RecordCellularStartFailure(30'000);
    policy.RecordCellularStartFailure(60'000);
    policy.SetMode(NetworkMode::Cellular, 61'000);
    assert(policy.CanRetryCellular(61'000));
    assert(policy.GetSnapshot().cellular_start_failures == 0);
}

void TestGlobalServerFailureDoesNotFlap() {
    NetworkPolicy policy;
    policy.SetMode(NetworkMode::Auto, 0);
    policy.SetActive(NetworkTransport::Wifi, 0);
    ConfirmFailure(policy, NetworkTransport::Wifi, 0);
    ConfirmFailure(policy, NetworkTransport::Cellular, 0);
    assert(!policy.Evaluate(15'000).switch_requested);
    const auto snapshot = policy.GetSnapshot();
    assert(snapshot.offline);
    assert(snapshot.active == NetworkTransport::Wifi);
    assert(snapshot.candidate == NetworkTransport::None);
}

void TestWifiRecoveryRequiresDwellAndTwoSpacedSuccesses() {
    NetworkPolicy policy;
    policy.SetMode(NetworkMode::Auto, 0);
    policy.SetActive(NetworkTransport::Cellular, 0);
    policy.ReportHealth(NetworkTransport::Cellular, NetworkHealth::InternetReady, 0);
    policy.ReportHealth(NetworkTransport::Wifi, NetworkHealth::InternetReady, 60'000);
    policy.ReportHealth(NetworkTransport::Wifi, NetworkHealth::InternetReady, 89'000);
    assert(!policy.Evaluate(120'000).switch_requested);
    policy.ReportHealth(NetworkTransport::Wifi, NetworkHealth::InternetReady, 90'000);
    auto decision = policy.Evaluate(120'000);
    assert(decision.switch_requested);
    assert(decision.target == NetworkTransport::Wifi);
    assert(decision.reason == NetworkSwitchReason::WifiRecovered);
}

void TestSwitchRateLimitAndCooldown() {
    NetworkPolicy policy;
    policy.SetMode(NetworkMode::Auto, 0);
    policy.RecordSwitch(NetworkTransport::Wifi, NetworkSwitchReason::Startup, 0);
    policy.RecordSwitch(NetworkTransport::Cellular, NetworkSwitchReason::WifiFailed, 60'000);
    policy.ReportHealth(NetworkTransport::Wifi, NetworkHealth::InternetReady, 120'000);
    ConfirmFailure(policy, NetworkTransport::Cellular, 100'000);
    assert(!policy.Evaluate(120'000).switch_requested);
    assert(policy.GetSnapshot().switch_rate_limited);
    assert(!policy.Evaluate(419'999).switch_requested);
    assert(policy.Evaluate(420'000).switch_requested);
}

void TestLateCallbackAndPowerPolicy() {
    assert(NetworkPolicy::IsCurrentGeneration(3, 3));
    assert(!NetworkPolicy::IsCurrentGeneration(2, 3));
    assert(NetworkPolicy::ShouldPowerCellular(true, NetworkMode::Auto,
                                              NetworkTransport::Wifi));
    assert(!NetworkPolicy::ShouldPowerCellular(false, NetworkMode::Auto,
                                               NetworkTransport::Wifi));
    assert(NetworkPolicy::ShouldPowerCellular(false, NetworkMode::Cellular,
                                              NetworkTransport::Wifi));
    assert(NetworkPolicy::ShouldPowerCellular(false, NetworkMode::Auto,
                                              NetworkTransport::Cellular));
}

void TestModeParsing() {
    NetworkMode mode = NetworkMode::Auto;
    assert(ParseNetworkMode("wifi", mode) && mode == NetworkMode::Wifi);
    assert(ParseNetworkMode("cellular", mode) && mode == NetworkMode::Cellular);
    assert(ParseNetworkMode("4g", mode) && mode == NetworkMode::Cellular);
    assert(ParseNetworkMode("auto", mode) && mode == NetworkMode::Auto);
    assert(!ParseNetworkMode("invalid", mode));
}

}  // namespace

int main() {
    TestWifiFailureUsesReadyCellular();
    TestAutoStartupWaitsForWifiWindowBeforeUsingCellular();
    TestActiveFailureRequiresCountAndDuration();
    TestNoSimAndRegistrationDeniedDoNotDropWifi();
    TestCellularFailuresOpenRetryBreakerUntilPositiveProbe();
    TestNoSimImmediatelyPausesCellularUntilLimitedProbe();
    TestManualCellularModeClearsRetryBreaker();
    TestGlobalServerFailureDoesNotFlap();
    TestWifiRecoveryRequiresDwellAndTwoSpacedSuccesses();
    TestSwitchRateLimitAndCooldown();
    TestLateCallbackAndPowerPolicy();
    TestModeParsing();
    return 0;
}
