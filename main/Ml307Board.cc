#include "Ml307Board.h"
#include "Application.h"

#include <esp_log.h>
#include <Ml307Http.h>
#include <Ml307SslTransport.h>
#include <WebSocket.h>
#include <esp_timer.h>

static const char *TAG = "Ml307Board";

static std::string csq_to_string(int csq) {
    if (csq == -1) {
        return "No network";
    } else if (csq >= 0 && csq <= 9) {
        return "Very bad";
    } else if (csq >= 10 && csq <= 14) {
        return "Bad";
    } else if (csq >= 15 && csq <= 19) {
        return "Fair";
    } else if (csq >= 20 && csq <= 24) {
        return "Good";
    } else if (csq >= 25 && csq <= 31) {
        return "Very good";
    }
    return "Invalid";
}


Ml307Board::Ml307Board() : modem_(ML307_TX_PIN, ML307_RX_PIN, 4096) {
}

void Ml307Board::StartNetwork() {
    auto& application = Application::GetInstance();
    auto& display = application.GetDisplay();
    display.SetText(std::string("Wait for network\n"));
    int result = modem_.WaitForNetworkReady();
    if (result == -1) {
        application.Alert("Error", "PIN is not ready");
    } else if (result == -2) {
        application.Alert("Error", "Registration denied");
    }

    // Print the ML307 modem information
    std::string module_name = modem_.GetModuleName();
    std::string imei = modem_.GetImei();
    std::string iccid = modem_.GetIccid();
    ESP_LOGI(TAG, "ML307 Module: %s", module_name.c_str());
    ESP_LOGI(TAG, "ML307 IMEI: %s", imei.c_str());
    ESP_LOGI(TAG, "ML307 ICCID: %s", iccid.c_str());
}

void Ml307Board::StartModem() {
    auto& display = Application::GetInstance().GetDisplay();
    display.SetText(std::string("Starting modem"));
    modem_.SetDebug(false);
    modem_.SetBaudRate(921600);

    auto& application = Application::GetInstance();
    // If low power, the material ready event will be triggered by the modem because of a reset
    modem_.OnMaterialReady([this, &application]() {
        ESP_LOGI(TAG, "ML307 material ready");
        application.Schedule([this, &application]() {
            application.SetChatState(kChatStateIdle);
            StartNetwork();
        });
    });
}

void Ml307Board::Initialize() {
    ESP_LOGI(TAG, "Initializing Ml307Board");
    StartModem();
}

AudioDevice* Ml307Board::CreateAudioDevice() {
    return new AudioDevice();
}

Http* Ml307Board::CreateHttp() {
    return new Ml307Http(modem_);
}

WebSocket* Ml307Board::CreateWebSocket() {
    return new WebSocket(new Ml307SslTransport(modem_, 0));
}

bool Ml307Board::GetNetworkState(std::string& network_name, int& signal_quality, std::string& signal_quality_text) {
    if (!modem_.network_ready()) {
        return false;
    }
    network_name = modem_.GetCarrierName();
    signal_quality = modem_.GetCsq();
    signal_quality_text = csq_to_string(signal_quality);
    return signal_quality != -1;
}

std::string Ml307Board::GetBoardJson() {
    // Set the board type for OTA
    std::string board_type = BOARD_TYPE;
    std::string module_name = modem_.GetModuleName();
    std::string carrier_name = modem_.GetCarrierName();
    std::string imei = modem_.GetImei();
    std::string iccid = modem_.GetIccid();
    int csq = modem_.GetCsq();
    std::string board_json = std::string("{\"type\":\"" + board_type + "\",");
    board_json += "\"revision\":\"" + module_name + "\",";
    board_json += "\"carrier\":\"" + carrier_name + "\",";
    board_json += "\"csq\":\"" + std::to_string(csq) + "\",";
    board_json += "\"imei\":\"" + imei + "\",";
    board_json += "\"iccid\":\"" + iccid + "\"}";
    return board_json;
}
