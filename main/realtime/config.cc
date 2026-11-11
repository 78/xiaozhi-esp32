#include "config.h"

#include <arpa/inet.h>
#include <lwip/sockets.h>
#include <nvs.h>

#include <algorithm>
#include <cctype>

namespace {
constexpr char kNamespace[] = "aliyun_rt";

std::string ReadString(nvs_handle_t handle, const char* key) {
    size_t length = 0;
    if (nvs_get_str(handle, key, nullptr, &length) != ESP_OK || length == 0) {
        return {};
    }
    std::string value(length, '\0');
    if (nvs_get_str(handle, key, value.data(), &length) != ESP_OK) {
        return {};
    }
    value.resize(length - 1);
    return value;
}

bool ValidWorkspace(const std::string& value) {
    if (value.empty() || value.size() > 64) {
        return false;
    }
    return std::all_of(value.begin(), value.end(),
                       [](unsigned char ch) { return std::isalnum(ch) || ch == '-'; });
}

bool ValidDebugAddress(const std::string& value) {
    if (value.empty()) {
        return true;
    }
    constexpr char prefix[] = "tcp://";
    if (value.rfind(prefix, 0) != 0 || value.size() > 64) {
        return false;
    }
    auto host_port = value.substr(sizeof(prefix) - 1);
    auto colon = host_port.rfind(':');
    if (colon == std::string::npos || colon == 0) {
        return false;
    }
    in_addr address{};
    if (inet_pton(AF_INET, host_port.substr(0, colon).c_str(), &address) != 1) {
        return false;
    }
    const auto port = host_port.substr(colon + 1);
    if (port.empty() || port.size() > 5 ||
        !std::all_of(port.begin(), port.end(),
                     [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return false;
    }
    const int number = std::stoi(port);
    return number > 0 && number <= 65535;
}
}  // namespace

bool AliyunRealtimeConfig::IsReady() const {
    return ValidWorkspace(workspace_id) && !api_key.empty() &&
           (region == "cn-beijing" || region == "ap-southeast-1");
}

std::string AliyunRealtimeConfig::WebSocketUrl() const {
    const char* domain = region == "ap-southeast-1" ? "ap-southeast-1" : "cn-beijing";
    return "wss://" + workspace_id + "." + domain +
           ".maas.aliyuncs.com/api-ws/v1/realtime?model=qwen3.8-omni-flash-realtime";
}

AliyunRealtimeConfig LoadAliyunRealtimeConfig() {
    AliyunRealtimeConfig config;
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return config;
    }
    uint8_t enabled = 0;
    int32_t level = 0;
    nvs_get_u8(handle, "selected", &enabled);
    config.selected = enabled != 0;
    const auto region = ReadString(handle, "region");
    if (!region.empty()) {
        config.region = region;
    }
    config.workspace_id = ReadString(handle, "workspace");
    config.api_key = ReadString(handle, "api_key");
    nvs_get_u8(handle, "dbg_enable", &enabled);
    config.debug_enabled = enabled != 0;
    config.debug_address = ReadString(handle, "dbg_addr");
    nvs_get_i32(handle, "dbg_level", &level);
    config.debug_level = level;
    nvs_close(handle);
    return config;
}

bool ValidateAliyunRealtimeConfig(const AliyunRealtimeConfig& config) {
    if (config.region != "cn-beijing" && config.region != "ap-southeast-1") {
        return false;
    }
    if (!config.workspace_id.empty() && !ValidWorkspace(config.workspace_id)) {
        return false;
    }
    if (config.api_key.size() > 256 ||
        !std::all_of(config.api_key.begin(), config.api_key.end(),
                     [](unsigned char ch) { return ch >= 33 && ch <= 126; })) {
        return false;
    }
    if (config.selected && !config.IsReady()) {
        return false;
    }
    return config.debug_level >= 0 && config.debug_level <= 2 &&
           ValidDebugAddress(config.debug_address) &&
           (!config.debug_enabled || !config.debug_address.empty());
}

bool SaveAliyunRealtimeConfig(const AliyunRealtimeConfig& config) {
    if (!ValidateAliyunRealtimeConfig(config)) {
        return false;
    }
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    esp_err_t result = nvs_set_u8(handle, "selected", config.selected ? 1 : 0);
    if (result == ESP_OK)
        result = nvs_set_str(handle, "region", config.region.c_str());
    if (result == ESP_OK)
        result = nvs_set_str(handle, "workspace", config.workspace_id.c_str());
    if (result == ESP_OK)
        result = nvs_set_str(handle, "api_key", config.api_key.c_str());
    if (result == ESP_OK)
        result = nvs_set_u8(handle, "dbg_enable", config.debug_enabled ? 1 : 0);
    if (result == ESP_OK)
        result = nvs_set_str(handle, "dbg_addr", config.debug_address.c_str());
    if (result == ESP_OK)
        result = nvs_set_i32(handle, "dbg_level", config.debug_level);
    if (result == ESP_OK)
        result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}
