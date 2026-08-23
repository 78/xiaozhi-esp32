#ifndef DUAL_NETWORK_BOARD_H
#define DUAL_NETWORK_BOARD_H

#include <memory>

#include "board.h"
#include "ml307_board.h"
#include "network_controller.h"
#include "wifi_board.h"

// Retained for source compatibility with existing dual-network boards.
enum class NetworkType {
    WIFI,
    ML307,
};

class DualNetworkBoard : public Board {
private:
    std::unique_ptr<WifiBoard> wifi_board_;
    std::unique_ptr<Ml307Board> cellular_board_;
    std::unique_ptr<NetworkController> network_controller_;
    void InitializeNetworkTools();

public:
    DualNetworkBoard(gpio_num_t ml307_tx_pin, gpio_num_t ml307_rx_pin,
                     gpio_num_t ml307_dtr_pin = GPIO_NUM_NC, int32_t default_net_type = 1);
    ~DualNetworkBoard() override = default;

    // Legacy convenience method now performs a live, non-rebooting mode change.
    void SwitchNetworkType();
    NetworkType GetNetworkType() const;
    Board& GetCurrentBoard() const;

    bool SetNetworkMode(NetworkMode mode);
    void CommitNetworkSwitch(NetworkTransport target, NetworkSwitchReason reason);
    void SetCellularPowerControl(std::function<bool(bool enabled)> callback);
    void SetExternalPowerProvider(std::function<bool()> callback);
    void RefreshNetworkPowerPolicy();
    void EnterMaintenanceMode();

    std::string GetBoardType() override;
    void StartNetwork() override;
    bool StopNetwork() override;
    void SetNetworkEventCallback(NetworkEventCallback callback) override;
    NetworkInterface* GetNetwork() override;
    NetworkController* GetNetworkController() override { return network_controller_.get(); }
    const char* GetNetworkStateIcon() override;
    void SetPowerSaveLevel(PowerSaveLevel level) override;
    std::string GetBoardJson() override;
    std::string GetDeviceStatusJson() override;
};

#endif  // DUAL_NETWORK_BOARD_H
