#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

#include <driver/gpio.h>
#include <driver/rmt_encoder.h>
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class IrController {
public:
    IrController(gpio_num_t tx_gpio, gpio_num_t rx_gpio);
    ~IrController();

    std::expected<void, std::string> StartLearning(const std::string& button, bool overwrite);
    std::expected<void, std::string> Send(const std::string& button);
    bool HasCode(const std::string& button);
    bool IsReady() const { return ready_; }

private:
    static constexpr uint32_t kResolutionHz = 1000000;
    static constexpr size_t kMaxSymbols = 64;
    static constexpr uint32_t kStoredCodeMagic = 0x49524E43;  // IRNC

    struct ReceiveEvent {
        const rmt_symbol_word_t* symbols;
        size_t symbol_count;
    };

    struct StoredCode {
        uint32_t magic;
        uint16_t address;
        uint16_t command;
    };

    static bool OnReceiveDone(rmt_channel_handle_t channel,
                              const rmt_rx_done_event_data_t* event_data,
                              void* user_data);
    static void ReceiverTaskEntry(void* user_data);

    void ReceiverTask();
    bool ArmReceiver();
    bool DecodeNec(const rmt_symbol_word_t* symbols, size_t symbol_count,
                   uint16_t& address, uint16_t& command) const;
    bool SaveCode(const std::string& button, uint16_t address, uint16_t command);
    bool LoadCode(const std::string& button, StoredCode& code);
    bool IsValidButtonName(const std::string& button) const;
    bool LearningExpired(TickType_t now) const;
    void ClearLearningState();

    gpio_num_t tx_gpio_;
    gpio_num_t rx_gpio_;
    rmt_channel_handle_t tx_channel_ = nullptr;
    rmt_channel_handle_t rx_channel_ = nullptr;
    rmt_encoder_handle_t copy_encoder_ = nullptr;
    QueueHandle_t receive_queue_ = nullptr;
    SemaphoreHandle_t state_mutex_ = nullptr;
    SemaphoreHandle_t tx_mutex_ = nullptr;
    TaskHandle_t receiver_task_ = nullptr;
    std::array<rmt_symbol_word_t, kMaxSymbols> rx_symbols_{};
    std::string pending_button_;
    TickType_t learning_deadline_ = 0;
    bool learning_ = false;
    bool ready_ = false;
};
