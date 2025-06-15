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
 * SC030IOT camera sensor register type definition.
 */
typedef struct {
    uint8_t reg;
    uint8_t val;
} sc030iot_reginfo_t;

#ifdef __cplusplus
}
#endif
