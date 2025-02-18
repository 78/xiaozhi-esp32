#include "hna_16mm65t.h"
#include "driver/usb_serial_jtag.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>
#include <esp_log.h>

// 定义日志标签
#define TAG "HNA_16MM65T"

/**
 * @brief HNA_16MM65T 类的构造函数。
 *
 * 初始化 PT6324 设备，并创建一个任务用于刷新显示和执行动画。
 *
 * @param spi_device SPI 设备句柄，用于与 PT6324 通信。
 */
HNA_16MM65T::HNA_16MM65T(spi_device_handle_t spi_device) : PT6324Writer(spi_device)
{
    // 初始化 PT6324 设备
    pt6324_init();

    // 创建一个任务用于刷新显示和执行动画
    xTaskCreate(
        [](void *arg)
        {
            // 将参数转换为 HNA_16MM65T 指针
            HNA_16MM65T *vfd = static_cast<HNA_16MM65T *>(arg);
            while (true)
            {
                // 刷新显示
                vfd->pt6324_refrash(vfd->gram);
                // 执行动画
                vfd->animate();
                // 任务延时 10 毫秒
                vTaskDelay(pdMS_TO_TICKS(10));
                // 可取消注释以打印任务空闲栈大小
                // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
                // ESP_LOGI(TAG, "Task refrash free stack size: %u bytes", uxHighWaterMark * sizeof(StackType_t));
            }
            // 删除当前任务
            vTaskDelete(NULL);
        },
        "vfd",
        4096 - 1024,
        this,
        6,
        nullptr);
}

/**
 * @brief 显示频谱信息。
 *
 * 处理传入的频谱数据，计算每个频段的平均值，并应用增益。
 * 更新目标值和动画步数，用于后续动画效果。
 *
 * @param buf 频谱数据缓冲区，包含每个频段的幅度值。
 * @param size 缓冲区的大小，即频谱数据的数量。
 */
