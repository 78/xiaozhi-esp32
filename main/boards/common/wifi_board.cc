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
#include <esp_wifi.h> // 添加此头文件以解决未声明的符号问题
#include <esp_err.h>  // 确保包含错误处理相关的头文件

static const char *TAG = "WifiBoard";

WifiBoard::WifiBoard()
{
    Settings settings("wifi", true);
    wifi_config_mode_ = settings.GetInt("force_ap") == 1;
    if (wifi_config_mode_)
    {
        ESP_LOGI(TAG, "force_ap is set to 1, reset to 0");
        settings.SetInt("force_ap", 0);
    }
}

std::string WifiBoard::GetBoardType()
{
    return "wifi";
}

void WifiBoard::EnterWifiConfigMode()
{
    auto &application = Application::GetInstance();
    application.SetDeviceState(kDeviceStateWifiConfiguring);

    /* ---------- 关键：网络接口要在 Wi-Fi init 之前 ---------- */
    ESP_ERROR_CHECK(esp_netif_init());
    (void)esp_netif_create_default_wifi_sta(); // 只需要这一行
    /* --------------------------------------------------------- */

    // 确保WiFi已初始化并启动为STA模式
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // 确保使用正确的类型
    esp_err_t err = esp_wifi_init(&cfg);                 // 调用esp_wifi_init
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    { // 修改错误检查条件
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // 调用esp_wifi_set_mode
    ESP_ERROR_CHECK(esp_wifi_start());                 // 调用esp_wifi_start

    // 启动 SmartConfig 配网
    auto &wifi_ap = WifiConfigurationAp::GetInstance();
    wifi_ap.SetLanguage(Lang::CODE);
    wifi_ap.SetSsidPrefix("Xiaozhi");
    wifi_ap.StartSmartConfig();

    // 提示用户使用手机APP进行配网
    std::string hint = "请使用手机APP进行一键配网(SmartConfig/EasyLink/AirKiss等)\n";
    hint += "配网成功后设备会自动联网。\n\n";
    application.Alert("智能配网模式", hint.c_str(), "", Lang::Sounds::P3_WIFICONFIG);

    // 等待WiFi连接成功
    auto &wifi_station = WifiStation::GetInstance();
    while (!wifi_station.IsConnected())
    {
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 连接成功后，停止SmartConfig，进入正常联网流程
    application.Alert("WiFi已连接", "设备已成功连接到WiFi", "", Lang::Sounds::P3_WIFICONFIG);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void WifiBoard::StartNetwork()
{
    // User can press BOOT button while starting to enter WiFi configuration mode
    if (wifi_config_mode_)
    {
        EnterWifiConfigMode();
        return;
    }

    // If no WiFi SSID is configured, enter WiFi configuration mode
    auto &ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    if (ssid_list.empty())
    {
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }

    auto &wifi_station = WifiStation::GetInstance();
    wifi_station.OnScanBegin([this]()
                             {
        auto display = Board::GetInstance().GetDisplay();
        display->ShowNotification(Lang::Strings::SCANNING_WIFI, 30000); });
    wifi_station.OnConnect([this](const std::string &ssid)
                           {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECT_TO;
        notification += ssid;
        notification += "...";
        display->ShowNotification(notification.c_str(), 30000); });
    wifi_station.OnConnected([this](const std::string &ssid)
                             {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECTED_TO;
        notification += ssid;
        display->ShowNotification(notification.c_str(), 30000); });
    wifi_station.Start();

    // Try to connect to WiFi, if failed, launch the WiFi configuration AP
    if (!wifi_station.WaitForConnected(60 * 1000))
    {
        wifi_station.Stop();
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }
}

Http *WifiBoard::CreateHttp()
{
    return new EspHttp();
}

WebSocket *WifiBoard::CreateWebSocket()
{
    Settings settings("websocket", false);
    std::string url = settings.GetString("url");
    if (url.find("wss://") == 0)
    {
        return new WebSocket(new TlsTransport());
    }
    else
    {
        return new WebSocket(new TcpTransport());
    }
    return nullptr;
}

Mqtt *WifiBoard::CreateMqtt()
{
    return new EspMqtt();
}

Udp *WifiBoard::CreateUdp()
{
    return new EspUdp();
}

const char *WifiBoard::GetNetworkStateIcon()
{
    if (wifi_config_mode_)
    {
        return FONT_AWESOME_WIFI;
    }
    auto &wifi_station = WifiStation::GetInstance();
    if (!wifi_station.IsConnected())
    {
        return FONT_AWESOME_WIFI_OFF;
    }
    int8_t rssi = wifi_station.GetRssi();
    if (rssi >= -60)
    {
        return FONT_AWESOME_WIFI;
    }
    else if (rssi >= -70)
    {
        return FONT_AWESOME_WIFI_FAIR;
    }
    else
    {
        return FONT_AWESOME_WIFI_WEAK;
    }
}

std::string WifiBoard::GetBoardJson()
{
    // Set the board type for OTA
    auto &wifi_station = WifiStation::GetInstance();
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    if (!wifi_config_mode_)
    {
        board_json += "\"ssid\":\"" + wifi_station.GetSsid() + "\",";
        board_json += "\"rssi\":" + std::to_string(wifi_station.GetRssi()) + ",";
        board_json += "\"channel\":" + std::to_string(wifi_station.GetChannel()) + ",";
        board_json += "\"ip\":\"" + wifi_station.GetIpAddress() + "\",";
    }
    board_json += "\"mac\":\"" + SystemInfo::GetMacAddress() + "\"}";
    return board_json;
}

void WifiBoard::SetPowerSaveMode(bool enabled)
{
    auto &wifi_station = WifiStation::GetInstance();
    wifi_station.SetPowerSaveMode(enabled);
}

void WifiBoard::ResetWifiConfiguration()
{
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

std::string WifiBoard::GetDeviceStatusJson()
{
    /*
     * 返回设备状态JSON
     *
     * 返回的JSON结构如下：
     * {
     *     "audio_speaker": {
     *         "volume": 70
     *     },
     *     "screen": {
     *         "brightness": 100,
     *         "theme": "light"
     *     },
     *     "battery": {
     *         "level": 50,
     *         "charging": true
     *     },
     *     "network": {
     *         "type": "wifi",
     *         "ssid": "Xiaozhi",
     *         "rssi": -60
     *     },
     *     "chip": {
     *         "temperature": 25
     *     }
     * }
     */
    auto &board = Board::GetInstance();
    auto root = cJSON_CreateObject();

    // Audio speaker
    auto audio_speaker = cJSON_CreateObject();
    auto audio_codec = board.GetAudioCodec();
    if (audio_codec)
    {
        cJSON_AddNumberToObject(audio_speaker, "volume", audio_codec->output_volume());
    }
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

    // Screen brightness
    auto backlight = board.GetBacklight();
    auto screen = cJSON_CreateObject();
    if (backlight)
    {
        cJSON_AddNumberToObject(screen, "brightness", backlight->brightness());
    }
    auto display = board.GetDisplay();
    if (display && display->height() > 64)
    { // For LCD display only
        cJSON_AddStringToObject(screen, "theme", display->GetTheme().c_str());
    }
    cJSON_AddItemToObject(root, "screen", screen);

    // Battery
    int battery_level = 0;
    bool charging = false;
    bool discharging = false;
    if (board.GetBatteryLevel(battery_level, charging, discharging))
    {
        cJSON *battery = cJSON_CreateObject();
        cJSON_AddNumberToObject(battery, "level", battery_level);
        cJSON_AddBoolToObject(battery, "charging", charging);
        cJSON_AddItemToObject(root, "battery", battery);
    }

    // Network
    auto network = cJSON_CreateObject();
    auto &wifi_station = WifiStation::GetInstance();
    cJSON_AddStringToObject(network, "type", "wifi");
    cJSON_AddStringToObject(network, "ssid", wifi_station.GetSsid().c_str());
    int rssi = wifi_station.GetRssi();
    if (rssi >= -60)
    {
        cJSON_AddStringToObject(network, "signal", "strong");
    }
    else if (rssi >= -70)
    {
        cJSON_AddStringToObject(network, "signal", "medium");
    }
    else
    {
        cJSON_AddStringToObject(network, "signal", "weak");
    }
    cJSON_AddItemToObject(root, "network", network);

    // Chip
    float esp32temp = 0.0f;
    if (board.GetTemperature(esp32temp))
    {
        auto chip = cJSON_CreateObject();
        cJSON_AddNumberToObject(chip, "temperature", esp32temp);
        cJSON_AddItemToObject(root, "chip", chip);
    }

    auto json_str = cJSON_PrintUnformatted(root);
    std::string json(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return json;
}