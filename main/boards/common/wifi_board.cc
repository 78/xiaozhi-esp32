#include "wifi_board.h"
#include "application.h"
#include "system_info.h"
#include "font_awesome_symbols.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http.h>
#include <esp_mqtt.h>
#include <esp_udp.h>
#include <tcp_transport.h>
#include <tls_transport.h>
#include <web_socket.h>
#include <esp_log.h>

#include <wifi_station.h>
#include <wifi_configuration_ap.h>

static const char *TAG = "WifiBoard";

static std::string rssi_to_string(int rssi) {
    if (rssi >= -55) {
        return "Very good";
    } else if (rssi >= -65) {
        return "Good";
    } else if (rssi >= -75) {
        return "Fair";
    } else if (rssi >= -85) {
        return "Poor";
    } else {
        return "No network";
    }
}

void WifiBoard::StartNetwork() {
    auto& application = Application::GetInstance();
    auto display = Board::GetInstance().GetDisplay();
    auto builtin_led = Board::GetInstance().GetBuiltinLed();

    // Try to connect to WiFi, if failed, launch the WiFi configuration AP
    auto& wifi_station = WifiStation::GetInstance();
    display->SetStatus(std::string("正在连接 ") + wifi_station.GetSsid());
    wifi_station.Start();
    if (!wifi_station.IsConnected()) {
        builtin_led->SetBlue();
        builtin_led->Blink(1000, 500);
        auto& wifi_ap = WifiConfigurationAp::GetInstance();
        wifi_ap.SetSsidPrefix("Xiaozhi");
        wifi_ap.Start();
        
        // 播报配置 WiFi 的提示
        application.Alert("Info", "Configuring WiFi");

        // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL
        std::string hint = "请在手机上连接热点 ";
        hint += wifi_ap.GetSsid();
        hint += "，然后打开浏览器访问 ";
        hint += wifi_ap.GetWebServerUrl();

        display->SetStatus(hint);
        
        // Wait forever until reset after configuration
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void WifiBoard::Initialize() {
    ESP_LOGI(TAG, "Initializing WifiBoard");
}

Http* WifiBoard::CreateHttp() {
    return new EspHttp();
}

WebSocket* WifiBoard::CreateWebSocket() {
#ifdef CONFIG_CONNECTION_TYPE_WEBSOCKET
    std::string url = CONFIG_WEBSOCKET_URL;
    if (url.find("wss://") == 0) {
        return new WebSocket(new TlsTransport());
    } else {
        return new WebSocket(new TcpTransport());
    }
#endif
    return nullptr;
}

Mqtt* WifiBoard::CreateMqtt() {
    return new EspMqtt();
}

Udp* WifiBoard::CreateUdp() {
    return new EspUdp();
}

bool WifiBoard::GetNetworkState(std::string& network_name, int& signal_quality, std::string& signal_quality_text) {
    if (wifi_config_mode_) {
        auto& wifi_ap = WifiConfigurationAp::GetInstance();
        network_name = wifi_ap.GetSsid();
        signal_quality = -99;
        signal_quality_text = wifi_ap.GetWebServerUrl();
        return true;
    }
    auto& wifi_station = WifiStation::GetInstance();
    if (!wifi_station.IsConnected()) {
        return false;
    }
    network_name = wifi_station.GetSsid();
    signal_quality = wifi_station.GetRssi();
    signal_quality_text = rssi_to_string(signal_quality);
    return signal_quality != -1;
}

const char* WifiBoard::GetNetworkStateIcon() {
    if (wifi_config_mode_) {
        return FONT_AWESOME_WIFI;
    }
    auto& wifi_station = WifiStation::GetInstance();
    if (!wifi_station.IsConnected()) {
        return FONT_AWESOME_WIFI_OFF;
    }
    int8_t rssi = wifi_station.GetRssi();
    if (rssi >= -55) {
        return FONT_AWESOME_WIFI;
    } else if (rssi >= -65) {
        return FONT_AWESOME_WIFI_FAIR;
    } else {
        return FONT_AWESOME_WIFI_WEAK;
    }
}

std::string WifiBoard::GetBoardJson() {
    // Set the board type for OTA
    auto& wifi_station = WifiStation::GetInstance();
    std::string board_type = BOARD_TYPE;
    std::string board_json = std::string("{\"type\":\"" + board_type + "\",");
    if (!wifi_config_mode_) {
        board_json += "\"ssid\":\"" + wifi_station.GetSsid() + "\",";
        board_json += "\"rssi\":" + std::to_string(wifi_station.GetRssi()) + ",";
        board_json += "\"channel\":" + std::to_string(wifi_station.GetChannel()) + ",";
        board_json += "\"ip\":\"" + wifi_station.GetIpAddress() + "\",";
    }
    board_json += "\"mac\":\"" + SystemInfo::GetMacAddress() + "\"}";
    return board_json;
}

void WifiBoard::SetPowerSaveMode(bool enabled) {
    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.SetPowerSaveMode(enabled);
}
