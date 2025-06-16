/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * SC101IOT register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* sc101iot registers */
#define SC101IOT_REG_ID_HIGH     0x31f7
#define SC101IOT_REG_ID_LOW      0x31f8
#define SC101IOT_REG_PAGE_SELECT 0xf0
#define SC101IOT_REG_DELAY       0xfe
#define SC101IOT_REG_SLEEP_MODE  0x3100

#ifdef __cplusplus
}
#endif
