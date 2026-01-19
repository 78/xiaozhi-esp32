#include "nt26_board.h"
#include "display.h"
#include "application.h"
#include "audio_codec.h"
#include <esp_log.h>
#include <font_awesome.h>
#include <cJSON.h>

#define TAG "Nt26Board"

Nt26Board::Nt26Board(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t dtr_pin, gpio_num_t ri_pin, gpio_num_t reset_pin)
    : tx_pin_(tx_pin), rx_pin_(rx_pin), dtr_pin_(dtr_pin), ri_pin_(ri_pin), reset_pin_(reset_pin) {

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    esp_event_loop_create_default();
    esp_netif_init();
    
    // Create PM lock handle
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "nt26_cpu", &pm_lock_cpu_max_);
    
    // Create network ready timeout timer
    esp_timer_create_args_t timer_args = {
        .callback = OnNetworkReadyTimeout,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "nt26_net_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&timer_args, &network_ready_timer_);
}

Nt26Board::~Nt26Board() {
    if (current_power_level_ != PowerSaveLevel::LOW_POWER) {
        SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);
    }
    
    if (network_ready_timer_) {
        esp_timer_stop(network_ready_timer_);
        esp_timer_delete(network_ready_timer_);
    }

    if (modem_) {
        modem_->Stop();
    }
    
    if (pm_lock_cpu_max_) {
        esp_pm_lock_delete(pm_lock_cpu_max_);
    }
}

std::string Nt26Board::GetBoardType() {
    return "nt26";
}

void Nt26Board::OnNetworkEvent(NetworkEvent event, const std::string& data) {
    if (network_event_callback_) {
        network_event_callback_(event, data);
    }
}

void Nt26Board::OnNetworkReadyTimeout(void* arg) {
    auto* self = static_cast<Nt26Board*>(arg);
    ESP_LOGW(TAG, "Network ready timeout");
    self->OnNetworkEvent(NetworkEvent::ModemErrorTimeout, "网络连接超时");
}

void Nt26Board::StartNetwork() {
    OnNetworkEvent(NetworkEvent::ModemDetecting);

    UartEthModem::Config config = {
        .uart_num = UART_NUM_1,
        .baud_rate = 3000000,
        .tx_pin = tx_pin_,
        .rx_pin = rx_pin_,
        .mrdy_pin = dtr_pin_,
        .srdy_pin = ri_pin_
    };
    
    modem_ = std::make_unique<UartEthModem>(config);
    modem_->SetDebug(false);
    
    modem_->SetNetworkEventCallback([this](UartEthModem::UartEthModemEvent event) {
        switch (event) {
            case UartEthModem::UartEthModemEvent::Connected:
                esp_timer_stop(network_ready_timer_);
                OnNetworkEvent(NetworkEvent::Connected);
                break;
            case UartEthModem::UartEthModemEvent::Disconnected:
                OnNetworkEvent(NetworkEvent::Disconnected);
                break;
            case UartEthModem::UartEthModemEvent::ErrorNoSim:
                esp_timer_stop(network_ready_timer_);
                ScheduleAsyncStop();
                OnNetworkEvent(NetworkEvent::ModemErrorNoSim);
                break;
            case UartEthModem::UartEthModemEvent::ErrorRegistrationDenied:
                esp_timer_stop(network_ready_timer_);
                ScheduleAsyncStop();
                OnNetworkEvent(NetworkEvent::ModemErrorRegDenied);
                break;
            case UartEthModem::UartEthModemEvent::Connecting:
                OnNetworkEvent(NetworkEvent::Connecting);
                break;
            case UartEthModem::UartEthModemEvent::ErrorInitFailed:
            case UartEthModem::UartEthModemEvent::ErrorNoCarrier:
                esp_timer_stop(network_ready_timer_);
                ScheduleAsyncStop();
                OnNetworkEvent(NetworkEvent::ModemErrorInitFailed);
                break;
        }
    });

    if (modem_->Start() != ESP_OK) {
        OnNetworkEvent(NetworkEvent::ModemErrorInitFailed);
        return;
    }

    esp_timer_start_once(network_ready_timer_, 30000 * 1000ULL);
    OnNetworkEvent(NetworkEvent::Connecting);
}

