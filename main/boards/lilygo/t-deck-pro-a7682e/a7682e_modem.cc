#include "a7682e_modem.h"

#include "config.h"

#include <algorithm>
#include <cstring>

#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "A7682eModem";

namespace {

constexpr uart_port_t kA7682eUart = UART_NUM_1;
constexpr uint32_t kUartRxBufferSize = 2048;
constexpr uint32_t kUartTxBufferSize = 2048;
constexpr uint32_t kModemReadyAttempts = 10;
constexpr uint32_t kModemReadyRetryMs = 500;
constexpr size_t kAtResponseBufferBytes = 512;
constexpr int kTtsStatusQueryTimeoutMs = 300;
constexpr int64_t kTtsStatusPollIntervalUs = 100 * 1000;
constexpr int64_t kQueueOverflowLogIntervalUs = 5 * 1000 * 1000;
constexpr EventBits_t kInitDoneBit = 1 << 0;
constexpr EventBits_t kReadyBit = 1 << 1;

bool HasResponseToken(const std::string& response, std::string_view token) {
    size_t offset = response.find(token.data(), 0, token.size());
    while (offset != std::string::npos) {
        const bool line_start = offset == 0 || response[offset - 1] == '\n';
        const size_t end = offset + token.size();
        const bool line_end =
            end == response.size() || response[end] == '\r' || response[end] == '\n';
        if (line_start && line_end) {
            return true;
        }
        offset = response.find(token.data(), offset + 1, token.size());
    }
    return false;
}

}  // namespace

A7682eModem::A7682eModem() = default;

A7682eModem::~A7682eModem() {
    if (worker_task_ != nullptr) {
        vTaskDelete(worker_task_);
        worker_task_ = nullptr;
    }
    if (command_queue_ != nullptr) {
        vQueueDelete(command_queue_);
        command_queue_ = nullptr;
    }
    if (init_event_ != nullptr) {
        vEventGroupDelete(init_event_);
        init_event_ = nullptr;
    }
    if (uart_driver_owned_) {
        uart_driver_delete(kA7682eUart);
        uart_driver_owned_ = false;
    }
}

