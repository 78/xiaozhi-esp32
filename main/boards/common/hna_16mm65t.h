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
#define CHAR_COUNT 62
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
    // 定义缓冲区大小
#define BUF_SIZE (1024)
    // 定义 FFT 大小
#define FFT_SIZE (12)

private:
    uint8_t gram[48] = {0};              // 显示缓冲区
    int last_values[FFT_SIZE] = {0};     // 上一次的 FFT 值
    int target_values[FFT_SIZE] = {0};   // 目标 FFT 值
    int current_values[FFT_SIZE] = {0};  // 当前 FFT 值
    int animation_steps[FFT_SIZE] = {0}; // 动画步数
    int total_steps = 20;                // 动画总步数

    /**
     * @brief 执行动画效果，更新显示缓冲区。
     *
     * 使用指数衰减函数计算当前值，并调用 wavehelper 方法更新显示。
     */
    void animate()
    {
        for (int i = 0; i < FFT_SIZE; i++)
        {
            if (animation_steps[i] < total_steps)
            {
                // 使用指数衰减函数计算当前值
                float progress = static_cast<float>(animation_steps[i]) / total_steps;
                float factor = 1 - std::exp(-3 * progress); // 指数衰减因子
                current_values[i] = last_values[i] + static_cast<int>((target_values[i] - last_values[i]) * factor);
                wavehelper(i, current_values[i] * 8 / 90);
                animation_steps[i]++;
            }
            else
            {
                last_values[i] = target_values[i];
                wavehelper(i, target_values[i] * 8 / 90);
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
     * @brief 显示数字信息。
     *
     * @param index 数字显示的索引位置。
     * @param ch 要显示的字符。
     */
    void numhelper(int index, char ch);

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
     * @brief 显示波形信息。
     *
     * @param index 波形显示的索引位置。
     * @param level 波形的级别。
     */
    void wavehelper(int index, int level);

    /**
     * @brief 测试方法，用于测试显示功能。
     */
    void test();
};

#endif