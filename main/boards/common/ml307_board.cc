#include "ml307_board.h"

#include "audio_codec.h"
#include "display.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <material_symbols.h>
#include <algorithm>
#include <utility>

static const char *TAG = "Ml307Board";

// Maximum retry count for modem detection
static constexpr int MODEM_DETECT_TIMEOUT_MS = 15'000;
static constexpr int NETWORK_REG_TIMEOUT_MS = 60'000;
static constexpr int MODEM_OPERATION_SLICE_MS = 3'000;
static constexpr EventBits_t NETWORK_TASK_STOPPED = BIT0;

Ml307Board::Ml307Board(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t dtr_pin) : tx_pin_(tx_pin), rx_pin_(rx_pin), dtr_pin_(dtr_pin) {
    lifecycle_events_ = xEventGroupCreate();
}

Ml307Board::~Ml307Board() {
    StopNetwork();
    if (lifecycle_events_ != nullptr) {
        vEventGroupDelete(lifecycle_events_);
    }
}

std::string Ml307Board::GetBoardType() {
    return "ml307";
}

void Ml307Board::SetNetworkEventCallback(NetworkEventCallback callback) {
    network_event_callback_ = std::move(callback);
}

void Ml307Board::OnNetworkEvent(NetworkEvent event, const std::string& data) {
    switch (event) {
        case NetworkEvent::ModemDetecting:
            ESP_LOGI(TAG, "Detecting modem...");
            break;
        case NetworkEvent::Connecting:
            ESP_LOGI(TAG, "Registering network...");
            break;
        case NetworkEvent::Connected:
            ESP_LOGI(TAG, "Network connected");
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
            ESP_LOGE(TAG, "Modem initialization failed");
            break;
        case NetworkEvent::ModemErrorTimeout:
            ESP_LOGE(TAG, "Operation timeout");
            break;
        default:
            break;
    }

    // Notify external callback if set
    if (network_event_callback_) {
        network_event_callback_(event, data);
    }
}

