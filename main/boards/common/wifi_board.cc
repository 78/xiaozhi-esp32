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

static const char *TAG = "WifiBoard";

// ============================================================================
// 硬编码WiFi配置 - 请根据您的实际网络环境修改以下值
// ============================================================================
//
// 使用说明：
// 1. 将 "YOUR_WIFI_SSID" 替换为您的WiFi网络名称
// 2. 将 "YOUR_WIFI_PASSWORD" 替换为您的WiFi密码
// 3. 如果您的WiFi网络没有密码，请将密码设置为空字符串 ""
// 4. 重新编译并烧录固件
//
// 示例：
// static const char* HARDCODED_WIFI_SSID = "MyHomeWiFi";
// static const char* HARDCODED_WIFI_PASSWORD = "MyPassword123";
//
// 注意：
// - SSID和密码区分大小写
// - 确保WiFi网络在设备启动时可用
// - 如果连接失败，设备将进入WiFi配置模式作为后备方案
// ============================================================================

static const char* HARDCODED_WIFI_SSID = "dzkjtestwifi";
static const char* HARDCODED_WIFI_PASSWORD = "66666666";

// 配置选项：是否在硬编码WiFi连接失败时启用配置模式后备方案
// true: 连接失败时进入WiFi配置模式（推荐用于开发和调试）
// false: 连接失败时不进入配置模式，设备将无法联网
static const bool ENABLE_WIFI_CONFIG_FALLBACK = true;

WifiBoard::WifiBoard() {
    Settings settings("wifi", true);
    wifi_config_mode_ = settings.GetInt("force_ap") == 1;
    if (wifi_config_mode_) {
        ESP_LOGI(TAG, "force_ap is set to 1, reset to 0");
        settings.SetInt("force_ap", 0);
    }
}

std::string WifiBoard::GetBoardType() {
    return "wifi";
}

void WifiBoard::SetupHardcodedWifi() {
    ESP_LOGI(TAG, "设置硬编码WiFi凭据");
    ESP_LOGI(TAG, "目标WiFi网络: %s", HARDCODED_WIFI_SSID);
    ESP_LOGI(TAG, "密码长度: %d字符", strlen(HARDCODED_WIFI_PASSWORD));

    // 验证WiFi凭据是否已设置
    if (strcmp(HARDCODED_WIFI_SSID, "YOUR_WIFI_SSID") == 0) {
        ESP_LOGW(TAG, "警告：WiFi SSID尚未配置，请修改代码中的HARDCODED_WIFI_SSID");
    }
    if (strcmp(HARDCODED_WIFI_PASSWORD, "YOUR_WIFI_PASSWORD") == 0) {
        ESP_LOGW(TAG, "警告：WiFi密码尚未配置，请修改代码中的HARDCODED_WIFI_PASSWORD");
    }

    // 清除现有的WiFi配置
    auto& ssid_manager = SsidManager::GetInstance();
    ssid_manager.Clear();
    ESP_LOGI(TAG, "已清除现有WiFi配置");

    // 添加硬编码的WiFi凭据
    ssid_manager.AddSsid(HARDCODED_WIFI_SSID, HARDCODED_WIFI_PASSWORD);

    ESP_LOGI(TAG, "硬编码WiFi凭据设置完成");
}

bool WifiBoard::ConnectToHardcodedWifi() {
    ESP_LOGI(TAG, "开始连接到硬编码WiFi网络: %s", HARDCODED_WIFI_SSID);

    auto& wifi_station = WifiStation::GetInstance();

    // 设置回调函数
    wifi_station.OnScanBegin([this]() {
        auto display = Board::GetInstance().GetDisplay();
        display->ShowNotification(Lang::Strings::SCANNING_WIFI, 30000);
        ESP_LOGI(TAG, "开始扫描WiFi网络");
    });

    wifi_station.OnConnect([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECT_TO;
        notification += ssid;
        notification += "...";
        display->ShowNotification(notification.c_str(), 30000);
        ESP_LOGI(TAG, "正在连接到WiFi: %s", ssid.c_str());
    });

    wifi_station.OnConnected([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECTED_TO;
        notification += ssid;
        display->ShowNotification(notification.c_str(), 30000);
        ESP_LOGI(TAG, "成功连接到WiFi: %s", ssid.c_str());
    });

    // 启动WiFi连接
    wifi_station.Start();

    // 等待连接，超时时间为60秒
    if (wifi_station.WaitForConnected(60 * 1000)) {
        ESP_LOGI(TAG, "硬编码WiFi连接成功");
        return true;
    } else {
        ESP_LOGE(TAG, "硬编码WiFi连接失败");
        wifi_station.Stop();
        return false;
    }
}

