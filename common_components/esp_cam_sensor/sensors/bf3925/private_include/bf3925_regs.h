/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * BF3925 register definitions.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* bf3925 registers */
#define BF3925_REG_DELAY               0xfe

#define BF3925_REG_OUTPUT_EN           0x25
#define BF3925_REG_PAGE_SELECT         0xff
#define BF3925_REG_SOFTWARE_STANDBY    0xf2
#define BF3925_REG_CHIP_ID_H           0xfc
#define BF3925_REG_CHIP_ID_L           0xfd
#define BF3925_REG_TEST_MODE           0x4c

#ifdef __cplusplus
}
#endif
