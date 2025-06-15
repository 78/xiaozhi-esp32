/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define OV5647_REG_DELAY            0xeeee
#define OV5647_REG_END              0xffff
#define OV5647_REG_SENSOR_ID_H      0x300a
#define OV5647_REG_SENSOR_ID_L      0x300b
#define OV5647_REG_SLEEP_MODE       0x0100
#define OV5647_REG_MIPI_CTRL00      0x4800
#define OV5647_REG_FRAME_OFF_NUMBER 0x4202
#define OV5640_REG_PAD_OUT          0x300d

#ifdef __cplusplus
}
#endif
