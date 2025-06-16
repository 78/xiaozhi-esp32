/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * GC2145 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* gc2145 registers */
#define GC2145_REG_DELAY          0xff
#define GC2145_REG_CHIP_ID_HIGH   0xf0
#define GC2145_REG_CHIP_ID_LOW    0xf1
#define GC2145_REG_PLL_MODE1      0xf7
#define GC2145_REG_PLL_MODE2      0xf8
#define GC2145_REG_CM_MODE        0xf9
#define GC2145_REG_CLK_DIV_MODE   0xfa
#define GC2145_REG_RESET_RELATED  0xfe    // Bit[7]: Software reset
// Bit[6]: cm reset
// Bit[5]: mipi reset
// Bit[4]: CISCTL_restart_n
// Bit[3]: NA
// Bit[2:0]: page select
//  000:page0
//  001:page1
//  010:page2
//  011:page3

//-page0----------------

#define GC2145_REG_P0_EXPOSURE_HIGH 0x03
#define GC2145_REG_P0_EXPOSURE_LOW 0x04
#define GC2145_REG_P0_HB_HIGH 0x05
#define GC2145_REG_P0_HB_LOW 0x06
#define GC2145_REG_P0_VB_HIGH 0x07
#define GC2145_REG_P0_VB_LOW 0x08
#define GC2145_REG_P0_ROW_START_HIGH 0x09
#define GC2145_REG_P0_ROW_START_LOW 0x0a
#define GC2145_REG_P0_COL_START_HIGH 0x0b
#define GC2145_REG_P0_COL_START_LOW 0x0c

#define GC2145_REG_P0_WIN_HEIGHT_HIGH 0x0d
#define GC2145_REG_P0_WIN_HEIGHT_LOW 0x0e
#define GC2145_REG_P0_WIN_WIDTH_HIGH 0x0f
#define GC2145_REG_P0_WIN_WIDTH_LOW 0x10
#define GC2145_REG_P0_ANALOG_MODE1 0x17
#define GC2145_REG_P0_ANALOG_MODE2 0x18

#define GC2145_REG_P0_SPECIAL_EFFECT 0x83
#define GC2145_REG_P0_OUTPUT_FORMAT 0x84 // Format select
// Bit[7]:YUV420 row switch
// Bit[6]:YUV420 col switch
// Bit[7]:YUV420_legacy
// Bit[4:0]:output data mode
//  5’h00 Cb Y Cr Y
//  5’h01 Cr Y Cb Y
//  5’h02 Y Cb Y Cr
//  5’h03 Y Cr Y Cb
//  5’h04 LSC bypass, C/Y
//  5’h05 LSC bypass, Y/C
//  5’h06 RGB 565
//  5’h0f bypass 10bits
//  5’h17 switch odd/even column /row to controls output Bayer pattern
//    00 RGBG
//    01 RGGB
//    10 BGGR
//    11 GBRG
//  5'h18 DNDD out mode
//  5'h19 LSC out mode
//  5;h1b EEINTP out mode
#define GC2145_REG_P0_FRAME_START 0x85
#define GC2145_REG_P0_SYNC_MODE 0x86
#define GC2145_REG_P0_MODULE_GATING 0x88
#define GC2145_REG_P0_BYPASS_MODE 0x89
#define GC2145_REG_P0_DEBUG_MODE2 0x8c
#define GC2145_REG_P0_DEBUG_MODE3 0x8d
#define GC2145_REG_P0_CROP_ENABLE 0x90
#define GC2145_REG_P0_OUT_WIN_Y1_HIGH 0x91
#define GC2145_REG_P0_OUT_WIN_Y1_LOW 0x92
#define GC2145_REG_P0_OUT_WIN_X1_HIGH 0x93
#define GC2145_REG_P0_OUT_WIN_X1_LOW 0x94
#define GC2145_REG_P0_OUT_WIN_HEIGHT_HIGH 0x95
#define GC2145_REG_P0_OUT_WIN_HEIGHT_LOW 0x96
#define GC2145_REG_P0_OUT_WIN_WIDTH_HIGH 0x97
#define GC2145_REG_P0_OUT_WIN_WIDTH_LOW 0x98
#define GC2145_REG_P0_SUBSAMPLE 0x99
#define GC2145_REG_P0_SUBSAMPLE_MODE 0x9a

#ifdef __cplusplus
}
#endif
