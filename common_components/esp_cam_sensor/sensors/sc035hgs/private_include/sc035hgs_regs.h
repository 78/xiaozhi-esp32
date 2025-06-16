/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SC035HGS register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* sc035hgs registers */
#define SC035HGS_REG_ID_HIGH         0x3107
#define SC035HGS_REG_ID_LOW          0x3108

#define SC035HGS_REG_DELAY           0xfefe
#define SC035HGS_REG_SLEEP_MODE      0x0100

#define SC035HGS_REG_GROUP_HOLD      0x3812
#define SC035HGS_REG_BLC_CTRL0       0x3900
#define SC035HGS_REG_BLC_CTRL1       0x3902

#define SC035HGS_REG_SHUTTER_TIME_H  0x3e01
#define SC035HGS_REG_SHUTTER_TIME_L  0x3e02

#define SC035HGS_REG_COARSE_DGAIN    0x3e06
#define SC035HGS_REG_FINE_DGAIN      0x3e07
#define SC035HGS_REG_COARSE_AGAIN    0x3e08
#define SC035HGS_REG_FINE_AGAIN      0x3e09

#ifdef __cplusplus
}
#endif
