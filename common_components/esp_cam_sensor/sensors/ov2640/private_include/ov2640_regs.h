/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * This file is part of the OpenMV project.
 * Copyright (c) 2013/2014 Ibrahim Abdelkader <i.abdalkader@gmail.com>
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * OV2640 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* DSP register bank FF=0x00*/
#define R_BYPASS            0x05
#define QS                  0x44
#define CTRLI               0x50
#define HSIZE               0x51
#define VSIZE               0x52
#define XOFFL               0x53
#define YOFFL               0x54
#define VHYX                0x55
#define DPRP                0x56
#define TEST                0x57
#define ZMOW                0x5A
#define ZMOH                0x5B
#define ZMHH                0x5C
#define BPADDR              0x7C
#define BPDATA              0x7D
#define CTRL2               0x86
#define CTRL3               0x87
#define SIZEL               0x8C
#define HSIZE8              0xC0
#define VSIZE8              0xC1
#define CTRL0               0xC2
#define CTRL1               0xC3
#define R_DVP_SP            0xD3
#define IMAGE_MODE          0xDA
#define RESET               0xE0
#define MS_SP               0xF0
#define SS_ID               0xF7
#define SS_CTRL             0xF7
#define MC_BIST             0xF9
#define MC_AL               0xFA
#define MC_AH               0xFB
#define MC_D                0xFC
#define P_CMD               0xFD
#define P_STATUS            0xFE
#define BANK_SEL            0xFF
#define REG_DELAY           0X00

#define CTRLI_LP_DP         0x80
#define CTRLI_ROUND         0x40

#define CTRL0_AEC_EN        0x80
#define CTRL0_AEC_SEL       0x40
#define CTRL0_STAT_SEL      0x20
#define CTRL0_VFIRST        0x10
#define CTRL0_YUV422        0x08
#define CTRL0_YUV_EN        0x04
#define CTRL0_RGB_EN        0x02
#define CTRL0_RAW_EN        0x01

#define CTRL2_DCW_EN        0x20
#define CTRL2_SDE_EN        0x10
#define CTRL2_UV_ADJ_EN     0x08
#define CTRL2_UV_AVG_EN     0x04
#define CTRL2_CMX_EN        0x01

#define CTRL3_BPC_EN        0x80
#define CTRL3_WPC_EN        0x40

#define R_DVP_SP_AUTO_MODE  0x80

#define R_BYPASS_DSP_EN         0x00
#define R_BYPASS_DSP_BYPAS      0x01

#define IMAGE_MODE_Y8_DVP_EN    0x40
#define IMAGE_MODE_JPEG_EN      0x10
#define IMAGE_MODE_YUV422       0x01 // YUV422 format and byte swap enabled
#define IMAGE_MODE_RAW10        0x04
#define IMAGE_MODE_RGB565       0x09 // RGB565 format and byte swap enabled
#define IMAGE_MODE_HREF_VSYNC   0x02
#define IMAGE_MODE_LBYTE_FIRST  0x01

#define RESET_MICROC            0x40
#define RESET_SCCB              0x20
#define RESET_JPEG              0x10
#define RESET_DVP               0x04
#define RESET_IPU               0x02
#define RESET_CIF               0x01

#define MC_BIST_RESET           0x80
#define MC_BIST_BOOT_ROM_SEL    0x40
#define MC_BIST_12KB_SEL        0x20
#define MC_BIST_12KB_MASK       0x30
#define MC_BIST_512KB_SEL       0x08
#define MC_BIST_512KB_MASK      0x0C
#define MC_BIST_BUSY_BIT_R      0x02
#define MC_BIST_MC_RES_ONE_SH_W 0x02
#define MC_BIST_LAUNCH          0x01

