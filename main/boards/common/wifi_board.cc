#include "wifi_board.h"

#include "display.h"
#include "application.h"
#include "system_info.h"
#include "font_awesome_symbols.h"
#include "settings.h"
#include "assets/lang_config.h"

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
#include <ssid_manager.h>

static const char *TAG = "WifiBoard";  // 定义日志标签

// WifiBoard类的构造函数
WifiBoard::WifiBoard() {
    // 从设置中读取是否强制进入WiFi配置模式
    Settings settings("wifi", true);
    wifi_config_mode_ = settings.GetInt("force_ap") == 1;
    if (wifi_config_mode_) {
        ESP_LOGI(TAG, "force_ap is set to 1, reset to 0");  // 记录日志
        settings.SetInt("force_ap", 0);  // 重置标志位
    }
}

// 获取板子类型的函数
std::string WifiBoard::GetBoardType() {
    return "wifi";  // 返回板子类型为"wifi"
}

// 进入WiFi配置模式的函数
void WifiBoard::EnterWifiConfigMode() {
    auto& application = Application::GetInstance();
    application.SetDeviceState(kDeviceStateWifiConfiguring);  // 设置设备状态为WiFi配置中

    auto& wifi_ap = WifiConfigurationAp::GetInstance();
    wifi_ap.SetLanguage(Lang::CODE);  // 设置语言
    wifi_ap.SetSsidPrefix("Xiaozhi");  // 设置WiFi热点的SSID前缀
    wifi_ap.Start();  // 启动WiFi配置热点

    // 显示WiFi配置热点的SSID和Web服务器URL
    std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT;
    hint += wifi_ap.GetSsid();
    hint += Lang::Strings::ACCESS_VIA_BROWSER;
    hint += wifi_ap.GetWebServerUrl();
    hint += "\n\n";
    
    // 播报配置WiFi的提示
    application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::P3_WIFICONFIG);
    
    // 等待配置完成，设备重启
    while (true) {
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);  // 获取当前空闲内存
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);  // 获取最小空闲内存
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);  // 记录内存信息
        vTaskDelay(pdMS_TO_TICKS(10000));  // 延迟10秒
    }
}

// 启动网络的函数
void WifiBoard::StartNetwork() {
    // 如果强制进入WiFi配置模式，则直接进入配置模式
    if (wifi_config_mode_) {
        EnterWifiConfigMode();
        return;
    }

    // 如果没有配置WiFi SSID，则进入WiFi配置模式
    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    if (ssid_list.empty()) {
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }

    // 初始化WiFi Station模式
    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.OnScanBegin([this]() {
        auto display = Board::GetInstance().GetDisplay();
        display->ShowNotification(Lang::Strings::SCANNING_WIFI, 30000);  // 显示扫描WiFi的通知
    });
    wifi_station.OnConnect([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECT_TO;
        notification += ssid;
        notification += "...";
        display->ShowNotification(notification.c_str(), 30000);  // 显示连接WiFi的通知
    });
    wifi_station.OnConnected([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECTED_TO;
        notification += ssid;
        display->ShowNotification(notification.c_str(), 30000);  // 显示已连接WiFi的通知
    });
    wifi_station.Start();  // 启动WiFi Station

    // 尝试连接WiFi，如果失败则启动WiFi配置热点
    if (!wifi_station.WaitForConnected(60 * 1000)) {
        wifi_station.Stop();
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }
}

// 创建HTTP对象的函数
Http* WifiBoard::CreateHttp() {
    return new EspHttp();  // 返回基于ESP的HTTP对象
}

// 创建WebSocket对象的函数
WebSocket* WifiBoard::CreateWebSocket() {
#ifdef CONFIG_CONNECTION_TYPE_WEBSOCKET
    std::string url = CONFIG_WEBSOCKET_URL;
    if (url.find("wss://") == 0) {
        return new WebSocket(new TlsTransport());  // 使用TLS传输层
    } else {
        return new WebSocket(new TcpTransport());  // 使用TCP传输层
    }
#endif
    return nullptr;
}

// 创建MQTT对象的函数
Mqtt* WifiBoard::CreateMqtt() {
    return new EspMqtt();  // 返回基于ESP的MQTT对象
}

// 创建UDP对象的函数
Udp* WifiBoard::CreateUdp() {
    return new EspUdp();  // 返回基于ESP的UDP对象
}

// 获取网络状态图标的函数
const char* WifiBoard::GetNetworkStateIcon() {
    if (wifi_config_mode_) {
        return FONT_AWESOME_WIFI;  // WiFi配置模式，返回WiFi图标
    }
    auto& wifi_station = WifiStation::GetInstance();
    if (!wifi_station.IsConnected()) {
        return FONT_AWESOME_WIFI_OFF;  // 未连接WiFi，返回WiFi关闭图标
    }
    int8_t rssi = wifi_station.GetRssi();  // 获取WiFi信号强度
    if (rssi >= -60) {
        return FONT_AWESOME_WIFI;  // 信号强，返回WiFi图标
    } else if (rssi >= -70) {
        return FONT_AWESOME_WIFI_FAIR;  // 信号中等，返回中等信号图标
    } else {
        return FONT_AWESOME_WIFI_WEAK;  // 信号弱，返回弱信号图标
    }
}

// 获取板子信息的JSON格式字符串
std::string WifiBoard::GetBoardJson() {
    // 设置OTA的板子类型
    auto& wifi_station = WifiStation::GetInstance();
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    if (!wifi_config_mode_) {
        board_json += "\"ssid\":\"" + wifi_station.GetSsid() + "\",";  // WiFi SSID
        board_json += "\"rssi\":" + std::to_string(wifi_station.GetRssi()) + ",";  // WiFi信号强度
        board_json += "\"channel\":" + std::to_string(wifi_station.GetChannel()) + ",";  // WiFi信道
        board_json += "\"ip\":\"" + wifi_station.GetIpAddress() + "\",";  // IP地址
    }
    board_json += "\"mac\":\"" + SystemInfo::GetMacAddress() + "\"}";  // MAC地址
    return board_json;
}

// 设置省电模式的函数
void WifiBoard::SetPowerSaveMode(bool enabled) {
    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.SetPowerSaveMode(enabled);  // 设置WiFi省电模式
}

// 重置WiFi配置的函数
void WifiBoard::ResetWifiConfiguration() {
    // 设置标志位并重启设备以进入WiFi配置模式
    {
        Settings settings("wifi", true);
        settings.SetInt("force_ap", 1);  // 设置强制进入WiFi配置模式的标志
    }
    GetDisplay()->ShowNotification(Lang::Strings::ENTERING_WIFI_CONFIG_MODE);  // 显示进入WiFi配置模式的通知
    vTaskDelay(pdMS_TO_TICKS(1000));  // 延迟1秒
    esp_restart();  // 重启设备
}