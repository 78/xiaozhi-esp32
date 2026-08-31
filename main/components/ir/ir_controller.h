#pragma once

#include <cstddef>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/rmt.h"

class IrController {
public:
    IrController(gpio_num_t rx_gpio, gpio_num_t tx_gpio, bool module_internal_modulator = true);
    ~IrController();

    esp_err_t SendNEC(uint16_t address, uint16_t command);
    bool GetLastReceived(uint16_t &address, uint16_t &command);

private:
    gpio_num_t rx_gpio_;
    gpio_num_t tx_gpio_;
    bool module_internal_modulator_;

    volatile uint16_t last_address_;
    volatile uint16_t last_command_;
    volatile bool last_valid_;

    esp_err_t InitRmtTx();
    esp_err_t InitRmtRx();
    static void RmtRxTask(void* arg);
    size_t BuildNecItems(uint16_t address, uint16_t command, rmt_item32_t* out_items, size_t max_items);

    IrController(const IrController&) = delete;
    IrController& operator=(const IrController&) = delete;
};
