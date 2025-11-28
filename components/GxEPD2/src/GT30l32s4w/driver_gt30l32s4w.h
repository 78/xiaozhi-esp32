/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      driver_gt30l32s4w.h
 * @brief     driver gt30l32s4w header file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2023-09-15
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2023/09/15  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#ifndef DRIVER_GT30L32S4W_H
#define DRIVER_GT30L32S4W_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @defgroup gt30l32s4w_driver gt30l32s4w driver function
 * @brief    gt30l32s4w driver modules
 * @{
 */

/**
 * @addtogroup gt30l32s4w_base_driver
 * @{
 */

/**
 * @brief gt30l32s4w mode enumeration definition
 */
typedef enum
{
    GT30L32S4W_MODE_READ      = 0x03,        /**< read data bytes */
    GT30L32S4W_MODE_FAST_MODE = 0x0B,        /**< read data bytes at higher speed */
} gt30l32s4w_mode_t;

/**
 * @brief gt30l32s4w type enumeration definition
 */
typedef enum
{
    GT30L32S4W_TYPE_12X12_GB2312    = 0x01,        /**< 12x12 gb2312 */
    GT30L32S4W_TYPE_15X16_GB2312    = 0x02,        /**< 15x16 gb2312 */
    GT30L32S4W_TYPE_24X24_GB2312    = 0x03,        /**< 24x24 gb2312 */
    GT30L32S4W_TYPE_32X32_GB2312    = 0x04,        /**< 32x32 gb2312 */
    GT30L32S4W_TYPE_6X12_GB2312_EX  = 0x05,        /**< 6x12 gb2312 extend */
    GT30L32S4W_TYPE_6X12_ASCII      = 0x06,        /**< 6x12 ascii */
    GT30L32S4W_TYPE_12_ARIAL_ASCII  = 0x07,        /**< 12 arial ascii */
    GT30L32S4W_TYPE_12_TIMES_ASCII  = 0x08,        /**< 12 times ascii */
    GT30L32S4W_TYPE_8X16_GB2312_EX  = 0x09,        /**< 8x16 gb2312 extend */
    GT30L32S4W_TYPE_8X16_ASCII      = 0x0A,        /**< 8x16 ascii */
    GT30L32S4W_TYPE_5X7_ASCII       = 0x0B,        /**< 5x7 ascii */
    GT30L32S4W_TYPE_7X8_ASCII       = 0x0C,        /**< 7x8 ascii */
    GT30L32S4W_TYPE_16_ARIAL_ASCII  = 0x0D,        /**< 16 arial ascii */
    GT30L32S4W_TYPE_16_TIMES_ASCII  = 0x0E,        /**< 16 times ascii */
    GT30L32S4W_TYPE_12X24_GB2312_EX = 0x0F,        /**< 12x24 gb2312 extend */
    GT30L32S4W_TYPE_12X24_ASCII     = 0x10,        /**< 12x24 ascii */
    GT30L32S4W_TYPE_24_ARIAL_ASCII  = 0x11,        /**< 24 arial ascii */
    GT30L32S4W_TYPE_24_TIMES_ASCII  = 0x12,        /**< 24 times ascii */
    GT30L32S4W_TYPE_16X32_GB2312_EX = 0x13,        /**< 16x32 gb2312 extend */
    GT30L32S4W_TYPE_16X32_ASCII     = 0x14,        /**< 16x32 ascii */
    GT30L32S4W_TYPE_32_ARIAL_ASCII  = 0x15,        /**< 32 arial ascii */
    GT30L32S4W_TYPE_32_TIMES_ASCII  = 0x16,        /**< 32 times ascii */
    GT30L32S4W_TYPE_8X16_GB2312_SP  = 0x17,        /**< 8x16 gb2312 special */
} gt30l32s4w_type_t;

/**
 * @brief gt30l32s4w handle structure definition
 */
typedef struct gt30l32s4w_handle_s
{
    uint8_t (*spi_init)(void);                                            /**< point to a spi_init function address */
    uint8_t (*spi_deinit)(void);                                          /**< point to a spi_deinit function address */
    uint8_t  (*spi_write_read)(uint8_t *in_buf, uint32_t in_len,
                               uint8_t *out_buf, uint32_t out_len);       /**< point to a spi_write_read function address */
    void (*delay_ms)(uint32_t ms);                                        /**< point to a delay_ms function address */
    void (*debug_print)(const char *const fmt, ...);                      /**< point to a debug_print function address */
    uint8_t inited;                                                       /**< inited flag */
    uint8_t mode;                                                         /**< mode */
} gt30l32s4w_handle_t;

/**
 * @brief gt30l32s4w information structure definition
 */
typedef struct gt30l32s4w_info_s
{
    char chip_name[32];                /**< chip name */
    char manufacturer_name[32];        /**< manufacturer name */
    char interface[8];                 /**< chip interface name */
    float supply_voltage_min_v;        /**< chip min supply voltage */
    float supply_voltage_max_v;        /**< chip max supply voltage */
    float max_current_ma;              /**< chip max current */
    float temperature_min;             /**< chip min operating temperature */
    float temperature_max;             /**< chip max operating temperature */
    uint32_t driver_version;           /**< driver version */
} gt30l32s4w_info_t;

/**
 * @}
 */

/**
 * @defgroup gt30l32s4w_link_driver gt30l32s4w link driver function
 * @brief    gt30l32s4w link driver modules
 * @ingroup  gt30l32s4w_driver
 * @{
 */

/**
 * @brief     initialize gt30l32s4w_handle_t structure
 * @param[in] HANDLE pointer to a gt30l32s4w handle structure
 * @param[in] STRUCTURE gt30l32s4w_handle_t
 * @note      none
 */
#define DRIVER_GT30L32S4W_LINK_INIT(HANDLE, STRUCTURE)         memset(HANDLE, 0, sizeof(STRUCTURE))

/**
 * @brief     link spi_init function
 * @param[in] HANDLE pointer to a gt30l32s4w handle structure
 * @param[in] FUC pointer to a spi_init function address
 * @note      none
 */
#define DRIVER_GT30L32S4W_LINK_SPI_INIT(HANDLE, FUC)          (HANDLE)->spi_init = FUC

/**
 * @brief     link spi_deinit function
 * @param[in] HANDLE pointer to a gt30l32s4w handle structure
 * @param[in] FUC pointer to a spi_deinit function address
 * @note      none
 */
#define DRIVER_GT30L32S4W_LINK_SPI_DEINIT(HANDLE, FUC)        (HANDLE)->spi_deinit = FUC

/**
 * @brief     link spi_write_read function
 * @param[in] HANDLE pointer to a gt30l32s4w handle structure
 * @param[in] FUC pointer to a spi_write_read function address
 * @note      none
 */
#define DRIVER_GT30L32S4W_LINK_SPI_WRITE_READ(HANDLE, FUC)    (HANDLE)->spi_write_read = FUC

/**
 * @brief     link delay_ms function
 * @param[in] HANDLE pointer to a gt30l32s4w handle structure
 * @param[in] FUC pointer to a delay_ms function address
 * @note      none
 */
#define DRIVER_GT30L32S4W_LINK_DELAY_MS(HANDLE, FUC)          (HANDLE)->delay_ms = FUC

/**
 * @brief     link debug_print function
 * @param[in] HANDLE pointer to a gt30l32s4w handle structure
 * @param[in] FUC pointer to a debug_print function address
 * @note      none
 */
#define DRIVER_GT30L32S4W_LINK_DEBUG_PRINT(HANDLE, FUC)       (HANDLE)->debug_print = FUC

/**
 * @}
 */

/**
 * @defgroup gt30l32s4w_base_driver gt30l32s4w base driver function
 * @brief    gt30l32s4w base driver modules
 * @ingroup  gt30l32s4w_driver
 * @{
 */

/**
 * @brief      get chip's information
 * @param[out] *info pointer to a gt30l32s4w info structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t gt30l32s4w_info(gt30l32s4w_info_t *info);

/**
 * @brief     set mode
 * @param[in] *handle pointer to a gt30l32s4w handle structure
 * @param[in] mode set mode
 * @return    status code
 *            - 0 success
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t gt30l32s4w_set_mode(gt30l32s4w_handle_t *handle, gt30l32s4w_mode_t mode);

/**
 * @brief      get mode
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[out] *mode pointer to a mode buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t gt30l32s4w_get_mode(gt30l32s4w_handle_t *handle, gt30l32s4w_mode_t *mode);

/**
 * @brief     initialize the chip
 * @param[in] *handle pointer to a gt30l32s4w handle structure
 * @return    status code
 *            - 0 success
 *            - 1 spi initialization failed
 *            - 2 handle is NULL
 *            - 3 linked functions is NULL
 *            - 4 spi read error
 * @note      none
 */
uint8_t gt30l32s4w_init(gt30l32s4w_handle_t *handle);

/**
 * @brief     close the chip
 * @param[in] *handle pointer to a gt30l32s4w handle structure
 * @return    status code
 *            - 0 success
 *            - 1 spi deinit failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t gt30l32s4w_deinit(gt30l32s4w_handle_t *handle);

/**
 * @brief     print pattern
 * @param[in] *handle pointer to a gt30l32s4w handle structure
 * @param[in] type pattern type
 * @param[in] *buf pointer to an input buffer
 * @param[in] len buffer length
 * @return    status code
 *            - 0 success
 *            - 1 print pattern failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 len is invalid
 * @note      none
 */
uint8_t gt30l32s4w_print_pattern(gt30l32s4w_handle_t *handle, gt30l32s4w_type_t type, uint8_t *buf, uint8_t len);

/**
 * @brief      read char 12x12
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read char 12x12 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_char_12x12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[24]);

/**
 * @brief      read char 15x16
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read char 15x16 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_char_15x16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[32]);

/**
 * @brief      read char 24x24
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read char 24x24 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_char_24x24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[72]);

/**
 * @brief      read char 32x32
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read char 32x32 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_char_32x32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[128]);

/**
 * @brief      read char extend 6x12
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read char extend 6x12 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_char_extend_6x12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[12]);

/**
 * @brief      read char extend 8x16
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read char extend 8x16 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_char_extend_8x16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[16]);

/**
 * @brief      read char special 8x16
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read char special 8x16 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_char_special_8x16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[16]);

/**
 * @brief      read char extend 12x24
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read char extend 12x24 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_char_extend_12x24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[48]);

/**
 * @brief      read char extend 16x32
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read char extend 16x32 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_char_extend_16x32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[64]);

/**
 * @brief      read ascii 5x7
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 5x7 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_5x7(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[8]);

/**
 * @brief      read ascii 7x8
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 7x8 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_7x8(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[8]);

/**
 * @brief      read ascii 6x12
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 6x12 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_6x12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[12]);

/**
 * @brief      read ascii 8x16
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 8x16 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_8x16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[16]);

/**
 * @brief      read ascii 12x24
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 12x24 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_12x24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[48]);

/**
 * @brief      read ascii 16x32
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 16x32 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_16x32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[64]);

/**
 * @brief      read ascii arial 12
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii arial 12 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_arial_12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[26]);

/**
 * @brief      read ascii times 12
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii times 12 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_times_12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[26]);

/**
 * @brief      read ascii arial 16
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii arial 16 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_arial_16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[34]);

/**
 * @brief      read ascii times 16
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii times 16 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_times_16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[34]);

/**
 * @brief      read ascii arial 24
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii arial 24 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_arial_24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[74]);

/**
 * @brief      read ascii times 24
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii times 24 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_times_24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[74]);

/**
 * @brief      read ascii arial 32
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii arial 32 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_arial_32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[130]);

/**
 * @brief      read ascii times 32
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii times 32 failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 *             - 4 char is invalid
 * @note       none
 */
uint8_t gt30l32s4w_read_ascii_times_32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[130]);

/**
 * @}
 */

/**
 * @defgroup gt30l32s4w_extern_driver gt30l32s4w extern driver function
 * @brief    gt30l32s4w extern driver modules
 * @ingroup  gt30l32s4w_driver
 * @{
 */

/**
 * @brief      get the chip register
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  *in_buf pointer to an input buffer
 * @param[in]  in_len input buffer length
 * @param[out] *out_buf pointer to an output buffer
 * @param[in]  out_len output buffer length
 * @return     status code
 *             - 0 success
 *             - 1 write read failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t gt30l32s4w_get_reg(gt30l32s4w_handle_t *handle, uint8_t *in_buf, uint32_t in_len, uint8_t *out_buf, uint32_t out_len);

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
