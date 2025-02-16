/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PT6324_H_
#define _PT6324_H_

#include "esp_log.h"
#include <driver/spi_master.h>

#define CHAR_COUNT 62
#define NUM_BEGIN 3

typedef enum {
    DOT_MATRIX_UP,
    DOT_MATRIX_NEXT,
    DOT_MATRIX_PAUSE,
    DOT_MATRIX_FILL
} Dots;

typedef enum {
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
    DOT_MATRIX_2_N1_2_7,
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

typedef struct {
    int byteIndex;
    int bitIndex;
} SymbolPosition;

class PT6324Writer
{
public:
    PT6324Writer(spi_device_handle_t spi_device) : spi_device_(spi_device) {}
    
    void pt6324_refrash();
    void pt6324_init();
    void pt6324_test();
    void pt6324_cali();
    void pt6324_numhelper(int index, char ch);
    void pt6324_symbolhelper(Symbols symbol, bool is_on);
    void pt6324_dotshelper(Dots dot);
    void pt6324_wavehelper(int index, int level);
private:
    spi_device_handle_t spi_device_;
    uint8_t gram[48] = {0};
    void pt6324_write_data(uint8_t *dat, int len);
    
};

#endif

// Dots 1:78 2:0 Up
// Dots 1:D0 2:A Next
// Dots 1:B2 2:1 Pause

// 0:1 -> NC
// 0:2 -> R圈 外圈B
// 0:4 -> R圈 外圈A
// 0:8 -> R圈 中心
// 0:10 -> L圈 外圈B
// 0:20 -> L圈 外圈A
// 0:40 -> L圈 中心
// 0:80 -> STEREO
// 1:1 -> MONO
// 1:2 -> GIGA
// 1:4 -> REC
// 1:8 -> 点阵 4,6 
// 1:10 -> 点阵 5,2 5,3 6,3
// 1:20 -> 点阵 0,3 0,5 0,6 1,2 1,3 1,5 1,6
// 1:40 -> 点阵 3,1 3,2 3,3 3,5 3,6 4,0 4,1 4,2 4,3 4,5 4,6 5,1 5,2 5,3 5,5
// 1:80 -> 点阵 5,4
// 2:1 -> 点阵 0,0 0,1 0,2 0,3 0,5 1,0 1,1 1,3 1,5 5,0 5,1 6,0 6,1 6,2 6,5
// 2:2 -> 点阵 2,0 2,4 3,4 4,4
// 2:4 -> 点阵 4,0
// 2:8 -> 点阵 2,-1 2,7
// 2:10 -> USB2
// 2:20 -> USB1
// 2:40 -> REC
// 2:80 -> Lbar Rbar
// 3:1 -> NC
// 3:2 -> NC
// 3:4 -> NC
// 3:8 -> NC
// 3:10 -> Num1:RB (Right-Bottom)
// 3:20 -> Num1:MB (Middle-Bottom)
// 3:40 -> Num1:B (Bottom)
// 3:80 -> Num1:LB (Left-Bottom)
// 4:1 -> Num1:RD (Right-Down)
// 4:2 -> Num1:RMD (Right-Middle-Down)
// 4:4 -> Num1:MD (Middle-Down)
// 4:8 -> Num1:LMD (Left-Middle-Down)
// 4:10 -> Num1:LD (Left-Down)
// 4:20 -> Num1:RM (Right-Middle)
// 4:40 -> Num1:M (Middle)
// 4:80 -> Num1:LM (Left-Middle)
// 5:1 -> Num1:RU (Right-Up)
// 5:2 -> Num1:RMU (Right-Middle-Up)
// 5:4 -> Num1:MU (Middle-Up)
// 5:8 -> Num1:LMU (Left-Middle-Up)
// 5:10 -> Num1:LU (Left-Up)
// 5:20 -> Num1:RT (Right-Top)
// 5:40 -> Num1:T (Top)
// 5:80 -> Num1:LT (Left-Top)
// 6:1 -> NC
// 6:2 -> NC
// 6:4 -> NC
// 6:8 -> NC
// 6:10 -> Num2:RB (Right-Bottom)
// 6:20 -> Num2:MB (Middle-Bottom)
// 6:40 -> Num2:B (Bottom)
// 6:80 -> Num2:LB (Left-Bottom)
// 7:1 -> Num2:RD (Right-Down)
// 7:2 -> Num2:RMD (Right-Middle-Down)
// 7:4 -> Num2:MD (Middle-Down)
// 7:8 -> Num2:LMD (Left-Middle-Down)
// 7:10 -> Num2:LD (Left-Down)
// 7:20 -> Num2:RM (Right-Middle)
// 7:40 -> Num2:M (Middle)
// 7:80 -> Num2:LM (Left-Middle)
// 8:1 -> Num2:RU (Right-Up)
// 8:2 -> Num2:RMU (Right-Middle-Up)
// 8:4 -> Num2:MU (Middle-Up)
// 8:8 -> Num2:LMU (Left-Middle-Up)
// 8:10 -> Num2:LU (Left-Up)
// 8:20 -> Num2:RT (Right-Top)
// 8:40 -> Num2:T (Top)
// 8:80 -> Num2:LT (Left-Top)
// 9:1 -> NC
// 9:2 -> NC
// 9:4 -> NC
// 9:8 -> NC
// 9:10 -> Num3:RB (Right-Bottom)
// 9:20 -> Num3:MB (Middle-Bottom)
// 9:40 -> Num3:B (Bottom)
// 9:80 -> Num3:LB (Left-Bottom)
// 10:1 -> Num3:RD (Right-Down)
// 10:2 -> Num3:RMD (Right-Middle-Down)
// 10:4 -> Num3:MD (Middle-Down)
// 10:8 -> Num3:LMD (Left-Middle-Down)
// 10:10 -> Num3:LD (Left-Down)
// 10:20 -> Num3:RM (Right-Middle)
// 10:40 -> Num3:M (Middle)
// 10:80 -> Num3:LM (Left-Middle)
// 11:1 -> Num3:RU (Right-Up)
// 11:2 -> Num3:RMU (Right-Middle-Up)
// 11:4 -> Num3:MU (Middle-Up)
// 11:8 -> Num3:LMU (Left-Middle-Up)
// 11:10 -> Num3:LU (Left-Up)
// 11:20 -> Num3:RT (Right-Top)
// 11:40 -> Num3:T (Top)
// 11:80 -> Num3:LT (Left-Top)
// 12:1 -> NC
// 12:2 -> NC
// 12:4 -> NC
// 12:8 -> NC
// 12:10 -> Num4:RB (Right-Bottom)
// 12:20 -> Num4:MB (Middle-Bottom)
// 12:40 -> Num4:B (Bottom)
// 12:80 -> Num4:LB (Left-Bottom)
// 13:1 -> Num4:RD (Right-Down)
// 13:2 -> Num4:RMD (Right-Middle-Down)
// 13:4 -> Num4:MD (Middle-Down)
// 13:8 -> Num4:LMD (Left-Middle-Down)
// 13:10 -> Num4:LD (Left-Down)
// 13:20 -> Num4:RM (Right-Middle)
// 13:40 -> Num4:M (Middle)
// 13:80 -> Num4:LM (Left-Middle)
// 14:1 -> Num4:RU (Right-Up)
// 14:2 -> Num4:RMU (Right-Middle-Up)
// 14:4 -> Num4:MU (Middle-Up)
// 14:8 -> Num4:LMU (Left-Middle-Up)
// 14:10 -> Num4:LU (Left-Up)
// 14:20 -> Num4:RT (Right-Top)
// 14:40 -> Num4:T (Top)
// 14:80 -> Num4:LT (Left-Top)
// 15:1 -> NC
// 15:2 -> NC
// 15:4 -> NC
// 15:8 -> NC
// 15:10 -> Num5:RB (Right-Bottom)
// 15:20 -> Num5:MB (Middle-Bottom)
// 15:40 -> Num5:B (Bottom)
// 15:80 -> Num5:LB (Left-Bottom)
// 16:1 -> Num5:RD (Right-Down)
// 16:2 -> Num5:RMD (Right-Middle-Down)
// 16:4 -> Num5:MD (Middle-Down)
// 16:8 -> Num5:LMD (Left-Middle-Down)
// 16:10 -> Num5:LD (Left-Down)
// 16:20 -> Num5:RM (Right-Middle)
// 16:40 -> Num5:M (Middle)
// 16:80 -> Num5:LM (Left-Middle)
// 17:1 -> Num5:RU (Right-Up)
// 17:2 -> Num5:RMU (Right-Middle-Up)
// 17:4 -> Num5:MU (Middle-Up)
// 17:8 -> Num5:LMU (Left-Middle-Up)
// 17:10 -> Num5:LU (Left-Up)
// 17:20 -> Num5:RT (Right-Top)
// 17:40 -> Num5:T (Top)
// 17:80 -> Num5:LT (Left-Top)
// 18:1 -> NC
// 18:2 -> NC
// 18:4 -> NC
// 18:8 -> NC
// 18:10 -> Num6:RB (Right-Bottom)
// 18:20 -> Num6:MB (Middle-Bottom)
// 18:40 -> Num6:B (Bottom)
// 18:80 -> Num6:LB (Left-Bottom)
// 19:1 -> Num6:RD (Right-Down)
// 19:2 -> Num6:RMD (Right-Middle-Down)
// 19:4 -> Num6:MD (Middle-Down)
// 19:8 -> Num6:LMD (Left-Middle-Down)
// 19:10 -> Num6:LD (Left-Down)
// 19:20 -> Num6:RM (Right-Middle)
// 19:40 -> Num6:M (Middle)
// 19:80 -> Num6:LM (Left-Middle)
// 20:1 -> Num6:RU (Right-Up)
// 20:2 -> Num6:RMU (Right-Middle-Up)
// 20:4 -> Num6:MU (Middle-Up)
// 20:8 -> Num6:LMU (Left-Middle-Up)
// 20:10 -> Num6:LU (Left-Up)
// 20:20 -> Num6:RT (Right-Top)
// 20:40 -> Num6:T (Top)
// 20:80 -> Num6:LT (Left-Top)
// 21:1 -> NC
// 21:2 -> NC
// 21:4 -> NC
// 21:8 -> NC
// 21:10 -> Num7:RB (Right-Bottom)
// 21:20 -> Num7:MB (Middle-Bottom)
// 21:40 -> Num7:B (Bottom)
// 21:80 -> Num7:LB (Left-Bottom)
// 22:1 -> Num7:RD (Right-Down)
// 22:2 -> Num7:RMD (Right-Middle-Down)
// 22:4 -> Num7:MD (Middle-Down)
// 22:8 -> Num7:LMD (Left-Middle-Down)
// 22:10 -> Num7:LD (Left-Down)
// 22:20 -> Num7:RM (Right-Middle)
// 22:40 -> Num7:M (Middle)
// 22:80 -> Num7:LM (Left-Middle)
// 23:1 -> Num7:RU (Right-Up)
// 23:2 -> Num7:RMU (Right-Middle-Up)
// 23:4 -> Num7:MU (Middle-Up)
// 23:8 -> Num7:LMU (Left-Middle-Up)
// 23:10 -> Num7:LU (Left-Up)
// 23:20 -> Num7:RT (Right-Top)
// 23:40 -> Num7:T (Top)
// 23:80 -> Num7:LT (Left-Top)
// 24:1 -> NC
// 24:2 -> NC
// 24:4 -> NC
// 24:8 -> NC
// 24:10 -> Num8:RB (Right-Bottom)
// 24:20 -> Num8:MB (Middle-Bottom)
// 24:40 -> Num8:B (Bottom)
// 24:80 -> Num8:LB (Left-Bottom)
// 25:1 -> Num8:RD (Right-Down)
// 25:2 -> Num8:RMD (Right-Middle-Down)
// 25:4 -> Num8:MD (Middle-Down)
// 25:8 -> Num8:LMD (Left-Middle-Down)
// 25:10 -> Num8:LD (Left-Down)
// 25:20 -> Num8:RM (Right-Middle)
// 25:40 -> Num8:M (Middle)
// 25:80 -> Num8:LM (Left-Middle)
// 26:1 -> Num8:RU (Right-Up)
// 26:2 -> Num8:RMU (Right-Middle-Up)
// 26:4 -> Num8:MU (Middle-Up)
// 26:8 -> Num8:LMU (Left-Middle-Up)
// 26:10 -> Num8:LU (Left-Up)
// 26:20 -> Num8:RT (Right-Top)
// 26:40 -> Num8:T (Top)
// 26:80 -> Num8:LT (Left-Top)
// 27:1 -> NC
// 27:2 -> NC
// 27:4 -> NC
// 27:8 -> NC
// 27:10 -> Num9:RB (Right-Bottom)
// 27:20 -> Num9:MB (Middle-Bottom)
// 27:40 -> Num9:B (Bottom)
// 27:80 -> Num9:LB (Left-Bottom)
// 28:1 -> Num9:RD (Right-Down)
// 28:2 -> Num9:RMD (Right-Middle-Down)
// 28:4 -> Num9:MD (Middle-Down)
// 28:8 -> Num9:LMD (Left-Middle-Down)
// 28:10 -> Num9:LD (Left-Down)
// 28:20 -> Num9:RM (Right-Middle)
// 28:40 -> Num9:M (Middle)
// 28:80 -> Num9:LM (Left-Middle)
// 29:1 -> Num9:RU (Right-Up)
// 29:2 -> Num9:RMU (Right-Middle-Up)
// 29:4 -> Num9:MU (Middle-Up)
// 29:8 -> Num9:LMU (Left-Middle-Up)
// 29:10 -> Num9:LU (Left-Up)
// 29:20 -> Num9:RT (Right-Top)
// 29:40 -> Num9:T (Top)
// 29:80 -> Num9:LT (Left-Top)
// 30:1 -> NC
// 30:2 -> NC
// 30:4 -> NC
// 30:8 -> NC
// 30:10 -> Num10:RB (Right-Bottom)
// 30:20 -> Num10:MB (Middle-Bottom)
// 30:40 -> Num10:B (Bottom)
// 30:80 -> Num10:LB (Left-Bottom)
// 31:1 -> Num10:RD (Right-Down)
// 31:2 -> Num10:RMD (Right-Middle-Down)
// 31:4 -> Num10:MD (Middle-Down)
// 31:8 -> Num10:LMD (Left-Middle-Down)
// 31:10 -> Num10:LD (Left-Down)
// 31:20 -> Num10:RM (Right-Middle)
// 31:40 -> Num10:M (Middle)
// 31:80 -> Num10:LM (Left-Middle)
// 32:1 -> Num10:RU (Right-Up)
// 32:2 -> Num10:RMU (Right-Middle-Up)
// 32:4 -> Num10:MU (Middle-Up)
// 32:8 -> Num10:LMU (Left-Middle-Up)
// 32:10 -> Num10:LU (Left-Up)
// 32:20 -> Num10:RT (Right-Top)
// 32:40 -> Num10:T (Top)
// 32:80 -> Num10:LT (Left-Top)
// 33:1 -> NC
// 33:2 -> NC
// 33:4 -> Wave3-1
// 33:8 -> Wave2-1
// 33:10 -> Wave1-1
// 33:20 -> Wave3-2
// 33:40 -> Wave2-2
// 33:80 -> Wave1-2
// 34:1 -> Wave3-3
// 34:2 -> Wave2-3
// 34:4 -> Wave1-3
// 34:8 -> Wave3-4
// 34:10 -> Wave2-4
// 34:20 -> Wave1-4
// 34:40 -> Wave3-5
// 34:80 -> Wave2-5
// 35:1 -> Wave1-5
// 35:2 -> Wave3-6
// 35:4 -> Wave2-6
// 35:8 -> Wave1-6
// 35:10 -> Wave3-7
// 35:20 -> Wave2-7
// 35:40 -> Wave1-7
// 35:80 -> Wave3-8 Wave2-8 Wave1-8 RedWave L(Left)
// 36:1 -> NC
// 36:2 -> NC
// 36:4 -> Wave6-1
// 36:8 -> Wave5-1
// 36:10 -> Wave4-1
// 36:20 -> Wave6-2
// 36:40 -> Wave5-2
// 36:80 -> Wave4-2
// 37:1 -> Wave6-3
// 37:2 -> Wave5-3
// 37:4 -> Wave4-3
// 37:8 -> Wave6-4
// 37:10 -> Wave5-4
// 37:20 -> Wave4-4
// 37:40 -> Wave6-5
// 37:80 -> Wave5-5
// 38:1 -> Wave4-5
// 38:2 -> Wave6-6
// 38:4 -> Wave5-6
// 38:8 -> Wave4-6
// 38:10 -> Wave6-7
// 38:20 -> Wave5-7
// 38:40 -> Wave4-7
// 38:80 -> Wave6-8 Wave5-8 Wave4-8 RedWave LM(Left-Middle)
// 39:1 -> Center-OutLay-BlueA
// 39:2 -> Center-OutLay-BlueB
// 39:4 -> Center-OutLay-RedA
// 39:8 -> Center-OutLay-RedB
// 39:10 -> Center-Inlay-BlueR
// 39:20 -> Center-Inlay-BlueT
// 39:40 -> Center-Inlay-BlueL
// 39:80 -> Center-Inlay-BlueB
// 40:1 -> Center-Inlay-Red1
// 40:2 -> Center-Inlay-Red2
// 40:4 -> Center-Inlay-Red3
// 40:8 -> Center-Inlay-Red4
// 40:10 -> Center-Inlay-Red5
// 40:20 -> Center-Inlay-Red6
// 40:40 -> Center-Inlay-Red7
// 40:80 -> Center-Inlay-Red8
// 41:1 -> Center-Inlay-Red9
// 41:2 -> Center-Inlay-Red10
// 41:4 -> Center-Inlay-Red11
// 41:8 -> Center-Inlay-Red12
// 41:10 -> Center-Inlay-Red13
// 41:20 -> Center-Inlay-Red14
// 41:40 -> Center-Inlay-Red15
// 41:80 -> Center-Inlay-Red16
// 42:1 -> NC
// 42:2 -> NC
// 42:4 -> Wave7-1
// 42:8 -> Wave8-1
// 42:10 -> Wave9-1
// 42:20 -> Wave7-2
// 42:40 -> Wave8-2
// 42:80 -> Wave9-2
// 43:1 -> Wave7-3
// 43:2 -> Wave8-3
// 43:4 -> Wave9-3
// 43:8 -> Wave7-4
// 43:10 -> Wave8-4
// 43:20 -> Wave9-4
// 43:40 -> Wave7-5
// 43:80 -> Wave8-5
// 44:1 -> Wave9-5
// 44:2 -> Wave7-6
// 44:4 -> Wave8-6
// 44:8 -> Wave9-6
// 44:10 -> Wave7-7
// 44:20 -> Wave8-7
// 44:40 -> Wave9-7
// 44:80 -> Wave7-8 Wave8-8 Wave9-8 RedWave RM(Right-Middle)
// 45:1 -> NC
// 45:2 -> NC
// 45:4 -> Wave10-1
// 45:8 -> Wave11-1
// 45:10 -> Wave12-1
// 45:20 -> Wave10-2
// 45:40 -> Wave11-2
// 45:80 -> Wave12-2
// 46:1 -> Wave10-3
// 46:2 -> Wave11-3
// 46:4 -> Wave12-3
// 46:8 -> Wave10-4
// 46:10 -> Wave11-4
// 46:20 -> Wave12-4
// 46:40 -> Wave10-5
// 46:80 -> Wave11-5
// 47:1 -> Wave12-5
// 47:2 -> Wave10-6
// 47:4 -> Wave11-6
// 47:8 -> Wave12-6
// 47:10 -> Wave10-7
// 47:20 -> Wave11-7
// 47:40 -> Wave12-7
// 47:80 -> Wave10-8 Wave11-8 Wave12-8 RedWave R(Right)