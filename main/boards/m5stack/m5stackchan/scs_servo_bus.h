#ifndef _SCS_SERVO_BUS_H_
#define _SCS_SERVO_BUS_H_

#include <driver/gpio.h>
#include <driver/uart.h>

#include <cstdint>

// Write-only driver for Feetech SCSCL-series (SCS0009) serial bus servos.
// StackChan only ever commands the servos, so the reply path is not decoded.
class ScsServoBus {
public:
    ScsServoBus(uart_port_t port, int baud_rate, gpio_num_t tx_pin, gpio_num_t rx_pin);
    ~ScsServoBus();

    ScsServoBus(const ScsServoBus&) = delete;
    ScsServoBus& operator=(const ScsServoBus&) = delete;

    bool IsReady() const { return installed_; }

    void WritePosition(uint8_t id, uint16_t position, uint16_t move_time_ms, uint16_t speed = 0);
    void EnableTorque(uint8_t id, bool enable);

private:
    void WriteBuf(uint8_t id, uint8_t mem_addr, const uint8_t* data, uint8_t length, uint8_t instruction);

    uart_port_t port_;
    bool installed_ = false;
};

#endif // _SCS_SERVO_BUS_H_
