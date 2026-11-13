#include "ir_controller.h"
#include "config.h"

#include <array>

#include <driver/rmt_common.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs.h>

namespace {

constexpr char kTag[] = "IrController";
constexpr char kNvsNamespace[] = "ir_codes";
constexpr TickType_t kLearningTimeout = pdMS_TO_TICKS(10000);
constexpr uint32_t kTimingMarginUs = 250;

constexpr uint32_t kNecLeadingMarkUs = 9000;
constexpr uint32_t kNecLeadingSpaceUs = 4500;
constexpr uint32_t kNecBitMarkUs = 560;
constexpr uint32_t kNecZeroSpaceUs = 560;
constexpr uint32_t kNecOneSpaceUs = 1690;
constexpr uint32_t kNecEndingSpaceUs = 0x7FFF;

bool TimingMatches(uint32_t measured, uint32_t expected) {
    const uint32_t minimum = expected > kTimingMarginUs ? expected - kTimingMarginUs : 0;
    return measured >= minimum && measured <= expected + kTimingMarginUs;
}

}  // namespace

IrController::IrController(gpio_num_t tx_gpio, gpio_num_t rx_gpio)
    : tx_gpio_(tx_gpio), rx_gpio_(rx_gpio) {
    receive_queue_ = xQueueCreate(4, sizeof(ReceiveEvent));
    state_mutex_ = xSemaphoreCreateMutex();
    tx_mutex_ = xSemaphoreCreateMutex();
    if (receive_queue_ == nullptr || state_mutex_ == nullptr || tx_mutex_ == nullptr) {
        ESP_LOGE(kTag, "Failed to allocate IR synchronization objects");
        return;
    }

    rmt_rx_channel_config_t rx_config = {};
    rx_config.gpio_num = rx_gpio_;
    rx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rx_config.resolution_hz = kResolutionHz;
    rx_config.mem_block_symbols = kMaxSymbols;
    esp_err_t error = rmt_new_rx_channel(&rx_config, &rx_channel_);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create RX channel: %s", esp_err_to_name(error));
        return;
    }

    rmt_rx_event_callbacks_t callbacks = {};
    callbacks.on_recv_done = OnReceiveDone;
    error = rmt_rx_register_event_callbacks(rx_channel_, &callbacks, this);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register RX callback: %s", esp_err_to_name(error));
        return;
    }

    rmt_tx_channel_config_t tx_config = {};
    tx_config.gpio_num = tx_gpio_;
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.resolution_hz = kResolutionHz;
    tx_config.mem_block_symbols = kMaxSymbols;
    tx_config.trans_queue_depth = 4;
    error = rmt_new_tx_channel(&tx_config, &tx_channel_);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create TX channel: %s", esp_err_to_name(error));
        return;
    }

    rmt_carrier_config_t carrier_config = {};
    carrier_config.frequency_hz = IR_CARRIER_HZ;
    carrier_config.duty_cycle = 0.33;
    error = rmt_apply_carrier(tx_channel_, &carrier_config);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to apply TX carrier: %s", esp_err_to_name(error));
        return;
    }

    rmt_copy_encoder_config_t encoder_config = {};
    error = rmt_new_copy_encoder(&encoder_config, &copy_encoder_);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to create copy encoder: %s", esp_err_to_name(error));
        return;
    }

    error = rmt_enable(tx_channel_);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to enable TX channel: %s", esp_err_to_name(error));
        return;
    }
    error = rmt_enable(rx_channel_);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to enable RX channel: %s", esp_err_to_name(error));
        return;
    }

    ready_ = true;
    if (xTaskCreate(ReceiverTaskEntry, "ir_rx", 4096, this, 3, &receiver_task_) != pdPASS) {
        ready_ = false;
        receiver_task_ = nullptr;
        ESP_LOGE(kTag, "Failed to create IR receiver task");
        return;
    }

    if (!ArmReceiver()) {
        ready_ = false;
        ESP_LOGE(kTag, "Failed to arm initial IR receive operation");
        return;
    }

    ESP_LOGI(kTag, "IR ready: RX GPIO%d, TX GPIO%d, carrier %d Hz",
             static_cast<int>(rx_gpio_), static_cast<int>(tx_gpio_), IR_CARRIER_HZ);
}

