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

void PT6324Writer::pt6324_refrash()
{
    uint8_t data_gram[48 + 1] = {0};
    data_gram[0] = 0xC0;
    for (size_t i = 1; i < sizeof data_gram; i++)
        data_gram[i] = gram[i - 1];
    pt6324_write_data(data_gram, (sizeof data_gram) * 8);

    uint8_t data[1] = {0x8f};
    pt6324_write_data(data, (sizeof data) * 8);
}

#define BUF_SIZE (1024)
void PT6324Writer::pt6324_cali()
{
    // Configure USB SERIAL JTAG
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .tx_buffer_size = BUF_SIZE,
        .rx_buffer_size = BUF_SIZE,
    };

    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    uint8_t *recv_data = (uint8_t *)malloc(BUF_SIZE);
    while (1)
    {
        // 从UART读取数据
        memset(recv_data, 0, BUF_SIZE);
        int len = usb_serial_jtag_read_bytes(recv_data, BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if (len > 0)
        {
            for (int i = 0; i < 10; i++)
                pt6324_numhelper(i, recv_data[0]);
            // int index = 0, data = 0;

            // sscanf((char *)recv_data, "%d:%X", &index, &data);
            // printf("Parsed numbers: %d and 0x%02X\n", index, data);
            // gram[index] = data;
            pt6324_refrash();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// 定义字符数组
const char characters[CHAR_COUNT] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
    'u', 'v', 'w', 'x', 'y', 'z'};

// 定义对应的十六进制编码数组
const unsigned int hex_codes[CHAR_COUNT] = {
    0xf111f0, 0x210110, 0x61f0e0, 0x61e170, 0xb1e110,
    0xd0e170, 0xd0f1f0, 0x610110, 0xf1f1f0, 0xf1e170,
    0xf1f190, 0xd0d0e0, 0xd010e0, 0xd111e0, 0xC050E0,
    0xC07000, 0xE051E0, 0xB07000, 0x400040, 0x2001D0,
    0x907100, 0x8010C0, 0xB0B100, 0xB03000, 0xF111D0,
    0xF16100, 0xF113D0, 0xF17100, 0xC07050, 0x400040,
    0xA01190, 0x801190, 0xA011B0, 0xB03110, 0x903110,
    0x6020E0,
    0x70D0, 0x8070D0, 0x40E0, 0x2031D0, 0x4070D0,
    0xC07000, 0x3031D0, 0x807000, 0x4040, 0x1D0,
    0x807100, 0x8010C0, 0xE200, 0x7000, 0x70D0,
    0xC07000, 0x302100, 0x6000, 0x7050, 0x4040E0,
    0x1190, 0x1190, 0x11B0, 0x3110, 0x103110, 0x6060};

// 简单的测试函数，用于查找某个字符对应的编码
unsigned int find_hex_code(char ch)
{
    for (int i = 0; i < CHAR_COUNT; i++)
    {
        if (characters[i] == ch)
        {
            return hex_codes[i];
        }
    }
    return 0; // 如果未找到，返回 0
}

void PT6324Writer::pt6324_numhelper(int index, char ch)
{
    if (index >= 10)
        return;
    uint32_t val = find_hex_code(ch);
    gram[3 + index * 3 + 2] = val >> 16;
    gram[3 + index * 3 + 1] = val >> 8;
    gram[3 + index * 3 + 0] = val & 0xff;
}

void PT6324Writer::pt6324_test()
{
    uint8_t data_gram[48 + 1] = {0};
    data_gram[0] = 0xC0;
    for (size_t i = 1; i < sizeof data_gram; i++)
        data_gram[i] = i;
    pt6324_write_data(data_gram, (sizeof data_gram) * 8);

    uint8_t data[1] = {0x8f};
    pt6324_write_data(data, (sizeof data) * 8);
}
