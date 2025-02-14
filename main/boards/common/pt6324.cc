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

void PT6324Writer::pt6324_test()
{
    int symbol = 0;
    for (size_t j = 0; j < (int)SYMBOL_MAX; j++)
    {
        for (size_t i = 0; i < 12; i++)
            pt6324_wavehelper(i, (i + j) % 9);
        for (size_t i = 0; i < 10; i++)
            pt6324_numhelper(i, (i + j) % 10 + '0');
        pt6324_symbolhelper((Symbols)symbol++, 1);
        pt6324_refrash();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
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
        memset(recv_data, 0, BUF_SIZE);
        int len = usb_serial_jtag_read_bytes(recv_data, BUF_SIZE - 1, 0x20 / portTICK_PERIOD_MS);
        if (len > 0)
        {
            pt6324_dotshelper((Dots)((recv_data[0] - '0') % 4));
            for (int i = 0; i < 10; i++)
                pt6324_numhelper(i, recv_data[0]);
            for (size_t i = 0; i < 12; i++)
                pt6324_wavehelper(i, (recv_data[0] - '0') % 9);
            // int index = 0, data = 0;

            // sscanf((char *)recv_data, "%d:%X", &index, &data);
            // printf("Parsed numbers: %d and 0x%02X\n", index, data);
            // gram[index] = data;
            pt6324_refrash();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

const char characters[CHAR_COUNT] = {
    '0', '1', '2', '3', '4',
    '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E',
    'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y',
    'Z',
    'a', 'b', 'c', 'd', 'e',
    'f', 'g', 'h', 'i', 'j',
    'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't',
    'u', 'v', 'w', 'x', 'y',
    'z'};

const unsigned int hex_codes[CHAR_COUNT] = {
    0xf111f0, // 0
    0x210110, // 1
    0x61f0e0, // 2
    0x61e170, // 3
    0xb1e110, // 4
    0xd0e170, // 5
    0xd0f1f0, // 6
    0x610110, // 7
    0xf1f1f0, // 8
    0xf1e170, // 9
    0x51f190, // A
    0xd1f1e0, // B
    0xf010f0, // C
    0xd111e0, // D
    0xf0f0f0, // E
    0xf0f080, // F
    0xf031e0, // G
    0xb1f190, // H
    0x444460, // I
    0x2101f0, // J
    0xb2d290, // K
    0x9010f0, // L
    0xbb5190, // M
    0xb35990, // N
    0x511160, // O
    0x51f080, // P
    0x511370, // Q
    0x51f290, // R
    0x70e1e0, // S
    0xe44420, // T
    0xb11160, // U
    0xb25880, // V
    0xb15b90, // W
    0xaa4a90, // X
    0xaa4420, // Y
    0xe248f0, // Z
    0x51f190, // a
    0xd1f1e0, // b
    0xf010f0, // c
    0xd111e0, // d
    0xf0f0f0, // e
    0xf0f080, // f
    0xf031e0, // g
    0xb1f190, // h
    0x444460, // i
    0x2101f0, // j
    0xb2d290, // k
    0x9010f0, // l
    0xbb5190, // m
    0xb35990, // n
    0x511160, // o
    0x51f080, // p
    0x511370, // q
    0x51f290, // r
    0x70e1e0, // s
    0xe44420, // t
    0xb11160, // u
    0xb25880, // v
    0xb15b90, // w
    0xaa4a90, // x
    0xaa4420, // y
    0xe248f0, // z
};

unsigned int find_hex_code(char ch)
{
    for (int i = 0; i < CHAR_COUNT; i++)
    {
        if (characters[i] == ch)
        {
            return hex_codes[i];
        }
    }
    return 0;
}

void PT6324Writer::pt6324_numhelper(int index, char ch)
{
    if (index >= 10)
        return;
    uint32_t val = find_hex_code(ch);
    gram[NUM_BEGIN + index * 3 + 2] = val >> 16;
    gram[NUM_BEGIN + index * 3 + 1] = val >> 8;
    gram[NUM_BEGIN + index * 3 + 0] = val & 0xff;
}

SymbolPosition symbolPositions[] = {
    {0, 2},     // R_OUTER_B
    {0, 4},     // R_OUTER_A
    {0, 8},     // R_CENTER
    {0, 0x10},  // L_OUTER_B
    {0, 0x20},  // L_OUTER_A
    {0, 0x40},  // L_CENTER
    {0, 0x80},  // STEREO
    {1, 1},     // MONO
    {1, 2},     // GIGA
    {1, 4},     // REC_1
    {1, 8},     // DOT_MATRIX_4_6
    {1, 0x10},  // DOT_MATRIX_5_2_5_3_6_3
    {1, 0x20},  // DOT_MATRIX_0_3_0_5_0_6_1_2_1_3_1_5_1_6
    {1, 0x40},  // DOT_MATRIX_3_1_3_2_3_3_3_5_3_6_4_0_4_1_4_2_4_3_4_5_4_6_5_1_5_2_5_3_5_5
    {1, 0x80},  // DOT_MATRIX_5_4
    {2, 1},     // DOT_MATRIX_0_0_0_1_0_2_0_3_0_5_1_0_1_1_1_3_1_5_5_0_5_1_6_0_6_1_6_2_6_5
    {2, 2},     // DOT_MATRIX_2_0_2_4_3_4_4_4
    {2, 4},     // DOT_MATRIX_4_0
    {2, 8},     // DOT_MATRIX_2_MINUS1_2_7
    {2, 0x10},  // USB2
    {2, 0x20},  // USB1
    {2, 0x40},  // REC_2
    {2, 0x80},  // LBAR_RBAR
    {39, 1},    // CENTER_OUTLAY_BLUEA
    {39, 2},    // CENTER_OUTLAY_BLUEB
    {39, 4},    // CENTER_OUTLAY_REDA
    {39, 8},    // CENTER_OUTLAY_REDB
    {39, 0x10}, // CENTER_INLAY_BLUER
    {39, 0x20}, // CENTER_INLAY_BLUET
    {39, 0x40}, // CENTER_INLAY_BLUEL
    {39, 0x80}, // CENTER_INLAY_BLUEB
    {40, 1},    // CENTER_INLAY_RED1
    {40, 2},    // CENTER_INLAY_RED2
    {40, 4},    // CENTER_INLAY_RED3
    {40, 8},    // CENTER_INLAY_RED4
    {40, 0x10}, // CENTER_INLAY_RED5
    {40, 0x20}, // CENTER_INLAY_RED6
    {40, 0x40}, // CENTER_INLAY_RED7
    {40, 0x80}, // CENTER_INLAY_RED8
    {41, 1},    // CENTER_INLAY_RED9
    {41, 2},    // CENTER_INLAY_RED10
    {41, 4},    // CENTER_INLAY_RED11
    {41, 8},    // CENTER_INLAY_RED12
    {41, 0x10}, // CENTER_INLAY_RED13
    {41, 0x20}, // CENTER_INLAY_RED14
    {41, 0x40}, // CENTER_INLAY_RED15
    {41, 0x80}  // CENTER_INLAY_RED16
};

void find_enum_code(Symbols flag, int *byteIndex, int *bitIndex)
{
    *byteIndex = symbolPositions[flag].byteIndex;
    *bitIndex = symbolPositions[flag].bitIndex;
}

void PT6324Writer::pt6324_symbolhelper(Symbols symbol, bool is_on)
{
    if (symbol >= SYMBOL_MAX)
        return;
    int byteIndex, bitIndex;
    find_enum_code(symbol, &byteIndex, &bitIndex);

    printf("symbol %d 所在字节: %d, 所在位: %d\n", symbol, byteIndex, bitIndex);
    if (is_on)
        gram[byteIndex] |= bitIndex;
    else
        gram[byteIndex] &= ~bitIndex;
}

void PT6324Writer::pt6324_dotshelper(Dots dot)
{
    gram[1] &= ~0xF8;
    gram[2] &= ~0xF;

    switch (dot)
    {
    case DOT_MATRIX_UP:
        gram[1] |= 0x78;
        break;
    case DOT_MATRIX_NEXT:
        gram[1] |= 0xD0;
        gram[2] |= 0xA;
        break;
    case DOT_MATRIX_PAUSE:
        gram[1] |= 0xB2;
        gram[2] |= 0x1;
        break;
    case DOT_MATRIX_FILL:
        gram[1] |= 0xF8;
        gram[2] |= 0xF;
        break;
    }
}

void PT6324Writer::pt6324_wavehelper(int index, int level)
{
    static SymbolPosition wavePositions[] = {
        {33, 0x10},
        {33, 8},
        {33, 4},

        {36, 0x10},
        {36, 8},
        {36, 4},

        {42, 4},
        {42, 8},
        {42, 0x10},

        {45, 4},
        {45, 8},
        {45, 0x10},
    };
    if (index >= 12 || level > 8)
        return;
    int byteIndex = wavePositions[index].byteIndex, bitIndex = wavePositions[index].bitIndex;

    if (level)
        gram[byteIndex + 2] |= 0x80;
    else if (level == -1)
        gram[byteIndex + 2] &= ~0x80;

    for (size_t i = 0; i < 7; i++)
    {
        if ((i) >= (8 - level) && level > 1)
            gram[byteIndex] |= bitIndex;
        else
            gram[byteIndex] &= ~bitIndex;

        bitIndex <<= 3;
        if (bitIndex > 0xFF)
        {
            bitIndex >>= 8;
            byteIndex++;
        }
    }
}