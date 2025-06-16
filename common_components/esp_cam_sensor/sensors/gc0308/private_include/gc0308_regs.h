/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * GC0308 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* gc0308 registers */
#define GC0308_REG_DELAY          0xff
#define GC0308_REG_OUTPUT_EN      0x25
#define GC0308_REG_PAGE_SELECT    0xfe
#define GC0308_REG_ANALOG_MODE    0x1a
#define GC0308_REG_DEBUG_MODE     0x2e
#define GC0308_REG_CISCTL_MODE1   0x14
#define GC0308_REG_OUTPUT_FMT     0x24

#ifdef __cplusplus
}
#endif
