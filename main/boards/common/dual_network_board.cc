#include "dual_network_board.h"
#include "application.h"
#include "display.h"
#include "assets/lang_config.h"
#include "settings.h"
#include <esp_log.h>

static const char *TAG = "DualNetworkBoard";

DualNetworkBoard::DualNetworkBoard(gpio_num_t tx_pin, gpio_num_t rx_pin,
                                   gpio_num_t aux_pin, int32_t default_net_type,
                                   CellularModuleType module_type) 
    : Board(), 
      cellular_tx_pin_(tx_pin), 
      cellular_rx_pin_(rx_pin),
      cellular_aux_pin_(aux_pin),
      cellular_module_type_(module_type) {

    // 从Settings加载网络类型
    network_type_ = LoadNetworkTypeFromSettings(default_net_type);

    // 只初始化当前网络类型对应的板卡
    InitializeCurrentBoard();
}


NetworkType DualNetworkBoard::LoadNetworkTypeFromSettings(int32_t default_net_type) {
    Settings settings("network", true);
    int net_type = settings.GetInt("type", default_net_type);
    return net_type == 1 ? NetworkType::CELLULAR : NetworkType::WIFI;
}

void DualNetworkBoard::SaveNetworkTypeToSettings(NetworkType type) {
    Settings settings("network", true);
    int network_type = (type == NetworkType::CELLULAR) ? 1 : 0;
    settings.SetInt("type", network_type);
}

void DualNetworkBoard::InitializeCurrentBoard() {
    if (cellular_module_type_ == CellularModuleType::A7670E) {
        ESP_LOGI(TAG, "Initialize A7670E board");
        current_board_ = std::make_unique<A7670EBoard>(cellular_tx_pin_, cellular_rx_pin_, cellular_aux_pin_);
    } else if (cellular_module_type_ == CellularModuleType::ML307) {
        ESP_LOGI(TAG, "Initialize ML307 board");
        current_board_ = std::make_unique<Ml307Board>(cellular_tx_pin_, cellular_rx_pin_, cellular_aux_pin_);
    } else {
        ESP_LOGI(TAG, "Initialize WiFi board");
        current_board_ = std::make_unique<WifiBoard>();
    }
}

void DualNetworkBoard::SwitchNetworkType() {
    auto display = GetDisplay();
    if (type == NetworkType::CELLULAR) {
        SaveNetworkTypeToSettings(NetworkType::CELLULAR);
        display->ShowNotification(Lang::Strings::SWITCH_TO_4G_NETWORK);
    } else {
        SaveNetworkTypeToSettings(NetworkType::WIFI);
        display->ShowNotification(Lang::Strings::SWITCH_TO_WIFI_NETWORK);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    auto& app = Application::GetInstance();
    app.Reboot();
}

 
std::string DualNetworkBoard::GetBoardType() {
    return current_board_->GetBoardType();
}

void DualNetworkBoard::StartNetwork() {
    auto display = Board::GetInstance().GetDisplay();
    
    if (network_type_ == NetworkType::WIFI) {
        display->SetStatus(Lang::Strings::CONNECTING);
    } else {
        display->SetStatus(Lang::Strings::DETECTING_MODULE);
    }
    current_board_->StartNetwork();
}

void DualNetworkBoard::SetNetworkEventCallback(NetworkEventCallback callback) {
    // Forward the callback to the current board
    current_board_->SetNetworkEventCallback(std::move(callback));
}

NetworkInterface* DualNetworkBoard::GetNetwork() {
    return current_board_->GetNetwork();
}

const char* DualNetworkBoard::GetNetworkStateIcon() {
    return current_board_->GetNetworkStateIcon();
}

void DualNetworkBoard::SetPowerSaveLevel(PowerSaveLevel level) {
    current_board_->SetPowerSaveLevel(level);
}

std::string DualNetworkBoard::GetBoardJson() {   
    return current_board_->GetBoardJson();
}

std::string DualNetworkBoard::GetDeviceStatusJson() {
    return current_board_->GetDeviceStatusJson();
}
