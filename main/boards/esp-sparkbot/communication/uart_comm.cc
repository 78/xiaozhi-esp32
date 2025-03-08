#include "uart_comm.h"

namespace iot {

UARTComm::UARTComm(uart_port_t port, int tx_pin, int rx_pin, int baud_rate)
    : port_(port), tx_pin_(tx_pin), rx_pin_(rx_pin), baud_rate_(baud_rate) {}

int UARTComm::Init() {
    uart_config_t config = {
        .baud_rate = baud_rate_,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (is_inited_) {
        return 0;
    }

    ESP_ERROR_CHECK(uart_driver_install(port_, 1024 * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port_, &config));
    ESP_ERROR_CHECK(uart_set_pin(port_, tx_pin_, rx_pin_, -1, -1));

    is_inited_ = true;
    return 0;
}

int UARTComm::Send(const std::string& str) {
    return uart_write_bytes(port_, str.c_str(), str.size()) == str.size();
}

/* TODO: implement recv function */
void UARTComm::SetRecvCallback(RecvCallback callback) {
    callback_ = callback;
}

}; // namespace iot