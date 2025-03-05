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

static const char *TAG = "Ml307Board";

Ml307Board::Ml307Board(gpio_num_t tx_pin, gpio_num_t rx_pin, size_t rx_buffer_size) : modem_(tx_pin, rx_pin, rx_buffer_size) {
}

std::string Ml307Board::GetBoardType() {
    return "ml307";
}

void Ml307Board::StartNetwork() {
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(Lang::Strings::DETECTING_MODULE);
    modem_.SetDebug(false);
    modem_.SetBaudRate(921600);

    auto& application = Application::GetInstance();
    // If low power, the material ready event will be triggered by the modem because of a reset
    modem_.OnMaterialReady([this, &application]() {
        ESP_LOGI(TAG, "ML307 material ready");
        application.Schedule([this, &application]() {
            application.SetDeviceState(kDeviceStateIdle);
            WaitForNetworkReady();
        });
    });

    WaitForNetworkReady();
}

void Ml307Board::WaitForNetworkReady() {
    auto& application = Application::GetInstance();
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(Lang::Strings::REGISTERING_NETWORK);
    int result = modem_.WaitForNetworkReady();
    if (result == -1) {
        application.Alert(Lang::Strings::ERROR, Lang::Strings::PIN_ERROR, "sad", Lang::Sounds::P3_ERR_PIN);
        return;
    } else if (result == -2) {
        application.Alert(Lang::Strings::ERROR, Lang::Strings::REG_ERROR, "sad", Lang::Sounds::P3_ERR_REG);
        return;
    }

    // Print the ML307 modem information
    std::string module_name = modem_.GetModuleName();
    std::string imei = modem_.GetImei();
    std::string iccid = modem_.GetIccid();
    ESP_LOGI(TAG, "ML307 Module: %s", module_name.c_str());
    ESP_LOGI(TAG, "ML307 IMEI: %s", imei.c_str());
    ESP_LOGI(TAG, "ML307 ICCID: %s", iccid.c_str());

    // Close all previous connections
    modem_.ResetConnections();
}

Http* Ml307Board::CreateHttp() {
    return new Ml307Http(modem_);
}

WebSocket* Ml307Board::CreateWebSocket() {
    return new WebSocket(new Ml307SslTransport(modem_, 0));
}

Mqtt* Ml307Board::CreateMqtt() {
    return new Ml307Mqtt(modem_, 0);
}

Udp* Ml307Board::CreateUdp() {
    return new Ml307Udp(modem_, 0);
}

const char* Ml307Board::GetNetworkStateIcon() {
    if (!modem_.network_ready()) {
        return FONT_AWESOME_SIGNAL_OFF;
    }
    int csq = modem_.GetCsq();
    if (csq == -1) {
        return FONT_AWESOME_SIGNAL_OFF;
    } else if (csq >= 0 && csq <= 14) {
        return FONT_AWESOME_SIGNAL_1;
    } else if (csq >= 15 && csq <= 19) {
        return FONT_AWESOME_SIGNAL_2;
    } else if (csq >= 20 && csq <= 24) {
        return FONT_AWESOME_SIGNAL_3;
    } else if (csq >= 25 && csq <= 31) {
        return FONT_AWESOME_SIGNAL_4;
    }

    ESP_LOGW(TAG, "Invalid CSQ: %d", csq);
    return FONT_AWESOME_SIGNAL_OFF;
}

std::string Ml307Board::GetBoardJson() {
    // Set the board type for OTA
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    board_json += "\"revision\":\"" + modem_.GetModuleName() + "\",";
    board_json += "\"carrier\":\"" + modem_.GetCarrierName() + "\",";
    board_json += "\"csq\":\"" + std::to_string(modem_.GetCsq()) + "\",";
    board_json += "\"imei\":\"" + modem_.GetImei() + "\",";
    board_json += "\"iccid\":\"" + modem_.GetIccid() + "\"}";
    return board_json;
}

void Ml307Board::SetPowerSaveMode(bool enabled) {
    // TODO: Implement power save mode for ML307
}
