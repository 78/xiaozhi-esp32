/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * OV2710 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define OV2710_REG_END      0xffff
#define OV2710_REG_DELAY    0xfffe

/* ov2710 registers */
#define OV2710_REG_SENSOR_ID_H             0x300a
#define OV2710_REG_SENSOR_ID_L             0x300b

#define OV2710_REG_RED_BEFORE_GAIN_AVERAGE      0x5196  /* R Bit[7:0]: Before AWB gain's red data average*/
#define OV2710_REG_GREEN_BEFORE_GAIN_AVERAGE    0x5197  /* R Bit[7:0]: Before AWB gain's green data average*/
#define OV2710_REG_BLUE_BEFORE_GAIN_AVERAGE     0x5198  /* R Bit[7:0]: Before AWB gain's blue data average*/
#define OV2710_REG_AEC_AGC_ADJ_MSB              0x350A
#define OV2710_REG_AEC_AGC_ADJ_LSB              0x350B

#ifdef __cplusplus
}
#endif
