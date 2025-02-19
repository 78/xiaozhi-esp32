/**
 * @file HNA_16MM65T.h
 * @brief 该头文件定义了 HNA_16MM65T 类，用于控制特定设备的显示，继承自 PT6324Writer 类。
 *
 * 该类提供了一系列方法，用于显示频谱、数字、符号、点矩阵等信息，同时支持动画效果。
 *
 * @author 施华锋
 * @date 2025-2-18
 */

#ifndef _HNA_16MM65T_H_
#define _HNA_16MM65T_H_

// 引入 PT6324Writer 类的头文件
#include "pt6324.h"
// 引入数学库，用于使用指数函数
#include <cmath>
// 引入 ESP32 Wi-Fi 相关库
#include <esp_wifi.h>

// 定义字符数量
#define CHAR_COUNT (62 + 1)
// 定义数字开始的索引
#define NUM_BEGIN 3

/**
 * @enum Dots
 * @brief 定义点矩阵的不同状态。
 */
typedef enum
{
    DOT_MATRIX_UP,    // 点矩阵向上
    DOT_MATRIX_NEXT,  // 点矩阵下一个
    DOT_MATRIX_PAUSE, // 点矩阵暂停
    DOT_MATRIX_FILL   // 点矩阵填充
} Dots;

/**
 * @enum Symbols
 * @brief 定义各种符号的枚举类型。
 */
typedef enum
{
    R_OUTER_B,
    R_OUTER_A,
    R_CENTER,
    L_OUTER_B,
    L_OUTER_A,
    L_CENTER,
    STEREO,
    MONO,
    GIGA,
    REC_1,
    DOT_MATRIX_4_6,
    DOT_MATRIX_5_2_5_3_6_3,
    DOT_MATRIX_0_3_0_5_0_6_1_2_1_3_1_5_1_6,
    DOT_MATRIX_3_1_3_2_3_3_3_5_3_6_4_0_4_1_4_2_4_3_4_5_4_6_5_1_5_2_5_3_5_5,
    DOT_MATRIX_5_4,
    DOT_MATRIX_0_0_0_1_0_2_0_3_0_5_1_0_1_1_1_3_1_5_5_0_5_1_6_0_6_1_6_2_6_5,
    DOT_MATRIX_2_0_2_4_3_4_4_4,
    DOT_MATRIX_4_0,
    DOT_MATRIX_2_MINUS1_2_7,
    USB2,
    USB1,
    REC_2,
    LBAR_RBAR,
    CENTER_OUTLAY_BLUEA,
    CENTER_OUTLAY_BLUEB,
    CENTER_OUTLAY_REDA,
    CENTER_OUTLAY_REDB,
    CENTER_INLAY_BLUER,
    CENTER_INLAY_BLUET,
    CENTER_INLAY_BLUEL,
    CENTER_INLAY_BLUEB,
    CENTER_INLAY_RED1,
    CENTER_INLAY_RED2,
    CENTER_INLAY_RED3,
    CENTER_INLAY_RED4,
    CENTER_INLAY_RED5,
    CENTER_INLAY_RED6,
    CENTER_INLAY_RED7,
    CENTER_INLAY_RED8,
    CENTER_INLAY_RED9,
    CENTER_INLAY_RED10,
    CENTER_INLAY_RED11,
    CENTER_INLAY_RED12,
    CENTER_INLAY_RED13,
    CENTER_INLAY_RED14,
    CENTER_INLAY_RED15,
    CENTER_INLAY_RED16,
    SYMBOL_MAX // 符号枚举的最大值
} Symbols;

typedef enum
{
    ANI_NONE,
    ANI_CLOCKWISE,
    ANI_ANTICLOCKWISE,
    ANI_UP2DOWN,
    ANI_DOWN2UP,
    ANI_LEFT2RT,
    ANI_RT2LEFT,
    ANI_MAX
} NumAni;
/**
 * @struct SymbolPosition
 * @brief 定义符号在显示缓冲区中的位置，由字节索引和位索引组成。
 */
typedef struct
{
    int byteIndex; // 字节索引
    int bitIndex;  // 位索引
} SymbolPosition;

/**
 * @class HNA_16MM65T
 * @brief 该类继承自 PT6324Writer 类，用于控制特定设备的显示。
 *
 * 提供了显示频谱、数字、符号、点矩阵等信息的方法，同时支持动画效果。
 */
class HNA_16MM65T : public PT6324Writer
{
    // 定义缓冲区 数量
#define BUF_SIZE (1024)
// 定义 FFT 数量
#define FFT_SIZE (12)
// 定义 数字 数量
#define NUM_SIZE (10)

private:
    uint8_t gram[48] = {0};                   // 显示缓冲区
    int wave_last_values[FFT_SIZE] = {0};     // 上一次的 FFT 值
    int wave_target_values[FFT_SIZE] = {0};   // 目标 FFT 值
    int wave_current_values[FFT_SIZE] = {0};  // 当前 FFT 值
    int wave_animation_steps[FFT_SIZE] = {0}; // 动画步数
    int wave_total_steps = 20;                // 动画总步数

