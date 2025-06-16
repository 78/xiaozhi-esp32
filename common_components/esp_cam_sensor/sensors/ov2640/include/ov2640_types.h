/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OV2640 camera sensor register type definition.
 */
typedef struct {
    uint8_t reg;
    uint8_t val;
} ov2640_reginfo_t;

/*
 * OV2640 camera sensor register banks definition.
 */
typedef enum {
    BANK_DSP,    // When register 0xFF=0x00, DSP register bank is available
    BANK_SENSOR, // When register 0xFF=0x01, Sensor register bank is available
    BANK_MAX,
} ov2640_bank_t;

#ifdef __cplusplus
}
#endif