IrController::~IrController() {
    ready_ = false;
    if (receiver_task_ != nullptr) {
        vTaskDelete(receiver_task_);
    }
    if (rx_channel_ != nullptr) {
        rmt_disable(rx_channel_);
        rmt_del_channel(rx_channel_);
    }
    if (tx_channel_ != nullptr) {
        rmt_disable(tx_channel_);
        rmt_del_channel(tx_channel_);
    }
    if (copy_encoder_ != nullptr) {
        rmt_del_encoder(copy_encoder_);
    }
    if (receive_queue_ != nullptr) {
        vQueueDelete(receive_queue_);
    }
    if (state_mutex_ != nullptr) {
        vSemaphoreDelete(state_mutex_);
    }
    if (tx_mutex_ != nullptr) {
        vSemaphoreDelete(tx_mutex_);
    }
}

std::expected<void, std::string> IrController::StartLearning(const std::string& button,
                                                             bool overwrite) {
    if (!ready_) {
        return std::unexpected("infrared controller is not ready");
    }
    if (!IsValidButtonName(button)) {
        return std::unexpected(
            "button must contain 1-15 lowercase letters, digits, or underscores");
    }
    if (!overwrite && HasCode(button)) {
        return std::unexpected("button already exists; set overwrite to true to replace it");
    }

    xSemaphoreTake(state_mutex_, portMAX_DELAY);
    if (learning_ && !LearningExpired(xTaskGetTickCount())) {
        xSemaphoreGive(state_mutex_);
        return std::unexpected("another infrared button is already being learned");
    }
    pending_button_ = button;
    learning_deadline_ = xTaskGetTickCount() + kLearningTimeout;
    learning_ = true;
    xSemaphoreGive(state_mutex_);

    ESP_LOGI(kTag, "Learning '%s': press the remote button within 10 seconds", button.c_str());
    return {};
}

std::expected<void, std::string> IrController::Send(const std::string& button) {
    if (!ready_) {
        return std::unexpected("infrared controller is not ready");
    }
    if (!IsValidButtonName(button)) {
        return std::unexpected("invalid button name");
    }

    StoredCode code = {};
    if (!LoadCode(button, code)) {
        return std::unexpected("no learned infrared code named '" + button + "'");
    }

    std::array<rmt_symbol_word_t, 34> symbols = {};
    auto set_symbol = [](rmt_symbol_word_t& symbol, uint32_t mark_us, uint32_t space_us) {
        symbol.level0 = 1;
        symbol.duration0 = mark_us;
        symbol.level1 = 0;
        symbol.duration1 = space_us;
    };
    set_symbol(symbols[0], kNecLeadingMarkUs, kNecLeadingSpaceUs);

    size_t symbol_index = 1;
    auto append_bits = [&symbols, &symbol_index, &set_symbol](uint16_t value) {
        for (int bit = 0; bit < 16; ++bit) {
            const bool one = (value & (1U << bit)) != 0;
            set_symbol(symbols[symbol_index++], kNecBitMarkUs,
                       one ? kNecOneSpaceUs : kNecZeroSpaceUs);
        }
    };
    append_bits(code.address);
    append_bits(code.command);
    set_symbol(symbols[symbol_index], kNecBitMarkUs, kNecEndingSpaceUs);

    rmt_transmit_config_t transmit_config = {};
    transmit_config.loop_count = 0;

    xSemaphoreTake(tx_mutex_, portMAX_DELAY);
    esp_err_t error = rmt_transmit(tx_channel_, copy_encoder_, symbols.data(),
                                   symbols.size() * sizeof(rmt_symbol_word_t),
                                   &transmit_config);
    if (error == ESP_OK) {
        error = rmt_tx_wait_all_done(tx_channel_, 1000);
    }
    xSemaphoreGive(tx_mutex_);

    if (error != ESP_OK) {
        return std::unexpected(std::string("infrared transmission failed: ") +
                               esp_err_to_name(error));
    }

    ESP_LOGI(kTag, "Sent '%s': address=0x%04X command=0x%04X", button.c_str(),
             code.address, code.command);
    return {};
}

bool IrController::HasCode(const std::string& button) {
    if (!IsValidButtonName(button)) {
        return false;
    }
    StoredCode code = {};
    return LoadCode(button, code);
}

bool IrController::OnReceiveDone(rmt_channel_handle_t,
                                 const rmt_rx_done_event_data_t* event_data,
                                 void* user_data) {
    auto* controller = static_cast<IrController*>(user_data);
    ReceiveEvent event = {};
    event.symbols = event_data->received_symbols;
    event.symbol_count = event_data->num_symbols;
    BaseType_t high_priority_task_woken = pdFALSE;
    xQueueSendFromISR(controller->receive_queue_, &event, &high_priority_task_woken);
    return high_priority_task_woken == pdTRUE;
}

void IrController::ReceiverTaskEntry(void* user_data) {
    static_cast<IrController*>(user_data)->ReceiverTask();
}

