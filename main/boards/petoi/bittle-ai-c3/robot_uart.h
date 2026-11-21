#ifndef PETOI_ROBOT_UART_H_
#define PETOI_ROBOT_UART_H_

#include <atomic>

#include <driver/uart.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "config.h"
#include "robot_frame.h"

namespace petoi {

class RobotUart {
public:
    esp_err_t Initialize() {
        uart_config_t config{};
        config.baud_rate = ROBOT_UART_BAUD_RATE;
        config.data_bits = UART_DATA_8_BITS;
        config.parity = UART_PARITY_DISABLE;
        config.stop_bits = UART_STOP_BITS_1;
        config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        config.source_clk = UART_SCLK_DEFAULT;
        esp_err_t error = uart_param_config(UART_NUM_1, &config);
        if (error != ESP_OK) {
            return error;
        }
        error = uart_set_pin(UART_NUM_1, ROBOT_UART_TX_PIN, ROBOT_UART_RX_PIN, UART_PIN_NO_CHANGE,
                             UART_PIN_NO_CHANGE);
        if (error != ESP_OK) {
            return error;
        }
        error = uart_driver_install(UART_NUM_1, 1024, 0, 0, nullptr, 0);
        if (error != ESP_OK) {
            return error;
        }
        queue_ = xQueueCreate(4, sizeof(RobotFrame));
        if (queue_ == nullptr) {
            uart_driver_delete(UART_NUM_1);
            return ESP_ERR_NO_MEM;
        }
        if (xTaskCreate([](void* context) { static_cast<RobotUart*>(context)->Run(); },
                        "petoi_uart", 3072, this, 2, nullptr) != pdPASS) {
            vQueueDelete(queue_);
            queue_ = nullptr;
            uart_driver_delete(UART_NUM_1);
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }

    // Called by MCP on the application task: never wait for the wire or robot.
    esp_err_t Enqueue(const std::string& command) {
        RobotFrame frame;
        if (!BuildRobotFrame(command, frame)) {
            return ESP_ERR_INVALID_ARG;
        }
        if (queue_ == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        return xQueueSend(queue_, &frame, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
    }

    std::string GetStatus() const {
        return std::string("pending=") +
               std::to_string(queue_ ? uxQueueMessagesWaiting(queue_) : 0) +
               "; transmitting=" + (transmitting_.load() ? "true" : "false") +
               "; last_tx_error=" + esp_err_to_name(last_tx_error_.load()) +
               "; physical action completion is not tracked";
    }

private:
    QueueHandle_t queue_ = nullptr;
    std::atomic<esp_err_t> last_tx_error_{ESP_OK};
    std::atomic<bool> transmitting_{false};

    void Run() {
        RobotFrame frame;
        uint8_t response[64];
        for (;;) {
            if (xQueueReceive(queue_, &frame, pdMS_TO_TICKS(20)) == pdTRUE) {
                transmitting_.store(true);
                // No TX ring buffer: only this worker can block on transmission.
                const int written = uart_write_bytes(UART_NUM_1, frame.data.data(), frame.length);
                const esp_err_t error = written == static_cast<int>(frame.length)
                                            ? uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(300))
                                            : ESP_FAIL;
                last_tx_error_.store(error);
                transmitting_.store(false);
                if (error != ESP_OK) {
                    ESP_LOGE("PetoiUart", "TX failed: %s; command is not retried",
                             esp_err_to_name(error));
                }
            }
            // Drain bounded RX chunks. These bytes are not a completion contract.
            const int received = uart_read_bytes(UART_NUM_1, response, sizeof(response), 0);
            if (received > 0) {
                ESP_LOG_BUFFER_HEX_LEVEL("PetoiUart", response, received, ESP_LOG_DEBUG);
            }
        }
    }
};

}  // namespace petoi

#endif  // PETOI_ROBOT_UART_H_
