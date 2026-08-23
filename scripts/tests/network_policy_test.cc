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

void TestGlobalServerFailureDoesNotFlap() {
    NetworkPolicy policy;
    policy.SetMode(NetworkMode::Auto, 0);
    policy.SetActive(NetworkTransport::Wifi, 0);
    ConfirmFailure(policy, NetworkTransport::Wifi, 0);
    ConfirmFailure(policy, NetworkTransport::Cellular, 0);
    assert(!policy.Evaluate(15'000).switch_requested);
    assert(policy.GetSnapshot().offline);
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
    TestNoSimAndRegistrationDeniedDoNotDropWifi();
    TestGlobalServerFailureDoesNotFlap();
    TestWifiRecoveryRequiresDwellAndTwoSpacedSuccesses();
    TestSwitchRateLimitAndCooldown();
    TestLateCallbackAndPowerPolicy();
    TestModeParsing();
    return 0;
}