void Ml307Board::NetworkTask() {
    // Notify modem detection started
    OnNetworkEvent(NetworkEvent::ModemDetecting);

    if (modem_ == nullptr) {
        const int64_t detect_deadline =
            esp_timer_get_time() + MODEM_DETECT_TIMEOUT_MS * 1000LL;
        int detect_attempt = 0;
        while (!cancel_requested_ && esp_timer_get_time() < detect_deadline) {
            const int target_baud = (detect_attempt++ % 2 == 0) ? 921600 : 115200;
            const int remaining_ms =
                static_cast<int>((detect_deadline - esp_timer_get_time()) / 1000);
            modem_ = AtModem::Detect(tx_pin_, rx_pin_, dtr_pin_, target_baud,
                                     std::min(remaining_ms, MODEM_OPERATION_SLICE_MS));
            if (modem_ != nullptr) {
                break;
            }
            if (!cancel_requested_) {
                vTaskDelay(pdMS_TO_TICKS(250));
            }
        }
    } else {
        ESP_LOGI(TAG, "Reusing detected modem to check for a newly inserted SIM");
    }

    if (cancel_requested_) {
        modem_.reset();
        return;
    }
    if (modem_ == nullptr) {
        ESP_LOGE(TAG, "Failed to detect modem within %d ms", MODEM_DETECT_TIMEOUT_MS);
        OnNetworkEvent(NetworkEvent::ModemErrorTimeout);
        return;
    }

    ESP_LOGI(TAG, "Modem detected successfully");

    // Set up network state change callback
    // Note: Don't call GetCarrierName() here as it sends AT command and will block ReceiveTask
    modem_->OnNetworkStateChanged([this](bool network_ready) {
        if (cancel_requested_) {
            return;
        }
        if (network_ready) {
            OnNetworkEvent(NetworkEvent::Connected);
        } else {
            OnNetworkEvent(NetworkEvent::Disconnected);
        }
    });

    // Notify network registration started
    OnNetworkEvent(NetworkEvent::Connecting);

    const int64_t registration_deadline = esp_timer_get_time() + NETWORK_REG_TIMEOUT_MS * 1000LL;
    while (!cancel_requested_ && esp_timer_get_time() < registration_deadline) {
        const int remaining_ms = static_cast<int>((registration_deadline - esp_timer_get_time()) / 1000);
        auto result = modem_->WaitForNetworkReady(std::min(remaining_ms, 5'000));
        if (result == NetworkStatus::Ready) {
            break;
        } else if (result == NetworkStatus::ErrorInsertPin) {
            OnNetworkEvent(NetworkEvent::ModemErrorNoSim);
            return;
        } else if (result == NetworkStatus::ErrorRegistrationDenied) {
            OnNetworkEvent(NetworkEvent::ModemErrorRegDenied);
            return;
        }
        if (!cancel_requested_) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    if (cancel_requested_) {
        modem_->OnNetworkStateChanged({});
        modem_.reset();
        return;
    }
    if (!modem_->network_ready()) {
        ESP_LOGE(TAG, "Failed to register network within %d ms", NETWORK_REG_TIMEOUT_MS);
        OnNetworkEvent(NetworkEvent::ModemErrorTimeout);
        return;
    }

    // Module revision is useful for diagnostics. Subscriber identifiers are intentionally
    // not written to ordinary logs.
    std::string module_revision = modem_->GetModuleRevision();
    cached_csq_ = modem_->GetCsq();
    ESP_LOGI(TAG, "ML307 Revision: %s", module_revision.c_str());
}

void Ml307Board::StartNetwork() {
    if (modem_ != nullptr && modem_->network_ready()) {
        ESP_LOGI(TAG, "Cellular network is already started");
        return;
    }
    bool expected = false;
    if (!task_running_.compare_exchange_strong(expected, true)) {
        ESP_LOGI(TAG, "Cellular network task is already running");
        return;
    }
    cancel_requested_ = false;
    cached_csq_ = -1;
    xEventGroupClearBits(lifecycle_events_, NETWORK_TASK_STOPPED);
    BaseType_t created = xTaskCreate([](void* arg) {
        Ml307Board* board = static_cast<Ml307Board*>(arg);
        board->NetworkTask();
        xEventGroupSetBits(board->lifecycle_events_, NETWORK_TASK_STOPPED);
        board->network_task_handle_ = nullptr;
        board->task_running_ = false;
        vTaskDelete(NULL);
    }, "ml307_net", 4096, this, 5, &network_task_handle_);
    if (created != pdPASS) {
        network_task_handle_ = nullptr;
        task_running_ = false;
        OnNetworkEvent(NetworkEvent::ModemErrorInitFailed);
    }
}

bool Ml307Board::StopNetwork() {
    cancel_requested_ = true;
    if (task_running_) {
        const auto bits = xEventGroupWaitBits(lifecycle_events_, NETWORK_TASK_STOPPED, pdTRUE,
                                              pdFALSE, pdMS_TO_TICKS(6'000));
        if ((bits & NETWORK_TASK_STOPPED) == 0) {
            ESP_LOGE(TAG, "Timed out waiting for cellular task cancellation");
            return false;
        }
    }
    if (modem_ != nullptr) {
        modem_->OnNetworkStateChanged({});
        modem_.reset();
    }
    cached_csq_ = -1;
    return true;
}

NetworkInterface* Ml307Board::GetNetwork() {
    return modem_.get();
}

const char* Ml307Board::GetNetworkStateIcon() {
    if (modem_ == nullptr || !modem_->network_ready()) {
        return MATERIAL_SYMBOLS_ANDROID_CELL_4_BAR_OFF;
    }
    int csq = modem_->GetCsq();
    cached_csq_ = csq;
    if (csq == -1) {
        return MATERIAL_SYMBOLS_ANDROID_CELL_4_BAR_OFF;
    } else if (csq >= 0 && csq <= 9) {
        return MATERIAL_SYMBOLS_SIGNAL_CELLULAR_ALT_1_BAR;
    } else if (csq >= 10 && csq <= 14) {
        return MATERIAL_SYMBOLS_SIGNAL_CELLULAR_ALT_2_BAR;
    } else if (csq >= 15 && csq <= 19) {
        return MATERIAL_SYMBOLS_SIGNAL_CELLULAR_ALT;
    } else if (csq >= 20 && csq <= 31) {
        return MATERIAL_SYMBOLS_ANDROID_CELL_4_BAR;
    }

    ESP_LOGW(TAG, "Invalid CSQ: %d", csq);
    return MATERIAL_SYMBOLS_ANDROID_CELL_4_BAR_OFF;
}

std::string Ml307Board::GetBoardJson() {
    if (modem_ == nullptr) {
        return std::string("{\"type\":\"" BOARD_TYPE "\",\"name\":\"" BOARD_NAME
                           "\",\"manufacturer\":\"" BOARD_MANUFACTURER "\"}");
    }
    // Set the board type for OTA
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    board_json += "\"manufacturer\":\"" BOARD_MANUFACTURER "\",";
    board_json += "\"revision\":\"" + modem_->GetModuleRevision() + "\",";
    board_json += "\"carrier\":\"" + modem_->GetCarrierName() + "\",";
    board_json += "\"csq\":\"" + std::to_string(modem_->GetCsq()) + "\",";
    board_json += "\"imei\":\"" + modem_->GetImei() + "\",";
    board_json += "\"iccid\":\"" + modem_->GetIccid() + "\",";
    board_json += "\"cereg\":" + modem_->GetRegistrationState().ToString() + "}";
    return board_json;
}

void Ml307Board::SetPowerSaveLevel(PowerSaveLevel level) {
    // TODO: Implement power save level for ML307
    (void)level;
}

std::string Ml307Board::GetDeviceStatusJson() {
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
     *         "type": "cellular",
     *         "carrier": "CHINA MOBILE",
     *         "csq": 10
     *     }
     * }
     */
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
    if (display && display->height() > 64) { // For LCD display only
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
    cJSON_AddStringToObject(network, "carrier", modem_->GetCarrierName().c_str());
    int csq = modem_->GetCsq();
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
    cJSON_AddItemToObject(root, "network", network);

    auto json_str = cJSON_PrintUnformatted(root);
    std::string json(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return json;
}