bool A7682eModem::ConfigureHardware() {
    uart_config_t uart_config = {};
    uart_config.baud_rate = 115200;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 0;

    esp_err_t ret = uart_param_config(kA7682eUart, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART config failed: %s", esp_err_to_name(ret));
        return false;
    }
    ret = uart_set_pin(kA7682eUart, T_DECK_PRO_A7682E_UART_TX, T_DECK_PRO_A7682E_UART_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART pin config failed: %s", esp_err_to_name(ret));
        return false;
    }
    ret = uart_driver_install(kA7682eUart, kUartRxBufferSize, kUartTxBufferSize, 0, nullptr, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(ret));
        return false;
    }
    uart_ready_ = true;
    uart_driver_owned_ = ret == ESP_OK;

    gpio_config_t control_config = {};
    control_config.pin_bit_mask =
        (1ULL << T_DECK_PRO_A7682E_POWER_EN) | (1ULL << T_DECK_PRO_A7682E_PWRKEY) |
        (1ULL << T_DECK_PRO_A7682E_RESET) | (1ULL << T_DECK_PRO_A7682E_ITR);
    control_config.mode = GPIO_MODE_OUTPUT;
    control_config.pull_up_en = GPIO_PULLUP_DISABLE;
    control_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    control_config.intr_type = GPIO_INTR_DISABLE;
    ret = gpio_config(&control_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Modem control GPIO config failed: %s", esp_err_to_name(ret));
        if (uart_driver_owned_) {
            uart_driver_delete(kA7682eUart);
            uart_driver_owned_ = false;
            uart_ready_ = false;
        }
        return false;
    }

    gpio_set_level(T_DECK_PRO_A7682E_POWER_EN, 1);
    gpio_set_level(T_DECK_PRO_A7682E_PWRKEY, 1);
    gpio_hold_dis(T_DECK_PRO_A7682E_RESET);
    gpio_set_level(T_DECK_PRO_A7682E_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Match the working A7682E audio example: reset the module before the
    // short PWRKEY power-on pulse, then hold ITR/DTR low to wake the UART.
    gpio_set_level(T_DECK_PRO_A7682E_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(2500));
    gpio_set_level(T_DECK_PRO_A7682E_RESET, 1);
    gpio_set_level(T_DECK_PRO_A7682E_PWRKEY, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(T_DECK_PRO_A7682E_PWRKEY, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(T_DECK_PRO_A7682E_PWRKEY, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(T_DECK_PRO_A7682E_ITR, 0);
    return true;
}

bool A7682eModem::Begin() {
    if (started_) {
        return IsReady();
    }
    if (!ConfigureHardware()) {
        return false;
    }

    command_queue_ = xQueueCreate(a7682e::kTtsQueueCapacity, sizeof(Command));
    init_event_ = xEventGroupCreate();
    if (command_queue_ == nullptr || init_event_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create A7682E worker resources");
        if (command_queue_ != nullptr) {
            vQueueDelete(command_queue_);
            command_queue_ = nullptr;
        }
        if (init_event_ != nullptr) {
            vEventGroupDelete(init_event_);
            init_event_ = nullptr;
        }
        return false;
    }

    started_ = true;
    const BaseType_t task_result =
        xTaskCreate(&A7682eModem::WorkerEntry, "a7682_at", 4096, this, 5, &worker_task_);
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create AT worker task");
        started_ = false;
        vQueueDelete(command_queue_);
        command_queue_ = nullptr;
        vEventGroupDelete(init_event_);
        init_event_ = nullptr;
        return false;
    }

    const EventBits_t bits =
        xEventGroupWaitBits(init_event_, kInitDoneBit, pdFALSE, pdTRUE, pdMS_TO_TICKS(30000));
    if ((bits & kReadyBit) == 0) {
        ESP_LOGW(TAG, "A7682E worker started but modem is not ready");
        return false;
    }
    ESP_LOGI(TAG, "A7682E UART worker started");
    return true;
}

bool A7682eModem::Enqueue(const Command& command, bool front) {
    std::lock_guard<std::mutex> lock(command_mutex_);
    return EnqueueUnlocked(command, front);
}

bool A7682eModem::EnqueueUnlocked(const Command& command, bool front) {
    if (command_queue_ == nullptr) {
        return false;
    }
    const BaseType_t result = front ? xQueueSendToFront(command_queue_, &command, 0)
                                    : xQueueSendToBack(command_queue_, &command, 0);
    return result == pdPASS;
}

void A7682eModem::MaybeLogQueueOverflow() {
    const int64_t now_us = esp_timer_get_time();
    if (last_queue_overflow_log_us_ == 0 ||
        now_us - last_queue_overflow_log_us_ >= kQueueOverflowLogIntervalUs) {
        ESP_LOGW(TAG, "A7682E TTS queue full; dropped oldest pending sentence");
        last_queue_overflow_log_us_ = now_us;
    }
}

bool A7682eModem::SpeakText(std::string_view utf8_text) {
    const std::vector<std::string> chunks = a7682e::SplitTtsText(utf8_text);
    if (chunks.empty()) {
        return false;
    }
    if (command_queue_ == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(command_mutex_);
    bool queued = false;
    for (const auto& chunk : chunks) {
        if (chunk.size() > a7682e::kTtsMaxTextBytes) {
            continue;
        }

        Command command;
        command.type = CommandType::Speak;
        std::memcpy(command.text, chunk.data(), chunk.size());
        command.text[chunk.size()] = '\0';
        outstanding_speech_count_.fetch_add(1, std::memory_order_release);
        if (EnqueueUnlocked(command)) {
            queued = true;
            continue;
        }
        outstanding_speech_count_.fetch_sub(1, std::memory_order_acq_rel);

        // Keep the callback non-blocking: discard the oldest pending sentence on overflow.
        Command dropped;
        if (xQueueReceive(command_queue_, &dropped, 0) != pdPASS) {
            MaybeLogQueueOverflow();
            continue;
        }
        if (dropped.type != CommandType::Speak) {
            EnqueueUnlocked(dropped, true);
            continue;
        }
        if (EnqueueUnlocked(command)) {
            queued = true;
            MaybeLogQueueOverflow();
        } else {
            const uint32_t outstanding = outstanding_speech_count_.load(std::memory_order_relaxed);
            if (outstanding > 0) {
                outstanding_speech_count_.store(outstanding - 1, std::memory_order_release);
            }
            MaybeLogQueueOverflow();
        }
    }
    return queued;
}

bool A7682eModem::IsSpeechBusy() const {
    return outstanding_speech_count_.load(std::memory_order_acquire) > 0;
}

void A7682eModem::StopSpeech() {
    std::lock_guard<std::mutex> lock(command_mutex_);
    if (command_queue_ == nullptr) {
        return;
    }
    stop_requested_.store(true, std::memory_order_release);
    xQueueReset(command_queue_);
    const uint32_t active_count = speech_active_.load(std::memory_order_acquire) ? 1 : 0;
    outstanding_speech_count_.store(active_count, std::memory_order_release);
    Command command;
    command.type = CommandType::Stop;
    if (!EnqueueUnlocked(command, true)) {
        ESP_LOGW(TAG, "Failed to enqueue CTTS stop command");
    }
}

bool A7682eModem::SetOutputGain(int gain) {
    gain = std::clamp(gain, 0, 100);
    volume_.store(gain, std::memory_order_relaxed);
    pending_gain_.store(gain, std::memory_order_release);
    return command_queue_ != nullptr && worker_task_ != nullptr;
}

bool A7682eModem::SendCommand(std::string_view command, std::string& response, int timeout_ms,
                              bool flush_input) {
    response.clear();
    if (!uart_ready_ || command.empty() || timeout_ms <= 0) {
        return false;
    }

    if (flush_input) {
        uart_flush_input(kA7682eUart);
    }
    ESP_LOGI(TAG, "AT >> %.*s", static_cast<int>(command.size()), command.data());
    uart_write_bytes(kA7682eUart, command.data(), command.size());
    uart_write_bytes(kA7682eUart, "\r\n", 2);
    uart_wait_tx_done(kA7682eUart, pdMS_TO_TICKS(1000));

    bool accepted = false;
    bool rejected = false;
    const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(timeout_ms) * 1000;
    uint8_t buffer[64];

    while (esp_timer_get_time() < deadline) {
        const int bytes = uart_read_bytes(kA7682eUart, buffer, sizeof(buffer), pdMS_TO_TICKS(50));
        if (bytes <= 0) {
            continue;
        }
        if (response.size() < kAtResponseBufferBytes) {
            const size_t remaining = kAtResponseBufferBytes - response.size();
            response.append(reinterpret_cast<const char*>(buffer),
                            std::min(remaining, static_cast<size_t>(bytes)));
        }
        accepted = accepted || HasResponseToken(response, "OK");
        rejected = rejected || HasResponseToken(response, "ERROR");
        if (accepted || rejected) {
            break;
        }
    }
    return accepted && !rejected;
}

bool A7682eModem::InitializeModem() {
    std::string response;
    bool answered = SendCommand("AT", response, 1500);
    if (!answered) {
        ESP_LOGW(TAG, "A7682E did not answer initial AT probe; pulsing PWRKEY");
        // A7682E powers on with a short low-high-low pulse on PWRKEY.
        gpio_set_level(T_DECK_PRO_A7682E_PWRKEY, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(T_DECK_PRO_A7682E_PWRKEY, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(T_DECK_PRO_A7682E_PWRKEY, 0);
        vTaskDelay(pdMS_TO_TICKS(10));

        for (uint32_t attempt = 0; attempt < kModemReadyAttempts; ++attempt) {
            if (SendCommand("AT", response, 1500)) {
                answered = true;
                break;
            }
            if (attempt + 1 != kModemReadyAttempts) {
                vTaskDelay(pdMS_TO_TICKS(kModemReadyRetryMs));
            }
        }
    }
    if (!answered) {
        ESP_LOGE(TAG, "A7682E did not answer AT");
        return false;
    }
    if (!SendCommand("AT+CTTSPARAM=1,3,0,1,1", response, 2500)) {
        ESP_LOGE(TAG, "AT+CTTSPARAM failed");
        return false;
    }
    if (!SendCommand("AT+VMUTE=0", response, 1500)) {
        ESP_LOGW(TAG, "AT+VMUTE=0 failed");
    }

    const std::string gain_command =
        "AT+COUTGAIN=" +
        std::to_string(a7682e::VolumeToModemGain(volume_.load(std::memory_order_relaxed)));
    if (!SendCommand(gain_command, response, 1500)) {
        ESP_LOGW(TAG, "Initial output gain command failed");
    }
    return true;
}

bool A7682eModem::EnsureReady() {
    if (modem_ready_.load(std::memory_order_acquire)) {
        return true;
    }
    const bool ready = InitializeModem();
    modem_ready_.store(ready, std::memory_order_release);
    return ready;
}

bool A7682eModem::EncodeAndSendTts(std::string_view utf8_text) {
    std::string command;
    int mode = 0;
    if (!a7682e::BuildTtsCommand(utf8_text, command, &mode)) {
        return false;
    }

    ESP_LOGI(TAG, "Sending CTTS mode %d text (%u bytes)", mode,
             static_cast<unsigned>(utf8_text.size()));
    std::string response;
    if (!SendCommand(command, response, 5000)) {
        return false;
    }
    return WaitForTtsCompletion(utf8_text, response);
}

bool A7682eModem::WaitForTtsCompletion(std::string_view utf8_text,
                                       const std::string& initial_response) {
    if (HasResponseToken(initial_response, "+CTTS:0")) {
        return true;
    }

    std::string response = initial_response;
    const uint32_t estimated_duration_ms = a7682e::EstimateTtsDurationMs(utf8_text);
    const int64_t completion_deadline =
        esp_timer_get_time() + static_cast<int64_t>(estimated_duration_ms) * 1000;
    int64_t next_status_query = 0;
    bool status_query_available = true;
    uint8_t buffer[64];
    while (esp_timer_get_time() < completion_deadline) {
        if (stop_requested_.exchange(false, std::memory_order_acq_rel)) {
            // StopSpeech() already removed pending commands while holding the
            // queue mutex. Keep commands added after that stop marker so a new
            // sentence cannot be lost while the current CTTS playback stops.
            std::string stop_response;
            return SendCommand("AT+CTTS=0", stop_response, 1500);
        }

        const int pending_gain = pending_gain_.exchange(-1, std::memory_order_acq_rel);
        if (pending_gain >= 0) {
            if (modem_ready_.load(std::memory_order_acquire)) {
                const std::string gain_command =
                    "AT+COUTGAIN=" + std::to_string(a7682e::VolumeToModemGain(pending_gain));
                std::string gain_response;
                const bool gain_ok = SendCommand(gain_command, gain_response, 1500, false);
                if (!gain_ok) {
                    modem_ready_.store(false, std::memory_order_release);
                    pending_gain_.store(pending_gain, std::memory_order_release);
                    ESP_LOGW(TAG, "Failed to set A7682E output gain during TTS");
                }
                if (HasResponseToken(gain_response, "+CTTS:0")) {
                    return true;
                }
            } else {
                pending_gain_.store(pending_gain, std::memory_order_release);
            }
        }

        const int bytes = uart_read_bytes(kA7682eUart, buffer, sizeof(buffer), pdMS_TO_TICKS(20));
        if (bytes <= 0) {
            continue;
        }
        if (response.size() < kAtResponseBufferBytes) {
            const size_t remaining = kAtResponseBufferBytes - response.size();
            response.append(reinterpret_cast<const char*>(buffer),
                            std::min(remaining, static_cast<size_t>(bytes)));
        }
        if (HasResponseToken(response, "+CTTS:0")) {
            return true;
        }
        if (HasResponseToken(response, "ERROR")) {
            return false;
        }

        // Prefer the module's status over a timing guess. The A7682E reports
        // +CTTS:0 when the current conversion/playback is complete.
        const int64_t now_us = esp_timer_get_time();
        if (status_query_available && now_us >= next_status_query) {
            std::string status_response;
            const bool status_ok =
                SendCommand("AT+CTTS?", status_response, kTtsStatusQueryTimeoutMs, false);
            if (HasResponseToken(status_response, "+CTTS:0")) {
                return true;
            }
            if (!status_ok) {
                status_query_available = false;
                ESP_LOGW(TAG, "A7682E TTS status query unavailable; using short fallback timing");
            } else {
                next_status_query = now_us + kTtsStatusPollIntervalUs;
            }
        }
    }
    ESP_LOGW(TAG, "A7682E did not emit +CTTS:0; releasing TTS worker after estimated %u ms",
             static_cast<unsigned>(estimated_duration_ms));
    return true;
}

void A7682eModem::CompleteSpeechCommand() {
    std::lock_guard<std::mutex> lock(command_mutex_);
    speech_active_.store(false, std::memory_order_release);
    const uint32_t outstanding = outstanding_speech_count_.load(std::memory_order_relaxed);
    if (outstanding > 0) {
        outstanding_speech_count_.store(outstanding - 1, std::memory_order_release);
    }
}

void A7682eModem::WorkerEntry(void* arg) {
    static_cast<A7682eModem*>(arg)->WorkerTask();
    vTaskDelete(nullptr);
}

void A7682eModem::WorkerTask() {
    const bool ready = EnsureReady();
    modem_ready_.store(ready, std::memory_order_release);
    if (init_event_ != nullptr) {
        xEventGroupSetBits(init_event_, kInitDoneBit | (ready ? kReadyBit : 0));
    }

    Command command;
    while (true) {
        const int pending_gain = pending_gain_.exchange(-1, std::memory_order_acq_rel);
        if (pending_gain >= 0) {
            if (EnsureReady()) {
                const std::string gain_command =
                    "AT+COUTGAIN=" + std::to_string(a7682e::VolumeToModemGain(pending_gain));
                std::string response;
                if (!SendCommand(gain_command, response, 1500)) {
                    modem_ready_.store(false, std::memory_order_release);
                    ESP_LOGW(TAG, "Failed to set A7682E output gain");
                }
            } else {
                pending_gain_.store(pending_gain, std::memory_order_release);
            }
        }

        bool command_received = false;
        {
            std::lock_guard<std::mutex> lock(command_mutex_);
            if (command_queue_ == nullptr) {
                break;
            }
            if (xQueueReceive(command_queue_, &command, 0) == pdTRUE) {
                command_received = true;
                if (command.type == CommandType::Speak) {
                    speech_active_.store(true, std::memory_order_release);
                }
            }
        }
        if (!command_received) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (command.type == CommandType::Stop) {
            stop_requested_.store(false, std::memory_order_release);
            if (EnsureReady()) {
                std::string response;
                if (!SendCommand("AT+CTTS=0", response, 1500)) {
                    ESP_LOGW(TAG, "AT+CTTS=0 failed");
                }
            }
            continue;
        }

        if (command.type != CommandType::Speak) {
            continue;
        }
        if (!EnsureReady()) {
            ESP_LOGW(TAG, "Dropping AT command while A7682E is unavailable");
            CompleteSpeechCommand();
            continue;
        }

        const bool tts_ok = EncodeAndSendTts(command.text);
        CompleteSpeechCommand();
        if (!tts_ok) {
            modem_ready_.store(false, std::memory_order_release);
            ESP_LOGW(TAG, "CTTS command failed");
        }
    }
}
