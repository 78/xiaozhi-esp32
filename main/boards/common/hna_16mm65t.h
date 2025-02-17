#ifndef _HNA_16MM65T_H_
#define _HNA_16MM65T_H_

#include "pt6324.h"
#include <cmath>
#include <esp_wifi.h>

#define CHAR_COUNT 62
#define NUM_BEGIN 3

typedef enum
{
    DOT_MATRIX_UP,
    DOT_MATRIX_NEXT,
    DOT_MATRIX_PAUSE,
    DOT_MATRIX_FILL
} Dots;

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
    SYMBOL_MAX
} Symbols;

typedef struct
{
    int byteIndex;
    int bitIndex;
} SymbolPosition;

class HNA_16MM65T : public PT6324Writer
{
#define BUF_SIZE (1024)
#define FFT_SIZE (12)
private:
    uint8_t gram[48] = {0};
    int last_values[FFT_SIZE] = {0};
    int target_values[FFT_SIZE] = {0};
    int current_values[FFT_SIZE] = {0};
    int animation_steps[FFT_SIZE] = {0};
    int total_steps = 20; // 动画总步数

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
    HNA_16MM65T(spi_device_handle_t spi_device);
    void spectrum_show(float *buf, int size);
    void test();
    void cali();
    void numhelper(int index, char ch);
    void symbolhelper(Symbols symbol, bool is_on);
    void dotshelper(Dots dot);
    void wavehelper(int index, int level);
};

#endif