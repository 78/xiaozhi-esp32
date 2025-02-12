/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PT6324_H_
#define _PT6324_H_

#include "esp_log.h"
#include <driver/spi_master.h>

class PT6324Writer
{
public:
    PT6324Writer(spi_device_handle_t spi_device) : spi_device_(spi_device) {}
    void pt6324_init();
    void pt6324_test();
private:
    spi_device_handle_t spi_device_;
    void pt6324_write_data(uint8_t *dat, int len);
};

#endif
