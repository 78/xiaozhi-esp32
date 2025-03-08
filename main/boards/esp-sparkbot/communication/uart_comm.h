#ifndef __UART_COMM_H__
#define __UART_COMM_H__

#include <driver/uart.h>
#include "simple_comm.h"

namespace iot {

class UARTComm : public SimpleComm {
public:
    UARTComm(uart_port_t port = UART_NUM_1, 
             int tx_pin = 0,
             int rx_pin = 0,
             int baud_rate = 115200);

    int Init() override;
    int Send(const std::string& str) override;
    void SetRecvCallback(RecvCallback callback) override;

private:
    uart_port_t port_;
    int tx_pin_, rx_pin_, baud_rate_;
    bool is_inited_ = false;
    RecvCallback callback_;
};

}; // namespace iot

#endif