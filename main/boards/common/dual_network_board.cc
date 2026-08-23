#include "dual_network_board.h"

#include <esp_log.h>

#include "mcp_server.h"

namespace {
const char* TAG = "DualNetworkBoard";
}

DualNetworkBoard::DualNetworkBoard(gpio_num_t ml307_tx_pin, gpio_num_t ml307_rx_pin,
                                   gpio_num_t ml307_dtr_pin, int32_t default_net_type)
    : Board(),
      wifi_board_(std::make_unique<WifiBoard>()),
      cellular_board_(
          std::make_unique<Ml307Board>(ml307_tx_pin, ml307_rx_pin, ml307_dtr_pin)),
      network_controller_(
          std::make_unique<NetworkController>(*wifi_board_, *cellular_board_)) {
    // New devices use AUTO. The controller migrates an existing network/type value and therefore
    // preserves the user's fixed selection. This parameter remains for source compatibility.
    (void)default_net_type;
    InitializeNetworkTools();
}

void DualNetworkBoard::InitializeNetworkTools() {
    auto& mcp_server = McpServer::GetInstance();
    mcp_server.AddTool(
        "self.network.get_status",
        "Get the selected network mode, active and candidate transports, health, offline state, "
        "and the most recent switch reason.",
        PropertyList(), [this](const PropertyList&) -> ReturnValue {
            const auto status = network_controller_->GetStatus();
            auto* root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "mode", ToString(status.mode));
            cJSON_AddStringToObject(root, "active", ToString(status.active));
            cJSON_AddStringToObject(root, "candidate", ToString(status.candidate));
            cJSON_AddStringToObject(root, "wifi_health", ToString(status.wifi_health));
            cJSON_AddStringToObject(root, "cellular_health", ToString(status.cellular_health));
            cJSON_AddStringToObject(root, "last_switch_reason",
                                    ToString(status.last_switch_reason));
            cJSON_AddBoolToObject(root, "offline", status.offline);
            cJSON_AddNumberToObject(root, "generation", status.generation);
            cJSON_AddNumberToObject(root, "cellular_start_failures",
                                    status.cellular_start_failures);
            cJSON_AddBoolToObject(root, "cellular_retry_limited",
                                  status.cellular_retry_limited);
            return root;
        });
    mcp_server.AddTool(
        "self.network.set_mode",
        "Set network mode to auto, wifi, or cellular. The current conversation may be ended while "
        "the device switches transports without rebooting.",
        PropertyList({Property("mode", kPropertyTypeString)}),
        [this](const PropertyList& properties) -> ReturnValue {
            NetworkMode mode;
            if (!ParseNetworkMode(properties["mode"].value<std::string>(), mode)) {
                return false;
            }
            return SetNetworkMode(mode);
        });
}

void DualNetworkBoard::SwitchNetworkType() {
    const auto active = network_controller_->GetStatus().active;
    SetNetworkMode(active == NetworkTransport::Cellular ? NetworkMode::Wifi
                                                        : NetworkMode::Cellular);
}

NetworkType DualNetworkBoard::GetNetworkType() const {
    const auto status = network_controller_->GetStatus();
    if (status.mode == NetworkMode::Cellular || status.active == NetworkTransport::Cellular) {
        return NetworkType::ML307;
    }
    return NetworkType::WIFI;
}

Board& DualNetworkBoard::GetCurrentBoard() const {
    return network_controller_->GetStatus().active == NetworkTransport::Cellular
               ? static_cast<Board&>(*cellular_board_)
               : static_cast<Board&>(*wifi_board_);
}

bool DualNetworkBoard::SetNetworkMode(NetworkMode mode) {
    return network_controller_->SetMode(mode);
}

void DualNetworkBoard::CommitNetworkSwitch(NetworkTransport target,
                                           NetworkSwitchReason reason) {
    network_controller_->CommitSwitch(target, reason);
}

void DualNetworkBoard::SetCellularPowerControl(std::function<bool(bool)> callback) {
    network_controller_->SetCellularPowerControl(std::move(callback));
}

void DualNetworkBoard::SetExternalPowerProvider(std::function<bool()> callback) {
    network_controller_->SetExternalPowerProvider(std::move(callback));
}

void DualNetworkBoard::SetWifiConfigModeHandler(std::function<void()> callback) {
    wifi_board_->SetConfigModeHandler(std::move(callback));
}

void DualNetworkBoard::RefreshNetworkPowerPolicy() {
    network_controller_->RefreshPowerPolicy();
}

void DualNetworkBoard::EnterMaintenanceMode() {
    wifi_board_->EnterWifiConfigMode();
}

int DualNetworkBoard::GetWifiRssi() const {
    return wifi_board_->GetSignalStrength();
}

int DualNetworkBoard::GetCellularSignalQuality() const {
    return cellular_board_->GetSignalQuality();
}

std::string DualNetworkBoard::GetBoardType() {
    return GetCurrentBoard().GetBoardType();
}

void DualNetworkBoard::StartNetwork() {
    ESP_LOGI(TAG, "Starting dual-network controller in %s mode",
             ToString(network_controller_->GetMode()));
    network_controller_->Start();
}

bool DualNetworkBoard::StopNetwork() {
    network_controller_->Stop();
    return true;
}

void DualNetworkBoard::SetNetworkEventCallback(NetworkEventCallback callback) {
    network_controller_->SetNetworkEventCallback(std::move(callback));
}

NetworkInterface* DualNetworkBoard::GetNetwork() {
    return network_controller_->GetNetwork();
}

const char* DualNetworkBoard::GetNetworkStateIcon() {
    return network_controller_->GetNetworkStateIcon();
}

void DualNetworkBoard::SetPowerSaveLevel(PowerSaveLevel level) {
    network_controller_->SetPowerSaveLevel(level);
}

std::string DualNetworkBoard::GetBoardJson() {
    return GetCurrentBoard().GetBoardJson();
}

std::string DualNetworkBoard::GetDeviceStatusJson() {
    return GetCurrentBoard().GetDeviceStatusJson();
}