void WifiBoard::EnterWifiConfigMode() {
    auto& application = Application::GetInstance();
    application.SetDeviceState(kDeviceStateWifiConfiguring);

    auto& wifi_ap = WifiConfigurationAp::GetInstance();
    wifi_ap.SetLanguage(Lang::CODE);
    wifi_ap.SetSsidPrefix("独众AI伴侣");
    wifi_ap.Start();

    // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL
    std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT;
    hint += wifi_ap.GetSsid();
    hint += Lang::Strings::ACCESS_VIA_BROWSER;
    hint += wifi_ap.GetWebServerUrl();
    hint += "\n\n";
    
    // 播报配置 WiFi 的提示
    application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::P3_CCC_PW);
    
    // Wait forever until reset after configuration
    while (true) {
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void WifiBoard::StartNetwork() {
    ESP_LOGI(TAG, "启动网络连接（使用硬编码WiFi凭据）");

    // 如果用户强制进入WiFi配置模式，仍然允许
    if (wifi_config_mode_) {
        ESP_LOGW(TAG, "强制进入WiFi配置模式");
        EnterWifiConfigMode();
        return;
    }

    // 设置硬编码的WiFi凭据
    SetupHardcodedWifi();

    // 尝试连接到硬编码的WiFi网络
    if (ConnectToHardcodedWifi()) {
        ESP_LOGI(TAG, "硬编码WiFi网络连接成功");
        return;
    }

    // 如果硬编码WiFi连接失败，根据配置决定处理方式
    ESP_LOGE(TAG, "硬编码WiFi网络连接失败");

    if (ENABLE_WIFI_CONFIG_FALLBACK) {
        // 进入WiFi配置模式作为后备方案
        ESP_LOGW(TAG, "硬编码WiFi连接失败，进入WiFi配置模式作为后备方案");
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
    } else {
        // 不使用后备方案，显示错误信息
        auto display = Board::GetInstance().GetDisplay();
        display->ShowNotification("WiFi连接失败，请检查网络设置", 10000);
        ESP_LOGE(TAG, "无法连接到硬编码WiFi网络，设备将无法正常工作");

        // 可选：您可以在这里添加其他错误处理逻辑
        // 例如：重启设备、进入低功耗模式等
    }
}

Http* WifiBoard::CreateHttp() {
    return new EspHttp();
}

WebSocket* WifiBoard::CreateWebSocket() {
    // WebSocket URL现在从OTA动态获取，根据协议选择传输层
    // ws:// 使用TCP传输，wss:// 使用TLS传输
    auto& application = Application::GetInstance();
    auto& ota = application.GetOta();
    
    if (ota.HasWebsocketConfig()) {
        std::string url = ota.GetWebsocketUrl();
        if (url.find("wss://") == 0) {
            // 使用TLS传输
            return new WebSocket(new TlsTransport());
        } else if (url.find("ws://") == 0) {
            // 使用TCP传输
            return new WebSocket(new TcpTransport());
        }
    }
    
    // 默认使用TLS传输作为后备方案
    return new WebSocket(new TlsTransport());
}

Mqtt* WifiBoard::CreateMqtt() {
    return new EspMqtt();
}

Udp* WifiBoard::CreateUdp() {
    return new EspUdp();
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
    if (rssi >= -60) {
        return FONT_AWESOME_WIFI;
    } else if (rssi >= -70) {
        return FONT_AWESOME_WIFI_FAIR;
    } else {
        return FONT_AWESOME_WIFI_WEAK;
    }
}

std::string WifiBoard::GetBoardJson() {
    // Set the board type for OTA
    auto& wifi_station = WifiStation::GetInstance();
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
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

void WifiBoard::ResetWifiConfiguration() {
    // Set a flag and reboot the device to enter the network configuration mode
    {
        Settings settings("wifi", true);
        settings.SetInt("force_ap", 1);
    }
    GetDisplay()->ShowNotification(Lang::Strings::ENTERING_WIFI_CONFIG_MODE);
    vTaskDelay(pdMS_TO_TICKS(1000));
    // Reboot the device
    esp_restart();
}
