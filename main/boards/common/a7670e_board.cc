#include "a7670e_board.h"

#include "assets/lang_config.h"
#include "display.h"
#include "application.h"
#include "network_interface.h"
#include <font_awesome.h>

#include <esp_log.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <cstring>
#include <cJSON.h>
#include <algorithm>

static const char* TAG = "A7670EBoard";

// 最大重试次数
static constexpr int MODEM_DETECT_MAX_RETRIES = 30;
static constexpr int NETWORK_REG_MAX_RETRIES = 6;
static constexpr int UART_BUFFER_SIZE = 1024;
static constexpr int AT_TIMEOUT_MS = 5000;

// UART端口号（使用UART_NUM_1，避免与其他外设冲突）
#define A7670E_UART_NUM UART_NUM_1

/**
 * A7670E网络接口实现
 */
class A7670ENetwork : public NetworkInterface {
private:
    bool network_ready_ = false;
    std::string carrier_name_;
    int csq_ = -1;
    std::string imei_;
    std::string iccid_;

public:
    A7670ENetwork() = default;
    ~A7670ENetwork() = default;

    bool network_ready() const { return network_ready_; }
    void set_network_ready(bool ready) { network_ready_ = ready; }

    std::string GetCarrierName() const { return carrier_name_; }
    void SetCarrierName(const std::string& name) { carrier_name_ = name; }
    
    int GetCsq() const { return csq_; }
    void SetCsq(int csq) { csq_ = csq; }
    
    std::string GetImei() const { return imei_; }
    void SetImei(const std::string& imei) { imei_ = imei; }
    
    std::string GetIccid() const { return iccid_; }
    void SetIccid(const std::string& iccid) { iccid_ = iccid; }

    std::unique_ptr<Http> CreateHttp(int timeout_ms) override {
        (void)timeout_ms;
        return nullptr;
    }

    std::unique_ptr<WebSocket> CreateWebSocket(int timeout_ms) override {
        (void)timeout_ms;
        return nullptr;
    }

    std::unique_ptr<Mqtt> CreateMqtt(int timeout_ms) override {
        (void)timeout_ms;
        return nullptr;
    }

    std::unique_ptr<Udp> CreateUdp(int timeout_ms) override {
        (void)timeout_ms;
        return nullptr;
    }

    std::unique_ptr<Tcp> CreateTcp(int connect_id = -1) override {
        (void)connect_id;
        return nullptr;
    }

    std::unique_ptr<Tcp> CreateSsl(int connect_id = -1) override {
        (void)connect_id;
        return nullptr;
    }
};

A7670EBoard::A7670EBoard(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t power_pin)
    : tx_pin_(tx_pin), rx_pin_(rx_pin), power_pin_(power_pin) {
    // 初始化GPIO控制引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << power_pin_),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // 根据微雪文档：GPIO33/22拉低开机，拉高关机
    // 初始化时拉低使模块开机
    gpio_set_level(power_pin_, 0);
    ESP_LOGI(TAG, "A7670E power pin (GPIO%d) set to LOW (power on)", power_pin_);
    
    // 等待模块启动（约3-5秒）
    vTaskDelay(pdMS_TO_TICKS(3000));
}

A7670EBoard::~A7670EBoard() = default;

/**
 * 发送AT指令并等待响应
 */
static bool SendATCommand(const char* cmd, char* response, size_t response_size, int timeout_ms = AT_TIMEOUT_MS) {
    // 清空接收缓冲区
    uart_flush(A7670E_UART_NUM);
    
    // 发送AT指令
    std::string cmd_str = std::string(cmd) + "\r\n";
    uart_write_bytes(A7670E_UART_NUM, cmd_str.c_str(), cmd_str.length());
    ESP_LOGD(TAG, "Sent: %s", cmd);
    
    // 等待响应
    int len = uart_read_bytes(A7670E_UART_NUM, (uint8_t*)response, response_size - 1, pdMS_TO_TICKS(timeout_ms));
    if (len <= 0) {
        ESP_LOGW(TAG, "No response to: %s", cmd);
        return false;
    }
    
    response[len] = '\0';
    ESP_LOGD(TAG, "Received: %s", response);
    
    // 检查响应是否包含OK
    return (strstr(response, "OK") != nullptr || strstr(response, "ok") != nullptr);
}

/**
 * 发送AT指令并解析响应（带参数）
 */
