#ifndef ALIYUN_REALTIME_CONFIG_H
#define ALIYUN_REALTIME_CONFIG_H

#include <string>

struct AliyunRealtimeConfig {
    bool selected = false;
    std::string region = "cn-beijing";
    std::string workspace_id;
    std::string api_key;
    bool debug_enabled = false;
    std::string debug_address;
    int debug_level = 0;

    bool IsReady() const;
    std::string WebSocketUrl() const;
};

AliyunRealtimeConfig LoadAliyunRealtimeConfig();
bool SaveAliyunRealtimeConfig(const AliyunRealtimeConfig& config);
bool ValidateAliyunRealtimeConfig(const AliyunRealtimeConfig& config);

#endif