void HNA_16MM65T::spectrum_show(float *buf, int size) // 0-100
{
#if true
    // 定义每个频段的增益系数
    static float fft_gain[FFT_SIZE] = {4.0f, 3.0f, 3.0f, 3.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
    // 定义每个频段的显示位置映射
    static uint8_t fft_postion[FFT_SIZE] = {0, FFT_SIZE - 1, 1, FFT_SIZE - 1 - 1, 2, FFT_SIZE - 1 - 2, 3, FFT_SIZE - 1 - 3, 4, FFT_SIZE - 1 - 4, 5, FFT_SIZE - 1 - 5};
    // 记录最大幅度值
    static float max = 0;
    // 存储每个频段的平均幅度值
    float fft_buf[FFT_SIZE];
    // 计算每个频段包含的数据元素数量
    int elements_per_part = size / 12;

    // 计算每个频段的平均幅度值
    for (int i = 0; i < FFT_SIZE; i++)
    {
        fft_buf[i] = 0;
        for (int j = 0; j < elements_per_part; j++)
        {
            fft_buf[i] += buf[i * elements_per_part + j] / elements_per_part;
        }
        // 更新最大幅度值
        if (max < fft_buf[i])
        {
            max = fft_buf[i];
        }

        // 确保幅度值非负，并应用增益
        if (fft_buf[i] < 0)
            fft_buf[i] = 0;
        else
            fft_buf[i] *= fft_gain[i];
    }

#else
#endif

    // 更新上一次的值、目标值和动画步数
    for (size_t i = 0; i < FFT_SIZE; i++)
    {
        last_values[i] = target_values[i];
        target_values[i] = fft_buf[fft_postion[i]];
        animation_steps[i] = 0;
    }
    // 打印最大幅度值和每个频段的目标值
    ESP_LOGI(TAG, "%d-FFT: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", (int)max, target_values[0], target_values[1], target_values[2], target_values[3], target_values[4], target_values[5],
             target_values[6], target_values[7], target_values[8], target_values[9], target_values[10], target_values[11]);
}

/**
 * @brief 测试函数，创建一个任务用于模拟频谱数据显示。
 *
 * 创建一个任务，在任务中随机生成频谱数据并调用 spectrum_show 函数显示。
 * 同时可以进行数字显示和点矩阵显示的测试。
 */
void HNA_16MM65T::test()
{
    // 创建一个任务用于测试显示功能
    xTaskCreate(
        [](void *arg)
        {
            // 将参数转换为 HNA_16MM65T 指针
            HNA_16MM65T *vfd = static_cast<HNA_16MM65T *>(arg);
            // 配置 USB SERIAL JTAG（此处注释掉，未实际使用）
            // usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
            //     .tx_buffer_size = BUF_SIZE,
            //     .rx_buffer_size = BUF_SIZE,
            // };
            // 存储测试用的频谱数据
            float testbuff[FFT_SIZE];
            // 安装 USB SERIAL JTAG 驱动（此处注释掉，未实际使用）
            // ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
            // 分配接收数据的缓冲区（此处注释掉，未实际使用）
            // uint8_t *recv_data = (uint8_t *)malloc(BUF_SIZE);
            while (1)
            {
                // 清空接收缓冲区（此处注释掉，未实际使用）
                // memset(recv_data, 0, BUF_SIZE);
                // 从 USB SERIAL JTAG 读取数据（此处注释掉，未实际使用）
                // int len = usb_serial_jtag_read_bytes(recv_data, BUF_SIZE - 1, 0x20 / portTICK_PERIOD_MS);
                // if (len > 0)
                {
                    // 设置点矩阵显示状态（此处注释掉，未实际使用）
                    // vfd->dotshelper((Dots)((recv_data[0] - '0') % 4));
                    // 显示数字 9
                    for (int i = 0; i < 10; i++)
                        vfd->numhelper(i, '9');
                    // 随机生成频谱数据
                    for (int i = 0; i < FFT_SIZE; i++)
                        testbuff[i] = rand() % 100;
                    // 显示频谱数据
                    vfd->spectrum_show(testbuff, FFT_SIZE);
                    // 解析接收数据（此处注释掉，未实际使用）
                    // int index = 0, data = 0;
                    // sscanf((char *)recv_data, "%d:%X", &index, &data);
                    // printf("Parsed numbers: %d and 0x%02X\n", index, data);
                    // gram[index] = data;
                }
                // 可取消注释以打印任务空闲栈大小
                // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
                // ESP_LOGI(TAG, "Task free stack size: %u bytes", uxHighWaterMark * sizeof(StackType_t));
                // 任务延时 100 毫秒
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            // 删除当前任务
            vTaskDelete(NULL);
        },
        "vfd1",
        4096 - 1024,
        this,
        5,
        nullptr);
}

// 可显示的字符数组
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

// 每个字符对应的十六进制编码
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

/**
 * @brief 根据字符查找对应的十六进制编码。
 *
 * 在字符数组中查找给定字符，并返回其对应的十六进制编码。
 *
 * @param ch 要查找的字符。
 * @return 字符对应的十六进制编码，如果未找到则返回 0。
 */
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

/**
 * @brief 显示数字字符。
 *
 * 根据给定的索引和字符，查找对应的十六进制编码，并更新显示缓冲区。
 *
 * @param index 数字显示的索引位置。
 * @param ch 要显示的字符。
 */
void HNA_16MM65T::numhelper(int index, char ch)
{
    // 检查索引是否越界
    if (index >= 10)
        return;
    // 查找字符对应的十六进制编码
    uint32_t val = find_hex_code(ch);
    // 更新显示缓冲区
    gram[NUM_BEGIN + index * 3 + 2] = val >> 16;
    gram[NUM_BEGIN + index * 3 + 1] = val >> 8;
    gram[NUM_BEGIN + index * 3 + 0] = val & 0xff;
}

// 每个符号在显示缓冲区中的位置
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
/**
 * @brief 根据符号枚举值查找其在显示缓冲区中的位置。
 *
 * 该函数接收一个符号枚举值，通过查询 `symbolPositions` 数组，
 * 找到该符号对应的字节索引和位索引，并将其存储在传入的指针所指向的变量中。
 *
 * @param flag 符号的枚举值，用于标识要查找的符号。
 * @param byteIndex 指向一个整数的指针，用于存储符号所在的字节索引。
 * @param bitIndex 指向一个整数的指针，用于存储符号所在的位索引。
 */
void find_enum_code(Symbols flag, int *byteIndex, int *bitIndex)
{
    *byteIndex = symbolPositions[flag].byteIndex;
    *bitIndex = symbolPositions[flag].bitIndex;
}

/**
 * @brief 控制特定符号的显示状态。
 *
 * 根据传入的符号枚举值和显示状态标志，找到该符号在显示缓冲区中的位置，
 * 并相应地设置或清除该位置的位，以控制符号的显示或隐藏。
 *
 * @param symbol 要控制的符号的枚举值。
 * @param is_on 一个布尔值，指示符号是否应该显示（true 表示显示，false 表示隐藏）。
 */
void HNA_16MM65T::symbolhelper(Symbols symbol, bool is_on)
{
    // 检查符号枚举值是否越界，如果越界则直接返回
    if (symbol >= SYMBOL_MAX)
        return;

    int byteIndex, bitIndex;
    // 调用 find_enum_code 函数查找符号在显示缓冲区中的位置
    find_enum_code(symbol, &byteIndex, &bitIndex);

    // 可取消注释以打印符号的位置信息
    // printf("symbol %d 所在字节: %d, 所在位: %d\n", symbol, byteIndex, bitIndex);

    if (is_on)
        // 如果要显示符号，则将该位置的位设置为 1
        gram[byteIndex] |= bitIndex;
    else
        // 如果要隐藏符号，则将该位置的位设置为 0
        gram[byteIndex] &= ~bitIndex;
}

/**
 * @brief 根据不同的点矩阵状态更新显示缓冲区。
 *
 * 该函数根据传入的点矩阵状态枚举值，对显示缓冲区中的特定字节进行操作，
 * 以实现不同的点矩阵显示效果。在操作之前，会先清除相关字节中的特定位。
 *
 * @param dot 点矩阵的状态枚举值，指示要显示的点矩阵样式。
 */
void HNA_16MM65T::dotshelper(Dots dot)
{
    // 清除 gram[1] 字节中从第 3 位到第 7 位的位
    gram[1] &= ~0xF8;
    // 清除 gram[2] 字节中从第 0 位到第 3 位的位
    gram[2] &= ~0xF;

    switch (dot)
    {
    case DOT_MATRIX_UP:
        // 如果是向上的点矩阵样式，设置 gram[1] 字节的相应位
        gram[1] |= 0x78;
        break;
    case DOT_MATRIX_NEXT:
        // 如果是下一个的点矩阵样式，设置 gram[1] 和 gram[2] 字节的相应位
        gram[1] |= 0xD0;
        gram[2] |= 0xA;
        break;
    case DOT_MATRIX_PAUSE:
        // 如果是暂停的点矩阵样式，设置 gram[1] 和 gram[2] 字节的相应位
        gram[1] |= 0xB2;
        gram[2] |= 0x1;
        break;
    case DOT_MATRIX_FILL:
        // 如果是填充的点矩阵样式，设置 gram[1] 和 gram[2] 字节的相应位
        gram[1] |= 0xF8;
        gram[2] |= 0x7;
        break;
    }
}

/**
 * @brief 根据索引和级别更新波形显示。
 *
 * 该函数根据传入的索引和级别，在显示缓冲区中更新相应的波形显示。
 * 会先检查索引和级别是否在有效范围内，然后根据级别设置或清除显示缓冲区中的位。
 *
 * @param index 波形的索引，用于确定在显示缓冲区中的起始位置。
 * @param level 波形的级别，指示波形的高度，范围通常为 0 到 8。
 */
void HNA_16MM65T::wavehelper(int index, int level)
{
    // 定义每个波形在显示缓冲区中的起始位置
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

    // 检查索引是否越界，如果越界则直接返回
    if (index >= 12)
        return;
    // 确保级别不超过 8
    if (level > 8)
        level = 8;

    int byteIndex = wavePositions[index].byteIndex, bitIndex = wavePositions[index].bitIndex;

    if (level)
        // 如果级别不为 0，则设置相应字节的最高位
        gram[byteIndex + 2] |= 0x80;
    else if (level == -1)
        // 如果级别为 -1，则清除相应字节的最高位
        gram[byteIndex + 2] &= ~0x80;

    for (size_t i = 0; i < 7; i++)
    {
        if ((i) >= (8 - level) && level > 1)
            // 如果当前位置在波形高度范围内，则设置相应位
            gram[byteIndex] |= bitIndex;
        else
            // 否则清除相应位
            gram[byteIndex] &= ~bitIndex;

        // 位索引左移 3 位
        bitIndex <<= 3;
        if (bitIndex > 0xFF)
        {
            // 如果位索引超过一个字节的范围，则右移 8 位并将字节索引加 1
            bitIndex >>= 8;
            byteIndex++;
        }
    }
}