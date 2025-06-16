/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SC202CS register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SC202CS_REG_END      0xffff
#define SC202CS_REG_DELAY    0xfffe

/* sc202cs registers */
#define SC202CS_REG_SENSOR_ID_H             0x3107
#define SC202CS_REG_SENSOR_ID_L             0x3108

#define SC202CS_REG_GROUP_HOLD              0x3812

#define SC202CS_REG_DIG_COARSE_GAIN         0x3e06
#define SC202CS_REG_DIG_FINE_GAIN           0x3e07
#define SC202CS_REG_ANG_GAIN                0x3e09

#define SC202CS_REG_SHUTTER_TIME_H          0x3e00
#define SC202CS_REG_SHUTTER_TIME_M          0x3e01
#define SC202CS_REG_SHUTTER_TIME_L          0x3e02

#define SC202CS_REG_TOTAL_WIDTH_H           0x320c // HTS,line width
#define SC202CS_REG_TOTAL_WIDTH_L           0x320d
#define SC202CS_REG_TOTAL_HEIGHT_H          0x320e // VTS,frame height
#define SC202CS_REG_TOTAL_HEIGHT_L          0x320f

#define SC202CS_REG_OUT_WIDTH_H             0x3208 // width
#define SC202CS_REG_OUT_WIDTH_L             0x3209
#define SC202CS_REG_OUT_HEIGHT_H            0x320a // height
#define SC202CS_REG_OUT_HEIGHT_L            0x320b

#define SC202CS_REG_OUT_START_PIXEL_H       0x3210 // start X
#define SC202CS_REG_OUT_START_PIXEL_L       0x3211
#define SC202CS_REG_OUT_START_LINE_H        0x3212 // start Y
#define SC202CS_REG_OUT_START_LINE_L        0x3213

#define SC202CS_REG_FLIP_MIRROR             0x3221
#define SC202CS_REG_SLEEP_MODE              0x0100

#ifdef __cplusplus
}
#endif
