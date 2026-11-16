#ifndef ALIYUN_REALTIME_CONFIG_H
#define ALIYUN_REALTIME_CONFIG_H

#include <string>

struct AliyunRealtimeConfig {
    bool selected = false;
    std::string region = "cn-beijing";
    std::string workspace_id;
    std::string api_key;
    // Empty keeps the generated Aliyun URL; ws:// selects the Debug Proxy.
    std::string websocket_url;
    bool debug_enabled = false;
    std::string debug_address;
    int debug_level = 0;
    bool debug_input_pcm = false;

    bool IsReady() const;
    std::string WebSocketUrl() const;
};

AliyunRealtimeConfig LoadAliyunRealtimeConfig();
bool SaveAliyunRealtimeConfig(const AliyunRealtimeConfig& config);
bool ValidateAliyunRealtimeConfig(const AliyunRealtimeConfig& config);

#endif
