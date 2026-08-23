#ifndef NETWORK_CONTROLLER_H
#define NETWORK_CONTROLLER_H

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "board.h"
#include "ml307_board.h"
#include "network_policy.h"
#include "wifi_board.h"

class NetworkController {
public:
    using SwitchRequestCallback =
        std::function<void(NetworkTransport target, NetworkSwitchReason reason)>;

    NetworkController(WifiBoard& wifi, Ml307Board& cellular);
    ~NetworkController();

    void Start();
    void Stop();
    bool SetMode(NetworkMode mode);
    NetworkMode GetMode() const;
    NetworkStatusSnapshot GetStatus() const;
    NetworkInterface* GetNetwork() const;
    const char* GetNetworkStateIcon() const;
    void SetPowerSaveLevel(PowerSaveLevel level);

    void SetNetworkEventCallback(NetworkEventCallback callback);
    void SetSwitchRequestCallback(SwitchRequestCallback callback);
    void SetCellularPowerControl(std::function<bool(bool enabled)> callback);
    void SetExternalPowerProvider(std::function<bool()> callback);
    void RefreshPowerPolicy();

    void CommitSwitch(NetworkTransport target, NetworkSwitchReason reason);
    void CancelPendingSwitch();
    void ReportProtocolConnected();
    void ReportProtocolFailure();

private:
    struct ProbeContext {
        NetworkController* controller;
        NetworkTransport transport;
        uint32_t generation;
    };

    WifiBoard& wifi_;
    Ml307Board& cellular_;
    mutable std::mutex mutex_;
    NetworkPolicy policy_;
    NetworkEventCallback network_event_callback_;
    SwitchRequestCallback switch_request_callback_;
    std::function<bool(bool)> cellular_power_control_;
    std::function<bool()> external_power_provider_;
    std::string health_check_url_;
    std::atomic<bool> running_{false};
    EventGroupHandle_t lifecycle_events_ = nullptr;
    TaskHandle_t worker_task_ = nullptr;
    uint32_t wifi_generation_ = 0;
    uint32_t cellular_generation_ = 0;
    bool wifi_started_ = false;
    bool cellular_started_ = false;
    bool cellular_powered_ = false;
    bool wifi_probe_in_progress_ = false;
    bool cellular_probe_in_progress_ = false;
    bool switch_request_pending_ = false;

    static void WorkerTaskEntry(void* arg);
    void WorkerTask();
    static void ProbeTaskEntry(void* arg);
    void Probe(NetworkTransport transport, uint32_t generation);

    void StartWifi();
    void StartCellular();
    void StopCellularAndPowerOff();
    void OnTransportEvent(NetworkTransport transport, uint32_t generation, NetworkEvent event,
                          const std::string& data);
    void ReportHealth(NetworkTransport transport, NetworkHealth health);
    void EvaluatePolicy();
    void ScheduleProbe(NetworkTransport transport);
    void ApplyPowerPolicy();
    void NotifyNetworkEvent(NetworkEvent event, const std::string& data = "");
    void LoadAndMigrateSettings();
    void SaveMode(NetworkMode mode);
};

#endif  // NETWORK_CONTROLLER_H
