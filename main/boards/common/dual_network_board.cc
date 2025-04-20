#include "dual_network_board.h"
#include "application.h"
#include "display.h"
#include "assets/lang_config.h"
#include "settings.h"
#include <esp_log.h>

static const char *TAG = "DualNetworkBoard";

DualNetworkBoard::DualNetworkBoard(gpio_num_t ml307_tx_pin, gpio_num_t ml307_rx_pin, size_t ml307_rx_buffer_size) 
    : Board(), 
      ml307_tx_pin_(ml307_tx_pin), 
      ml307_rx_pin_(ml307_rx_pin), 
      ml307_rx_buffer_size_(ml307_rx_buffer_size) {
    
    // 从Settings加载网络类型
    network_type_ = LoadNetworkTypeFromSettings();
    
    // 初始化当前网络类型对应的板卡
    InitializeCurrentBoard();
}

NetworkType DualNetworkBoard::LoadNetworkTypeFromSettings() {
    Settings settings("network", true);
    int network_type = settings.GetInt("type", 0); // 默认使用WiFi (0)
    
    ESP_LOGI(TAG, "从Settings加载网络类型: %d", network_type);
    
    return network_type == 1 ? NetworkType::ML307 : NetworkType::WIFI;
}

void DualNetworkBoard::SaveNetworkTypeToSettings(NetworkType type) {
    Settings settings("network", true);
    int network_type = (type == NetworkType::ML307) ? 1 : 0;
    
    ESP_LOGI(TAG, "保存网络类型到Settings: %d", network_type);
    
    settings.SetInt("type", network_type);
}

void DualNetworkBoard::InitializeCurrentBoard() {
    if (network_type_ == NetworkType::ML307) {
        ESP_LOGI(TAG, "初始化ML307板卡");
        current_board_ = std::make_unique<Ml307Board>(ml307_tx_pin_, ml307_rx_pin_, ml307_rx_buffer_size_);
    } else {
        ESP_LOGI(TAG, "初始化WiFi板卡");
        current_board_ = std::make_unique<WifiBoard>();
    }
}

void DualNetworkBoard::SwitchNetType() {
    if (network_type_ == NetworkType::WIFI) {    
        ESP_LOGI(TAG, "切换到ML307模式");
        SaveNetworkTypeToSettings(NetworkType::ML307);
    } else {
        ESP_LOGI(TAG, "切换到WiFi模式");
        SaveNetworkTypeToSettings(NetworkType::WIFI);
    }
}
 
std::string DualNetworkBoard::GetBoardType() {
    return "ml307_wifi";
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

Http* DualNetworkBoard::CreateHttp() {
    return current_board_->CreateHttp();
}

WebSocket* DualNetworkBoard::CreateWebSocket() {
    return current_board_->CreateWebSocket();
}

Mqtt* DualNetworkBoard::CreateMqtt() {
    return current_board_->CreateMqtt();
}

Udp* DualNetworkBoard::CreateUdp() {
    return current_board_->CreateUdp();
}

const char* DualNetworkBoard::GetNetworkStateIcon() {
    return current_board_->GetNetworkStateIcon();
}

void DualNetworkBoard::SetPowerSaveMode(bool enabled) {
    current_board_->SetPowerSaveMode(enabled);
}

std::string DualNetworkBoard::GetBoardJson() {   
    return current_board_->GetBoardJson();
} 