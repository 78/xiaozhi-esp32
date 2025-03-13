#include "ml307_board.h"

#include "application.h"
#include "display.h"
#include "font_awesome_symbols.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <ml307_http.h>
#include <ml307_ssl_transport.h>
#include <web_socket.h>
#include <ml307_mqtt.h>
#include <ml307_udp.h>
#include <opus_encoder.h>

static const char *TAG = "Ml307Board";  // 定义日志标签

// Ml307Board类的构造函数
Ml307Board::Ml307Board(gpio_num_t tx_pin, gpio_num_t rx_pin, size_t rx_buffer_size) : modem_(tx_pin, rx_pin, rx_buffer_size) {
    // 初始化ML307模块，设置TX、RX引脚和接收缓冲区大小
}

// 获取板子类型的函数
std::string Ml307Board::GetBoardType() {
    return "ml307";  // 返回板子类型为"ml307"
}

// 启动网络的函数
void Ml307Board::StartNetwork() {
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(Lang::Strings::DETECTING_MODULE);  // 设置显示状态为“检测模块”
    modem_.SetDebug(false);  // 禁用调试模式
    modem_.SetBaudRate(921600);  // 设置波特率为921600

    auto& application = Application::GetInstance();
    // 如果处于低功耗模式，模块准备好事件将由模块触发（由于复位）
    modem_.OnMaterialReady([this, &application]() {
        ESP_LOGI(TAG, "ML307 material ready");  // 记录模块准备就绪的日志
        application.Schedule([this, &application]() {
            application.SetDeviceState(kDeviceStateIdle);  // 设置设备状态为空闲
            WaitForNetworkReady();  // 等待网络就绪
        });
    });

    WaitForNetworkReady();  // 等待网络就绪
}

// 等待网络就绪的函数
void Ml307Board::WaitForNetworkReady() {
    auto& application = Application::GetInstance();
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(Lang::Strings::REGISTERING_NETWORK);  // 设置显示状态为“注册网络”
    int result = modem_.WaitForNetworkReady();  // 等待网络就绪
    if (result == -1) {
        application.Alert(Lang::Strings::ERROR, Lang::Strings::PIN_ERROR, "sad", Lang::Sounds::P3_ERR_PIN);  // PIN错误提示
        return;
    } else if (result == -2) {
        application.Alert(Lang::Strings::ERROR, Lang::Strings::REG_ERROR, "sad", Lang::Sounds::P3_ERR_REG);  // 注册错误提示
        return;
    }

    // 打印ML307模块信息
    std::string module_name = modem_.GetModuleName();  // 获取模块名称
    std::string imei = modem_.GetImei();  // 获取IMEI
    std::string iccid = modem_.GetIccid();  // 获取ICCID
    ESP_LOGI(TAG, "ML307 Module: %s", module_name.c_str());  // 记录模块名称
    ESP_LOGI(TAG, "ML307 IMEI: %s", imei.c_str());  // 记录IMEI
    ESP_LOGI(TAG, "ML307 ICCID: %s", iccid.c_str());  // 记录ICCID

    // 关闭所有之前的连接
    modem_.ResetConnections();  // 重置连接
}

// 创建HTTP对象的函数
Http* Ml307Board::CreateHttp() {
    return new Ml307Http(modem_);  // 返回基于ML307的HTTP对象
}

// 创建WebSocket对象的函数
WebSocket* Ml307Board::CreateWebSocket() {
    return new WebSocket(new Ml307SslTransport(modem_, 0));  // 返回基于ML307的WebSocket对象
}

// 创建MQTT对象的函数
Mqtt* Ml307Board::CreateMqtt() {
    return new Ml307Mqtt(modem_, 0);  // 返回基于ML307的MQTT对象
}

// 创建UDP对象的函数
Udp* Ml307Board::CreateUdp() {
    return new Ml307Udp(modem_, 0);  // 返回基于ML307的UDP对象
}

// 获取网络状态图标的函数
const char* Ml307Board::GetNetworkStateIcon() {
    if (!modem_.network_ready()) {
        return FONT_AWESOME_SIGNAL_OFF;  // 网络未就绪，返回无信号图标
    }
    int csq = modem_.GetCsq();  // 获取信号质量
    if (csq == -1) {
        return FONT_AWESOME_SIGNAL_OFF;  // 信号质量无效，返回无信号图标
    } else if (csq >= 0 && csq <= 14) {
        return FONT_AWESOME_SIGNAL_1;  // 信号质量1级
    } else if (csq >= 15 && csq <= 19) {
        return FONT_AWESOME_SIGNAL_2;  // 信号质量2级
    } else if (csq >= 20 && csq <= 24) {
        return FONT_AWESOME_SIGNAL_3;  // 信号质量3级
    } else if (csq >= 25 && csq <= 31) {
        return FONT_AWESOME_SIGNAL_4;  // 信号质量4级
    }

    ESP_LOGW(TAG, "Invalid CSQ: %d", csq);  // 记录无效信号质量的警告日志
    return FONT_AWESOME_SIGNAL_OFF;  // 返回无信号图标
}

// 获取板子信息的JSON格式字符串
std::string Ml307Board::GetBoardJson() {
    // 设置OTA的板子类型
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    board_json += "\"revision\":\"" + modem_.GetModuleName() + "\",";  // 模块名称
    board_json += "\"carrier\":\"" + modem_.GetCarrierName() + "\",";  // 运营商名称
    board_json += "\"csq\":\"" + std::to_string(modem_.GetCsq()) + "\",";  // 信号质量
    board_json += "\"imei\":\"" + modem_.GetImei() + "\",";  // IMEI
    board_json += "\"iccid\":\"" + modem_.GetIccid() + "\"}";  // ICCID
    return board_json;
}

// 设置省电模式的函数
void Ml307Board::SetPowerSaveMode(bool enabled) {
    // TODO: 实现ML307的省电模式
}