    char number_buf[NUM_SIZE] = {0};
    char number_last_buf[NUM_SIZE] = {0};
    int number_animation_steps[NUM_SIZE] = {0}; // 动画步数
    NumAni number_animation_type = ANI_CLOCKWISE;

    // 每个字符对应的十六进制编码
    // !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz
    const unsigned int hex_codes[CHAR_COUNT] = {
        0x000000, // ' '
        0x044020, // !
        0,        // "
        0,        // #
        0,        // $
        0,        // %
        0,        // &
        0x040000, // '
        0x024200, // (
        0x084800, // )
        0x0eee00, // *
        0x04e420, // +
        0x000210, // ,
        0x00e000, // -
        0x000020, // .
        0x224880, // /
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
        0x020800, // :
        0x040420, // ;
        0x224210, // <
        0x00e060, // =
        0x884880, // >
        0x416020, // ?
        0,        // @
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
    };
    // 每个符号在显示缓冲区中的位置
    SymbolPosition symbolPositions[100] = {
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
     * @brief 执行动画效果，更新显示缓冲区。
     *
     * 使用指数衰减函数计算当前值，并调用 wavehelper 方法更新显示。
     */
    void waveanimate()
    {
        for (int i = 0; i < FFT_SIZE; i++)
        {
            if (wave_animation_steps[i] < wave_total_steps)
            {
                // 使用指数衰减函数计算当前值
                float progress = static_cast<float>(wave_animation_steps[i]) / wave_total_steps;
                float factor = 1 - std::exp(-3 * progress); // 指数衰减因子
                wave_current_values[i] = wave_last_values[i] + static_cast<int>((wave_target_values[i] - wave_last_values[i]) * factor);
                wavehelper(i, wave_current_values[i] * 8 / 90);
                wave_animation_steps[i]++;
            }
            else
            {
                wave_last_values[i] = wave_target_values[i];
                wavehelper(i, wave_target_values[i] * 8 / 90);
            }
        }
    }

    uint32_t numbergetpart(uint32_t raw, uint32_t mask)
    {
        return raw & mask;
    }

    void numberanimate()
    {
        for (int i = 0; i < NUM_SIZE; i++)
        {
            if (number_buf[i] != number_last_buf[i] || number_animation_steps[i] != 0)
            {
                number_last_buf[i] = number_buf[i];
                uint32_t raw_code = find_hex_code(number_buf[i]);
                uint32_t code = raw_code;
                if (number_animation_type == ANI_CLOCKWISE)
                {
                    switch (number_animation_steps[i])
                    {
                    case 0:
                        code = numbergetpart(raw_code, 0x080000 | 0x800000);
                        if (code != 0)
                            break;
                    case 1:
                        code = numbergetpart(raw_code, 0x4C0000 | 0x800000);
                        if (code != 0)
                            break;
                    case 2:
                        code = numbergetpart(raw_code, 0x6e0000 | 0x800000);
                        if (code != 0)
                            break;
                    case 3:
                        code = numbergetpart(raw_code, 0x6f6000 | 0x800000);
                        if (code != 0)
                            break;
                    case 4:
                        code = numbergetpart(raw_code, 0x6f6300 | 0x800000);
                        if (code != 0)
                            break;
                    case 5:
                        code = numbergetpart(raw_code, 0x6f6770 | 0x800000);
                        if (code != 0)
                            break;
                    case 6:
                        code = numbergetpart(raw_code, 0x6f6ff0 | 0x800000);
                        if (code != 0)
                            break;
                    case 7:
                        code = numbergetpart(raw_code, 0x6ffff0 | 0x800000);
                        if (code != 0)
                            break;
                    default:
                        number_animation_steps[i] = 0;
                        break;
                    }
                }
                else if (number_animation_type == ANI_ANTICLOCKWISE)
                {
                    switch (number_animation_steps[i])
                    {
                    case 0:
                        code = numbergetpart(raw_code, 0x004880);
                        if (code != 0)
                            break;
                    case 1:
                        code = numbergetpart(raw_code, 0x004ca0);
                        if (code != 0)
                            break;
                    case 2:
                        code = numbergetpart(raw_code, 0x004ef0);
                        if (code != 0)
                            break;
                    case 3:
                        code = numbergetpart(raw_code, 0x006ff0);
                        if (code != 0)
                            break;
                    case 4:
                        code = numbergetpart(raw_code, 0x036ff0);
                        if (code != 0)
                            break;
                    case 5:
                        code = numbergetpart(raw_code, 0x676ff0);
                        if (code != 0)
                            break;
                    case 6:
                        code = numbergetpart(raw_code, 0xef6ff0);
                        if (code != 0)
                            break;
                    case 7:
                        code = numbergetpart(raw_code, 0xffeff0);
                        if (code != 0)
                            break;
                    default:
                        number_animation_steps[i] = 0;
                        break;
                    }
                }
                else if (number_animation_type == ANI_UP2DOWN)
                {
                    switch (number_animation_steps[i])
                    {
                    case 0:
                        code = numbergetpart(raw_code, 0xe00000);
                        if (code != 0)
                            break;
                    case 1:
                        code = numbergetpart(raw_code, 0xff0000);
                        if (code != 0)
                            break;
                    case 2:
                        code = numbergetpart(raw_code, 0xffe000);
                        if (code != 0)
                            break;
                    case 3:
                        code = numbergetpart(raw_code, 0xffff00);
                        if (code != 0)
                            break;
                    default:
                        number_animation_steps[i] = 0;
                        break;
                    }
                }
                else if (number_animation_type == ANI_DOWN2UP)
                {
                    switch (number_animation_steps[i])
                    {
                    case 0:
                        code = numbergetpart(raw_code, 0x0000f0);
                        if (code != 0)
                            break;
                    case 1:
                        code = numbergetpart(raw_code, 0x001ff0);
                        if (code != 0)
                            break;
                    case 2:
                        code = numbergetpart(raw_code, 0x00fff0);
                        if (code != 0)
                            break;
                    case 3:
                        code = numbergetpart(raw_code, 0x1ffff0);
                        if (code != 0)
                            break;
                    default:
                        number_animation_steps[i] = 0;
                        break;
                    }
                }
                else if (number_animation_type == ANI_LEFT2RT)
                {
                    switch (number_animation_steps[i])
                    {
                    case 0:
                        code = numbergetpart(raw_code, 0x901080);
                        if (code != 0)
                            break;
                    case 1:
                        code = numbergetpart(raw_code, 0xd89880);
                        if (code != 0)
                            break;
                    case 2:
                        code = numbergetpart(raw_code, 0xdcdce0);
                        if (code != 0)
                            break;
                    case 3:
                        code = numbergetpart(raw_code, 0xdefee0);
                        if (code != 0)
                            break;
                    default:
                        number_animation_steps[i] = 0;
                        break;
                    }
                }
                else if (number_animation_type == ANI_RT2LEFT)
                {
                    switch (number_animation_steps[i])
                    {
                    case 0:
                        code = numbergetpart(raw_code, 0x210110);
                        if (code != 0)
                            break;
                    case 1:
                        code = numbergetpart(raw_code, 0x632310);
                        if (code != 0)
                            break;
                    case 2:
                        code = numbergetpart(raw_code, 0x676770);
                        if (code != 0)
                            break;
                    case 3:
                        code = numbergetpart(raw_code, 0x6fef70);
                        if (code != 0)
                            break;
                    default:
                        number_animation_steps[i] = 0;
                        break;
                    }
                }
                else
                    number_animation_steps[i] = 0;

                numhelper(i, code);
                number_animation_steps[i]++;
            }
        }
    }

public:
    /**
     * @brief 构造函数，使用给定的 SPI 设备句柄初始化 HNA_16MM65T 对象。
     *
     * @param spi_device SPI 设备句柄。
     */
    HNA_16MM65T(spi_device_handle_t spi_device);

    /**
     * @brief 显示频谱信息。
     *
     * @param buf 频谱数据缓冲区。
     * @param size 缓冲区大小。
     */
    void spectrum_show(float *buf, int size);

    /**
     * @brief 显示String。
     *
     * @param buf String
     * @param size String大小
     */
    void number_show(char *buf, int size, NumAni ani = ANI_CLOCKWISE);

    /**
     * @brief 显示符号信息。
     *
     * @param symbol 要显示的符号枚举值。
     * @param is_on 符号是否显示。
     */
    void symbolhelper(Symbols symbol, bool is_on);

    /**
     * @brief 显示点矩阵信息。
     *
     * @param dot 点矩阵的状态枚举值。
     */
    void dotshelper(Dots dot);

    /**
     * @brief 测试方法，用于测试显示功能。
     */
    void test();

    /**
     * @brief 测试方法，用于测试显示功能。
     */
    void cali();

protected:
    /**
     * @brief 显示数字信息。
     *
     * @param index 数字显示的索引位置。
     * @param ch 要显示的字符。
     */
    void numhelper(int index, char ch);

    /**
     * @brief 显示数字信息。
     *
     * @param index 数字显示的索引位置。
     * @param code 要显示的断码。
     */
    void numhelper(int index, uint32_t code);

    /**
     * @brief 显示波形信息。
     *
     * @param index 波形显示的索引位置。
     * @param level 波形的级别。
     */
    void wavehelper(int index, int level);

    /**
     * @brief 查找字符对应的十六进制代码
     *
     * @param ch 要查找十六进制代码的字符
     * @return unsigned int 字符对应的十六进制代码
     */
    unsigned int find_hex_code(char ch);

    /**
     * @brief 根据符号标志查找对应的枚举代码，并更新字节索引和位索引
     *
     * @param flag 符号标志，用于查找对应的枚举代码
     * @param byteIndex 指向字节索引的指针，函数会更新该指针所指向的值
     * @param bitIndex 指向位索引的指针，函数会更新该指针所指向的值
     */
    void find_enum_code(Symbols flag, int *byteIndex, int *bitIndex);
};

#endif