void IrController::ReceiverTask() {
    while (true) {
        ReceiveEvent event = {};
        if (xQueueReceive(receive_queue_, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            std::string button;
            xSemaphoreTake(state_mutex_, portMAX_DELAY);
            if (learning_ && !LearningExpired(xTaskGetTickCount())) {
                button = pending_button_;
            }
            xSemaphoreGive(state_mutex_);

            if (!button.empty()) {
                uint16_t address = 0;
                uint16_t command = 0;
                if (DecodeNec(event.symbols, event.symbol_count, address, command)) {
                    if (SaveCode(button, address, command)) {
                        ESP_LOGI(kTag, "Learned '%s': address=0x%04X command=0x%04X",
                                 button.c_str(), address, command);
                        xSemaphoreTake(state_mutex_, portMAX_DELAY);
                        ClearLearningState();
                        xSemaphoreGive(state_mutex_);
                    }
                } else {
                    ESP_LOGW(kTag, "Received %u symbols, but they are not a normal NEC frame",
                             static_cast<unsigned>(event.symbol_count));
                }
            }

            if (!ArmReceiver()) {
                ESP_LOGE(kTag, "Failed to re-arm IR receiver");
            }
        }

        xSemaphoreTake(state_mutex_, portMAX_DELAY);
        if (learning_ && LearningExpired(xTaskGetTickCount())) {
            ESP_LOGW(kTag, "Learning '%s' timed out", pending_button_.c_str());
            ClearLearningState();
        }
        xSemaphoreGive(state_mutex_);
    }
}

bool IrController::ArmReceiver() {
    rmt_receive_config_t receive_config = {};
    receive_config.signal_range_min_ns = 1000;
    receive_config.signal_range_max_ns = 12000000;
    esp_err_t error = rmt_receive(rx_channel_, rx_symbols_.data(),
                                  rx_symbols_.size() * sizeof(rmt_symbol_word_t),
                                  &receive_config);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "rmt_receive failed: %s", esp_err_to_name(error));
        return false;
    }
    return true;
}

bool IrController::DecodeNec(const rmt_symbol_word_t* symbols, size_t symbol_count,
                             uint16_t& address, uint16_t& command) const {
    if (symbol_count != 34) {
        return false;
    }
    if (!TimingMatches(symbols[0].duration0, kNecLeadingMarkUs) ||
        !TimingMatches(symbols[0].duration1, kNecLeadingSpaceUs)) {
        return false;
    }

    auto decode_bits = [symbols](size_t start, uint16_t& value) {
        value = 0;
        for (size_t bit = 0; bit < 16; ++bit) {
            const auto& symbol = symbols[start + bit];
            if (!TimingMatches(symbol.duration0, kNecBitMarkUs)) {
                return false;
            }
            if (TimingMatches(symbol.duration1, kNecOneSpaceUs)) {
                value |= static_cast<uint16_t>(1U << bit);
            } else if (!TimingMatches(symbol.duration1, kNecZeroSpaceUs)) {
                return false;
            }
        }
        return true;
    };

    return decode_bits(1, address) && decode_bits(17, command);
}

bool IrController::SaveCode(const std::string& button, uint16_t address, uint16_t command) {
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "nvs_open failed: %s", esp_err_to_name(error));
        return false;
    }

    StoredCode code = {};
    code.magic = kStoredCodeMagic;
    code.address = address;
    code.command = command;
    error = nvs_set_blob(handle, button.c_str(), &code, sizeof(code));
    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }
    nvs_close(handle);

    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Failed to save '%s': %s", button.c_str(), esp_err_to_name(error));
        return false;
    }
    return true;
}

bool IrController::LoadCode(const std::string& button, StoredCode& code) {
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (error != ESP_OK) {
        return false;
    }

    size_t size = sizeof(code);
    error = nvs_get_blob(handle, button.c_str(), &code, &size);
    nvs_close(handle);
    return error == ESP_OK && size == sizeof(code) && code.magic == kStoredCodeMagic;
}

bool IrController::IsValidButtonName(const std::string& button) const {
    if (button.empty() || button.size() > 15) {
        return false;
    }
    for (char character : button) {
        const bool lowercase = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        if (!lowercase && !digit && character != '_') {
            return false;
        }
    }
    return true;
}

bool IrController::LearningExpired(TickType_t now) const {
    return static_cast<int32_t>(now - learning_deadline_) >= 0;
}

void IrController::ClearLearningState() {
    learning_ = false;
    pending_button_.clear();
    learning_deadline_ = 0;
}
