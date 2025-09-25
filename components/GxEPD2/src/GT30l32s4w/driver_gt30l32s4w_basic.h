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
 * @file      driver_gt30l32s4w_basic.h
 * @brief     driver gt30l32s4w basic header file
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

#ifndef DRIVER_GT30L32S4W_BASIC_H
#define DRIVER_GT30L32S4W_BASIC_H

#include "driver_gt30l32s4w_interface.h"

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @defgroup gt30l32s4w_example_driver gt30l32s4w example driver function
 * @brief    gt30l32s4w example driver modules
 * @ingroup  gt30l32s4w_driver
 * @{
 */

/**
 * @brief gt30l32s4w basic example default definition
 */
#define GT30L32S4W_BASIC_DEFAULT_MODE        GT30L32S4W_MODE_READ        /**< normal mode */

/**
 * @brief gt30l32s4w basic type enumeration definition
 */
typedef enum
{
    GT30L32S4W_BASIC_TYPE_ARIAL = 0x00,        /**< arial type */
    GT30L32S4W_BASIC_TYPE_TIMES = 0x01,        /**< times type */
    
} gt30l32s4w_basic_type_t;

/**
 * @brief  basic example init
 * @return status code
 *         - 0 success
 *         - 1 init failed
 * @note   none
 */
uint8_t gt30l32s4w_basic_init(void);

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t gt30l32s4w_basic_deinit(void);

/**
 * @brief     basic example print pattern
 * @param[in] type pattern type
 * @param[in] *buf pointer to an input buffer
 * @param[in] len buffer length
 * @return    status code
 *            - 0 success
 *            - 1 print pattern failed
 * @note      none
 */
uint8_t gt30l32s4w_basic_print_pattern(gt30l32s4w_type_t type, uint8_t *buf, uint8_t len);

/**
 * @brief      basic example read 12
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @param[out] *len pointer to a length buffer
 * @return     status code
 *             - 0 success
 *             - 1 read 12 failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_12(uint16_t ch, uint8_t buf[24], uint8_t *len);

/**
 * @brief      basic example read 16
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @param[out] *len pointer to a length buffer
 * @return     status code
 *             - 0 success
 *             - 1 read 16 failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_16(uint16_t ch, uint8_t buf[32], uint8_t *len);

/**
 * @brief      basic example read 24
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @param[out] *len pointer to a length buffer
 * @return     status code
 *             - 0 success
 *             - 1 read 24 failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_24(uint16_t ch, uint8_t buf[72], uint8_t *len);

/**
 * @brief      basic example read 32
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @param[out] *len pointer to a length buffer
 * @return     status code
 *             - 0 success
 *             - 1 read 32 failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_32(uint16_t ch, uint8_t buf[128], uint8_t *len);

/**
 * @brief      read ascii 7
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 7 failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_ascii_7(uint16_t ch, uint8_t buf[8]);

/**
 * @brief      read ascii 8
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 8 failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_ascii_8(uint16_t ch, uint8_t buf[8]);

/**
 * @brief      read ascii 12 with length
 * @param[in]  type output type
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 12 with length failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_ascii_12_with_length(gt30l32s4w_basic_type_t type, uint16_t ch, uint8_t buf[26]);

/**
 * @brief      read ascii 16 with length
 * @param[in]  type output type
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 16 with length failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_ascii_16_with_length(gt30l32s4w_basic_type_t type, uint16_t ch, uint8_t buf[34]);

/**
 * @brief      read ascii 24 with length
 * @param[in]  type output type
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 24 with length failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_ascii_24_with_length(gt30l32s4w_basic_type_t type, uint16_t ch, uint8_t buf[74]);

/**
 * @brief      read ascii 32 with length
 * @param[in]  type output type
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 32 with length failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_ascii_32_with_length(gt30l32s4w_basic_type_t type, uint16_t ch, uint8_t buf[130]);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
