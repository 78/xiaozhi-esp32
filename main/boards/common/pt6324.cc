/*
 * Author: 施华锋
 * Date: 2025-02-12
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "pt6324.h"
#include "esp_log.h"
#include "string.h"

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
    uint8_t data[] = {0x0F, 0x40, 0x8F}; // 2. 亮度
    pt6324_write_data(data, (sizeof data) * 8);
}

void PT6324Writer::pt6324_test()
{
    uint8_t data[] = {0xC0};
    pt6324_write_data(data, (sizeof data) * 8);

    uint8_t data_gram[48] = {0};
    for (size_t i = 0; i < sizeof data_gram; i++)
        data_gram[i] = i;
    pt6324_write_data(data_gram, (sizeof data_gram) * 8);

    data[0] = 0x0F;
    pt6324_write_data(data, (sizeof data) * 8);
}
