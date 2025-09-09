#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <string>

class WifiManager {
public:
    static WifiManager& GetInstance();
    
    // 初始化WiFi系统
    bool Initialize();
    
    // WiFi配置方法
    void SetDefaultCredentials();
    void ConfigureWifiSettings();
    
    // 检查是否已初始化
    bool IsInitialized() const { return initialized_; }

private:
    WifiManager() = default;
    ~WifiManager() = default;
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;
    
    bool initialized_ = false;
};

#endif // WIFI_MANAGER_H