static bool SendATCommandParse(const char* cmd, const char* prefix, std::string& result) {
    char response[512];
    if (!SendATCommand(cmd, response, sizeof(response))) {
        return false;
    }
    
    // 查找前缀
    char* prefix_pos = strstr(response, prefix);
    if (prefix_pos == nullptr) {
        return false;
    }
    
    // 提取值（跳过前缀和可能的引号）
    prefix_pos += strlen(prefix);
    while (*prefix_pos == ' ' || *prefix_pos == ':' || *prefix_pos == '"') {
        prefix_pos++;
    }
    
    // 提取到换行或结束
    char* end_pos = strchr(prefix_pos, '\r');
    if (end_pos == nullptr) {
        end_pos = strchr(prefix_pos, '\n');
    }
    if (end_pos == nullptr) {
        end_pos = prefix_pos + strlen(prefix_pos);
    }
    
    result = std::string(prefix_pos, end_pos - prefix_pos);
    return true;
}

void A7670EBoard::NetworkTask() {
    // 通知开始检测模块
    OnNetworkEvent(NetworkEvent::ModemDetecting);
    
    // 初始化UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(A7670E_UART_NUM, UART_BUFFER_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(A7670E_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(A7670E_UART_NUM, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    ESP_LOGI(TAG, "UART initialized: TX=%d, RX=%d, Baud=115200", tx_pin_, rx_pin_);
    
    // 等待UART稳定
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 检测模块（发送AT指令）
    int detect_retries = 0;
    bool modem_detected = false;
    
    while (detect_retries < MODEM_DETECT_MAX_RETRIES) {
        char response[256];
        if (SendATCommand("AT", response, sizeof(response), 2000)) {
            ESP_LOGI(TAG, "A7670E modem detected");
            modem_detected = true;
            break;
        }
        detect_retries++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    if (!modem_detected) {
        ESP_LOGE(TAG, "Failed to detect A7670E modem after %d retries", MODEM_DETECT_MAX_RETRIES);
        OnNetworkEvent(NetworkEvent::ModemErrorInitFailed);
        uart_driver_delete(A7670E_UART_NUM);
        return;
    }
    
    // 获取模块信息
    std::string imei_str, iccid_str;
    if (SendATCommandParse("AT+GSN", "+GSN:", imei_str)) {
        // 清理IMEI字符串（去除空格和换行）
        imei_str.erase(std::remove(imei_str.begin(), imei_str.end(), ' '), imei_str.end());
        imei_str.erase(std::remove(imei_str.begin(), imei_str.end(), '\r'), imei_str.end());
        imei_str.erase(std::remove(imei_str.begin(), imei_str.end(), '\n'), imei_str.end());
        network_->SetImei(imei_str);
        ESP_LOGI(TAG, "IMEI: %s", imei_str.c_str());
    }
    
    if (SendATCommandParse("AT+CCID", "+CCID:", iccid_str)) {
        // 清理ICCID字符串（去除空格和换行）
        iccid_str.erase(std::remove(iccid_str.begin(), iccid_str.end(), ' '), iccid_str.end());
        iccid_str.erase(std::remove(iccid_str.begin(), iccid_str.end(), '\r'), iccid_str.end());
        iccid_str.erase(std::remove(iccid_str.begin(), iccid_str.end(), '\n'), iccid_str.end());
        network_->SetIccid(iccid_str);
        ESP_LOGI(TAG, "ICCID: %s", iccid_str.c_str());
    }
    
    // 检查SIM卡状态
    char response[256];
    if (!SendATCommand("AT+CPIN?", response, sizeof(response))) {
        ESP_LOGE(TAG, "Failed to check SIM card status");
        OnNetworkEvent(NetworkEvent::ModemErrorNoSim);
        uart_driver_delete(A7670E_UART_NUM);
        return;
    }
    
    if (strstr(response, "READY") == nullptr) {
        ESP_LOGE(TAG, "SIM card not ready: %s", response);
        OnNetworkEvent(NetworkEvent::ModemErrorNoSim);
        uart_driver_delete(A7670E_UART_NUM);
        return;
    }
    
    ESP_LOGI(TAG, "SIM card ready");
    
    // 设置网络注册通知
    SendATCommand("AT+CREG=2", response, sizeof(response)); // 启用网络注册状态通知
    
    // 通知开始网络注册
    OnNetworkEvent(NetworkEvent::Connecting);
    
    // 等待网络注册
    int reg_retries = 0;
    bool network_registered = false;
    
    while (reg_retries < NETWORK_REG_MAX_RETRIES) {
        // 检查注册状态 AT+CREG?
        std::string creg_response;
        if (SendATCommandParse("AT+CREG?", "+CREG:", creg_response)) {
            // +CREG: 2,1 表示已注册到本地网络
            // +CREG: 2,5 表示已注册到漫游网络
            if (creg_response.find(",1") != std::string::npos || creg_response.find(",5") != std::string::npos) {
                network_registered = true;
                ESP_LOGI(TAG, "Network registered: %s", creg_response.c_str());
                break;
            }
        }
        
        reg_retries++;
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    
    if (!network_registered) {
        ESP_LOGE(TAG, "Failed to register network after %d retries", NETWORK_REG_MAX_RETRIES);
        OnNetworkEvent(NetworkEvent::ModemErrorRegDenied);
        uart_driver_delete(A7670E_UART_NUM);
        return;
    }
    
    // 获取运营商名称
    std::string carrier;
    if (SendATCommandParse("AT+COPS?", "+COPS:", carrier)) {
        // 解析运营商名称（格式：+COPS: 0,0,"CHINA MOBILE",7）
        size_t quote_start = carrier.find('"');
        if (quote_start != std::string::npos) {
            size_t quote_end = carrier.find('"', quote_start + 1);
            if (quote_end != std::string::npos) {
                network_->SetCarrierName(carrier.substr(quote_start + 1, quote_end - quote_start - 1));
                ESP_LOGI(TAG, "Carrier: %s", network_->GetCarrierName().c_str());
            }
        }
    }
    
    // 获取信号强度
    std::string csq_str;
    if (SendATCommandParse("AT+CSQ", "+CSQ:", csq_str)) {
        // 解析CSQ值（格式：+CSQ: 20,99）
        int csq = -1;
        if (sscanf(csq_str.c_str(), "%d", &csq) == 1) {
            network_->SetCsq(csq);
            ESP_LOGI(TAG, "CSQ: %d", csq);
        }
    }
    
    // 配置APN（如果需要）
    // 注意：A7670E通常可以自动识别APN，如果需要手动配置，可以取消注释以下代码
    // SendATCommand("AT+CGDCONT=1,\"IP\",\"your_apn\"", response, sizeof(response));
    
    // 激活PDP上下文
    if (!SendATCommand("AT+CGACT=1,1", response, sizeof(response))) {
        ESP_LOGW(TAG, "Failed to activate PDP context, trying again...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        SendATCommand("AT+CGACT=1,1", response, sizeof(response));
    }
    
    // 检查PDP上下文状态
    std::string pdp_status;
    if (SendATCommandParse("AT+CGACT?", "+CGACT:", pdp_status)) {
        if (pdp_status.find("1,1") != std::string::npos) {
            ESP_LOGI(TAG, "PDP context activated");
            network_->set_network_ready(true);
            OnNetworkEvent(NetworkEvent::Connected, network_->GetCarrierName());
        } else {
            ESP_LOGW(TAG, "PDP context not activated: %s", pdp_status.c_str());
        }
    }
    
    ESP_LOGI(TAG, "A7670E network initialization completed");
}

void A7670EBoard::StartNetwork() {
    // 创建网络初始化任务
    network_ = std::make_unique<A7670ENetwork>();
    
    xTaskCreate([](void* arg) {
        A7670EBoard* board = static_cast<A7670EBoard*>(arg);
        board->NetworkTask();
        vTaskDelete(NULL);
    }, "a7670e_net", 8192, this, 5, NULL);
}

NetworkInterface* A7670EBoard::GetNetwork() {
    return network_.get();
}

const char* A7670EBoard::GetNetworkStateIcon() {
    if (network_ == nullptr || !network_->network_ready()) {
        return FONT_AWESOME_SIGNAL_OFF;
    }
    int csq = network_->GetCsq();
    if (csq == -1) {
        return FONT_AWESOME_SIGNAL_OFF;
    } else if (csq >= 0 && csq <= 9) {
        return FONT_AWESOME_SIGNAL_WEAK;
    } else if (csq >= 10 && csq <= 14) {
        return FONT_AWESOME_SIGNAL_FAIR;
    } else if (csq >= 15 && csq <= 19) {
        return FONT_AWESOME_SIGNAL_GOOD;
    } else if (csq >= 20 && csq <= 31) {
        return FONT_AWESOME_SIGNAL_STRONG;
    }
    
    ESP_LOGW(TAG, "Invalid CSQ: %d", csq);
    return FONT_AWESOME_SIGNAL_OFF;
}

void A7670EBoard::OnNetworkEvent(NetworkEvent event, const std::string& data) {
    switch (event) {
        case NetworkEvent::ModemDetecting:
            ESP_LOGI(TAG, "Detecting A7670E modem...");
            break;
        case NetworkEvent::Connecting:
            ESP_LOGI(TAG, "Registering network...");
            break;
        case NetworkEvent::Connected:
            ESP_LOGI(TAG, "Network connected: %s", data.c_str());
            break;
        case NetworkEvent::Disconnected:
            ESP_LOGW(TAG, "Network disconnected");
            break;
        case NetworkEvent::ModemErrorNoSim:
            ESP_LOGE(TAG, "No SIM card detected");
            break;
        case NetworkEvent::ModemErrorRegDenied:
            ESP_LOGE(TAG, "Network registration denied");
            break;
        case NetworkEvent::ModemErrorInitFailed:
            ESP_LOGE(TAG, "A7670E initialization failed");
            break;
        case NetworkEvent::ModemErrorTimeout:
            ESP_LOGE(TAG, "Operation timeout");
            break;
        default:
            break;
    }
    
    if (network_event_callback_) {
        network_event_callback_(event, data);
    }
}

std::string A7670EBoard::GetBoardJson() {
    if (network_ == nullptr) {
        return R"({"type":"a7670e","name":"A7670E","status":"not_initialized"})";
    }
    
    std::string board_json = R"({"type":"a7670e","name":"A7670E")";
    board_json += R"(,"imei":")" + network_->GetImei() + R"(")";
    board_json += R"(,"iccid":")" + network_->GetIccid() + R"(")";
    board_json += R"(,"carrier":")" + network_->GetCarrierName() + R"(")";
    board_json += R"(,"csq":")" + std::to_string(network_->GetCsq()) + R"(")";
    board_json += R"(,"network_ready":)" + std::string(network_->network_ready() ? "true" : "false");
    board_json += "}";
    return board_json;
}

std::string A7670EBoard::GetDeviceStatusJson() {
    auto& board = Board::GetInstance();
    auto root = cJSON_CreateObject();
    
    // Audio speaker
    auto audio_speaker = cJSON_CreateObject();
    auto audio_codec = board.GetAudioCodec();
    if (audio_codec) {
        cJSON_AddNumberToObject(audio_speaker, "volume", audio_codec->output_volume());
    }
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);
    
    // Screen brightness
    auto backlight = board.GetBacklight();
    auto screen = cJSON_CreateObject();
    if (backlight) {
        cJSON_AddNumberToObject(screen, "brightness", backlight->brightness());
    }
    auto display = board.GetDisplay();
    if (display && display->height() > 64) {
        auto theme = display->GetTheme();
        if (theme != nullptr) {
            cJSON_AddStringToObject(screen, "theme", theme->name().c_str());
        }
    }
    cJSON_AddItemToObject(root, "screen", screen);
    
    // Battery
    int battery_level = 0;
    bool charging = false;
    bool discharging = false;
    if (board.GetBatteryLevel(battery_level, charging, discharging)) {
        cJSON* battery = cJSON_CreateObject();
        cJSON_AddNumberToObject(battery, "level", battery_level);
        cJSON_AddBoolToObject(battery, "charging", charging);
        cJSON_AddItemToObject(root, "battery", battery);
    }
    
    // Network
    auto network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "type", "cellular");
    if (network_ != nullptr) {
        cJSON_AddStringToObject(network, "carrier", network_->GetCarrierName().c_str());
        int csq = network_->GetCsq();
        if (csq == -1) {
            cJSON_AddStringToObject(network, "signal", "unknown");
        } else if (csq >= 0 && csq <= 14) {
            cJSON_AddStringToObject(network, "signal", "very weak");
        } else if (csq >= 15 && csq <= 19) {
            cJSON_AddStringToObject(network, "signal", "weak");
        } else if (csq >= 20 && csq <= 24) {
            cJSON_AddStringToObject(network, "signal", "medium");
        } else if (csq >= 25 && csq <= 31) {
            cJSON_AddStringToObject(network, "signal", "strong");
        }
    }
    cJSON_AddItemToObject(root, "network", network);
    
    auto json_str = cJSON_PrintUnformatted(root);
    std::string json(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return json;
}

void A7670EBoard::SetPowerSaveLevel(PowerSaveLevel level) {
    // TODO: 实现A7670E的省电模式
    (void)level;
}
