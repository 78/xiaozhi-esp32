#include "scs_servo_bus.h"

#include <esp_log.h>

#define TAG "ScsServoBus"

namespace {

constexpr uint8_t kInstWrite = 0x03;
constexpr uint8_t kRegTorqueEnable = 40;
constexpr uint8_t kRegGoalPosition = 42;
constexpr size_t kUartBufferSize = 512;

// SCSCL stores 16-bit values high byte first.
void PutWord(uint8_t* dest, uint16_t value) {
    dest[0] = static_cast<uint8_t>(value >> 8);
    dest[1] = static_cast<uint8_t>(value & 0xFF);
}

}  // namespace

ScsServoBus::ScsServoBus(uart_port_t port, int baud_rate, gpio_num_t tx_pin, gpio_num_t rx_pin)
    : port_(port) {
    uart_config_t uart_config = {};
    uart_config.baud_rate = baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_driver_install(port_, kUartBufferSize, kUartBufferSize, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return;
    }
    err = uart_param_config(port_, &uart_config);
    if (err == ESP_OK) {
        err = uart_set_pin(port_, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart setup failed: %s", esp_err_to_name(err));
        uart_driver_delete(port_);
        return;
    }

    installed_ = true;
    ESP_LOGI(TAG, "SCS servo bus ready on UART%d (tx=%d, rx=%d, %d bps)", port_, tx_pin, rx_pin, baud_rate);
}

ScsServoBus::~ScsServoBus() {
    if (installed_) {
        uart_driver_delete(port_);
    }
}

void ScsServoBus::WriteBuf(uint8_t id, uint8_t mem_addr, const uint8_t* data, uint8_t length,
                           uint8_t instruction) {
    if (!installed_) {
        return;
    }

    uint8_t packet[16];
    uint8_t msg_len = length + 3;
    packet[0] = 0xFF;
    packet[1] = 0xFF;
    packet[2] = id;
    packet[3] = msg_len;
    packet[4] = instruction;
    packet[5] = mem_addr;

    uint8_t checksum = id + msg_len + instruction + mem_addr;
    for (uint8_t i = 0; i < length; i++) {
        packet[6 + i] = data[i];
        checksum += data[i];
    }
    packet[6 + length] = ~checksum;

    uart_write_bytes(port_, packet, 7 + length);
}

void ScsServoBus::WritePosition(uint8_t id, uint16_t position, uint16_t move_time_ms, uint16_t speed) {
    uint8_t params[6];
    PutWord(params + 0, position);
    PutWord(params + 2, move_time_ms);
    PutWord(params + 4, speed);
    WriteBuf(id, kRegGoalPosition, params, sizeof(params), kInstWrite);
}

void ScsServoBus::EnableTorque(uint8_t id, bool enable) {
    uint8_t value = enable ? 1 : 0;
    WriteBuf(id, kRegTorqueEnable, &value, 1, kInstWrite);
}