/* Sensor register bank FF=0x01*/
#define GAIN                0x00
#define COM1                0x03
#define REG04               0x04
#define REG08               0x08
#define COM2                0x09
#define REG_PID             0x0A
#define REG_VER             0x0B
#define COM3                0x0C
#define COM4                0x0D
#define AEC                 0x10
#define CLKRC               0x11
#define COM7                0x12
#define COM8                0x13
#define COM9                0x14 /* AGC gain ceiling */
#define COM10               0x15
#define HSTART              0x17
#define HSTOP               0x18
#define VSTART              0x19
#define VSTOP               0x1A
#define REG_MIDH            0x1C
#define REG_MIDL            0x1D
#define AEW                 0x24
#define AEB                 0x25
#define VV                  0x26
#define REG2A               0x2A
#define FRARL               0x2B
#define ADDVSL              0x2D
#define ADDVSH              0x2E
#define YAVG                0x2F
#define HSDY                0x30
#define HEDY                0x31
#define REG32               0x32
#define ARCOM2              0x34
#define REG45               0x45
#define FLL                 0x46
#define FLH                 0x47
#define COM19               0x48
#define ZOOMS               0x49
#define COM22               0x4B
#define COM25               0x4E
#define BD50                0x4F
#define BD60                0x50
#define REG5D               0x5D
#define REG5E               0x5E
#define REG5F               0x5F
#define REG60               0x60
#define HISTO_LOW           0x61
#define HISTO_HIGH          0x62

#define REG04_DEFAULT       0x28
#define REG04_HFLIP_IMG     0x80
#define REG04_VFLIP_IMG     0x40
#define REG04_VREF_EN       0x10
#define REG04_HREF_EN       0x08
#define REG04_SET(x)        (REG04_DEFAULT|x)

#define COM2_STDBY          0x10
#define COM2_OUT_DRIVE_1x   0x00
#define COM2_OUT_DRIVE_2x   0x01
#define COM2_OUT_DRIVE_3x   0x02
#define COM2_OUT_DRIVE_4x   0x03

#define COM3_DEFAULT        0x38
#define COM3_BAND_50Hz      0x04
#define COM3_BAND_60Hz      0x00
#define COM3_BAND_AUTO      0x02
#define COM3_BAND_SET(x)    (COM3_DEFAULT|x)

#define COM7_SRST           0x80 /* Init system reset */
#define COM7_RES_UXGA       0x00 /* UXGA */
#define COM7_RES_SVGA       0x40 /* SVGA */
#define COM7_RES_CIF        0x20 /* CIF  */
#define COM7_ZOOM_EN        0x04 /* Enable Zoom */
#define COM7_COLOR_BAR      0x02 /* Enable Color Bar Test */

#define COM8_DEFAULT        0xC0
#define COM8_BNDF_EN        0x20 /* Enable Banding filter */
#define COM8_AGC_EN         0x04 /* AGC Auto/Manual control selection */
#define COM8_AEC_EN         0x01 /* Auto/Manual Exposure control */
#define COM8_SET(x)         (COM8_DEFAULT|x)

#define COM9_DEFAULT        0x08
#define COM9_AGC_GAIN_2x    0x00 /* AGC:    2x */
#define COM9_AGC_GAIN_4x    0x01 /* AGC:    4x */
#define COM9_AGC_GAIN_8x    0x02 /* AGC:    8x */
#define COM9_AGC_GAIN_16x   0x03 /* AGC:   16x */
#define COM9_AGC_GAIN_32x   0x04 /* AGC:   32x */
#define COM9_AGC_GAIN_64x   0x05 /* AGC:   64x */
#define COM9_AGC_GAIN_128x  0x06 /* AGC:  128x */
#define COM9_AGC_SET(x)     (COM9_DEFAULT|(x<<5))

#define COM10_HREF_EN       0x80 /* HSYNC changes to HREF */
#define COM10_HSYNC_EN      0x40 /* HREF changes to HSYNC */
#define COM10_PCLK_FREE     0x20 /* PCLK output option: free running PCLK */
#define COM10_PCLK_EDGE     0x10 /* Data is updated at the rising edge of PCLK */
#define COM10_HREF_NEG      0x08 /* HREF negative */
#define COM10_VSYNC_NEG     0x02 /* VSYNC negative */
#define COM10_HSYNC_NEG     0x01 /* HSYNC negative */

#define CTRL1_AWB           0x08 /* Enable AWB */

#define VV_AGC_TH_SET(h,l)  ((h<<4)|(l&0x0F))

#define REG32_UXGA          0x36
#define REG32_SVGA          0x09
#define REG32_CIF           0x89

#define CLKRC_2X            0x80
#define CLKRC_2X_UXGA       (0x01 | CLKRC_2X)
#define CLKRC_2X_SVGA       CLKRC_2X
#define CLKRC_2X_CIF        CLKRC_2X

#ifdef __cplusplus
}
#endif
