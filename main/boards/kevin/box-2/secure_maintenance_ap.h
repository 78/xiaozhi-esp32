#ifndef SECURE_MAINTENANCE_AP_H
#define SECURE_MAINTENANCE_AP_H

#include <mutex>
#include <string>

#include <esp_netif.h>

class LocalControlPanel;

class SecureMaintenanceAccessPoint {
public:
    explicit SecureMaintenanceAccessPoint(LocalControlPanel& panel);
    ~SecureMaintenanceAccessPoint();

    bool Start();
    void Stop();
    bool IsRunning() const;
    std::string GetSsid() const;
    std::string GetPassword() const;

private:
    LocalControlPanel& panel_;
    mutable std::mutex mutex_;
    esp_netif_t* ap_netif_ = nullptr;
    std::string ssid_;
    std::string password_;
    bool running_ = false;

    static std::string GenerateSsid();
    static std::string GeneratePassword();
};

#endif  // SECURE_MAINTENANCE_AP_H
