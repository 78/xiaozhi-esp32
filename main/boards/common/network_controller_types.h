#ifndef NETWORK_CONTROLLER_TYPES_H
#define NETWORK_CONTROLLER_TYPES_H

#include <cstdint>
#include <string>

enum class NetworkMode {
    Auto,
    Wifi,
    Cellular,
};

enum class NetworkTransport {
    None,
    Wifi,
    Cellular,
};

enum class NetworkHealth {
    Down,
    Starting,
    LinkUp,
    InternetReady,
    Degraded,
};

enum class NetworkSwitchReason {
    None,
    Startup,
    ManualOverride,
    WifiFailed,
    WifiRecovered,
    CellularFailed,
    PowerPolicy,
    BothNetworksDown,
    SwitchRateLimited,
};

struct NetworkStatusSnapshot {
    NetworkMode mode = NetworkMode::Auto;
    NetworkTransport active = NetworkTransport::None;
    NetworkTransport candidate = NetworkTransport::None;
    NetworkHealth wifi_health = NetworkHealth::Down;
    NetworkHealth cellular_health = NetworkHealth::Down;
    NetworkSwitchReason last_switch_reason = NetworkSwitchReason::None;
    uint32_t generation = 0;
    bool offline = true;
    bool switch_rate_limited = false;
};

const char* ToString(NetworkMode mode);
const char* ToString(NetworkTransport transport);
const char* ToString(NetworkHealth health);
const char* ToString(NetworkSwitchReason reason);

bool ParseNetworkMode(const std::string& value, NetworkMode& mode);

#endif  // NETWORK_CONTROLLER_TYPES_H
