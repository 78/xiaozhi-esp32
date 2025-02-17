/*
 * Author: 施华锋
 * Date: 2025-02-12
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "pt6324.h"
#include "esp_log.h"
#include "string.h"
#include "driver/usb_serial_jtag.h"

#define TAG "PT6324Writer"

void PT6324Writer::pt6324_write_data(uint8_t *dat, int len)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len;
    t.tx_buffer = dat;
    ESP_ERROR_CHECK(spi_device_transmit(spi_device_, &t));
}

void PT6324Writer::pt6324_init()
{
    uint8_t data[] = {0x0F, 0x0F, 0x40}; // 2. 亮度
    pt6324_write_data(data, (sizeof data) * 8);
}

void PT6324Writer::pt6324_refrash(uint8_t *gram)
{
    uint8_t data_gram[48 + 1] = {0};
    data_gram[0] = 0xC0;
    for (size_t i = 1; i < sizeof data_gram; i++)
        data_gram[i] = gram[i - 1];
    pt6324_write_data(data_gram, (sizeof data_gram) * 8);

    uint8_t data[1] = {0x8f};
    pt6324_write_data(data, (sizeof data) * 8);
}