void Nt26Board::ScheduleAsyncStop() {
    Application::GetInstance().Schedule([this]() {
        if (modem_) {
            modem_->Stop();
        }
    });
}

void Nt26Board::SetNetworkEventCallback(NetworkEventCallback callback) {
    network_event_callback_ = std::move(callback);
}

NetworkInterface* Nt26Board::GetNetwork() {
    static EspNetwork network;
    return &network;
}

const char* Nt26Board::GetNetworkStateIcon() {
    if (modem_ == nullptr || !modem_->IsInitialized()) {
        return FONT_AWESOME_SIGNAL_OFF;
    }
    int csq = modem_->GetSignalStrength();
    if (csq == 99 || csq == -1) {
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
    return FONT_AWESOME_SIGNAL_OFF;
}

void Nt26Board::SetPowerSaveLevel(PowerSaveLevel level) {
    if (level == current_power_level_) return;
    
    if (current_power_level_ == PowerSaveLevel::BALANCED ||
        current_power_level_ == PowerSaveLevel::PERFORMANCE) {
        if (pm_lock_cpu_max_) {
            esp_pm_lock_release(pm_lock_cpu_max_);
        }
    }
    
    if (level == PowerSaveLevel::BALANCED || level == PowerSaveLevel::PERFORMANCE) {
        if (pm_lock_cpu_max_) {
            esp_pm_lock_acquire(pm_lock_cpu_max_);
        }
    }
    
    current_power_level_ = level;
}

std::string Nt26Board::GetBoardJson() {
    // Set the board type for OTA
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    if (modem_) {
        board_json += "\"revision\":\"" + modem_->GetModuleRevision() + "\",";
        board_json += "\"carrier\":\"" + modem_->GetCarrierName() + "\",";
        board_json += "\"csq\":\"" + std::to_string(modem_->GetSignalStrength()) + "\",";
        board_json += "\"imei\":\"" + modem_->GetImei() + "\",";
        board_json += "\"iccid\":\"" + modem_->GetIccid() + "\",";
        board_json += "\"cereg\":" + GetRegistrationState().ToString() + "}";
    } else {
        board_json += "\"status\":\"offline\"}";
    }
    return board_json;
}

Nt26CeregState Nt26Board::GetRegistrationState() {
    Nt26CeregState state;
    if (modem_) {
        auto cell_info = modem_->GetCellInfo();
        state.stat = cell_info.stat;
        state.tac = cell_info.tac;
        state.ci = cell_info.ci;
        state.AcT = cell_info.act;
    }
    return state;
}

std::string Nt26Board::GetDeviceStatusJson() {
    auto& board = Board::GetInstance();
    auto root = cJSON_CreateObject();

    // Audio speaker
    auto audio_speaker = cJSON_CreateObject();
    auto audio_codec = board.GetAudioCodec();
    if (audio_codec) {
        cJSON_AddNumberToObject(audio_speaker, "volume", audio_codec->output_volume());
    }
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

    // Screen
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
    bool charging = false, discharging = false;
    if (board.GetBatteryLevel(battery_level, charging, discharging)) {
        auto battery = cJSON_CreateObject();
        cJSON_AddNumberToObject(battery, "level", battery_level);
        cJSON_AddBoolToObject(battery, "charging", charging);
        cJSON_AddItemToObject(root, "battery", battery);
    }

    // Network
    auto network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "type", "cellular");
    if (modem_) {
        cJSON_AddStringToObject(network, "carrier", modem_->GetCarrierName().c_str());
        int csq = modem_->GetSignalStrength();
        if (csq == 99 || csq == -1) {
            cJSON_AddStringToObject(network, "signal", "unknown");
        } else if (csq >= 0 && csq <= 14) {
            cJSON_AddStringToObject(network, "signal", "weak");
        } else if (csq >= 15 && csq <= 24) {
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
