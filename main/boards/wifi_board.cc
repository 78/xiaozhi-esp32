#include "wifi_board.h"
#include "application.h"
#include "system_info.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http.h>
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
    display->SetText(std::string("Connect to WiFi\n") + wifi_station.GetSsid());
    wifi_station.Start();
    if (!wifi_station.IsConnected()) {
        application.Alert("Info", "Configuring WiFi");
        builtin_led->SetBlue();
        builtin_led->Blink(1000, 500);
        auto& wifi_ap = WifiConfigurationAp::GetInstance();
        wifi_ap.SetSsidPrefix("Xiaozhi");
        wifi_ap.Start();
        display->SetText("请用手机连接Xiaozhi开头Wifi,连接成功后进入192.168.4.1进行配网,配网完成进入https://xiaozhi.me/注册后输入验证码配置");

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
    std::string url = CONFIG_WEBSOCKET_URL;
    if (url.find("wss://") == 0) {
            return new WebSocket(new TlsTransport());
        } else {
            return new WebSocket(new TcpTransport());
        }
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
