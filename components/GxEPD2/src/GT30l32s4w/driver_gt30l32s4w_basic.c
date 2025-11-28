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
 * @file      driver_gt30l32s4w_basic.c
 * @brief     driver gt30l32s4w basic source file
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

#include "driver_gt30l32s4w_basic.h"

static gt30l32s4w_handle_t gs_handle;        /**< gt30l32s4w handle */

/**
 * @brief  basic example init
 * @return status code
 *         - 0 success
 *         - 1 init failed
 * @note   none
 */
uint8_t gt30l32s4w_basic_init(void)
{
    uint8_t res;
    
    /* link function */
    DRIVER_GT30L32S4W_LINK_INIT(&gs_handle, gt30l32s4w_handle_t);
    DRIVER_GT30L32S4W_LINK_SPI_INIT(&gs_handle, gt30l32s4w_interface_spi_init);
    DRIVER_GT30L32S4W_LINK_SPI_DEINIT(&gs_handle, gt30l32s4w_interface_spi_deinit);
    DRIVER_GT30L32S4W_LINK_SPI_WRITE_READ(&gs_handle, gt30l32s4w_interface_spi_write_read);
    DRIVER_GT30L32S4W_LINK_DELAY_MS(&gs_handle, gt30l32s4w_interface_delay_ms);
    DRIVER_GT30L32S4W_LINK_DEBUG_PRINT(&gs_handle, gt30l32s4w_interface_debug_print);
    
    /* gt30l32s4w init */
    res = gt30l32s4w_init(&gs_handle);
    if (res != 0)
    {
        gt30l32s4w_interface_debug_print("gt30l32s4w: init failed.\n");
        
        return 1;
    }
    
     /* set default mode */
    res = gt30l32s4w_set_mode(&gs_handle, GT30L32S4W_BASIC_DEFAULT_MODE);
    if (res != 0)
    {
        gt30l32s4w_interface_debug_print("gt30l32s4w: set mode failed.\n");
        (void)gt30l32s4w_deinit(&gs_handle);
        
        return 1;
    }
    
    return 0;
}

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t gt30l32s4w_basic_deinit(void)
{
    /* deinit */
    if (gt30l32s4w_deinit(&gs_handle) != 0)
    {
        return 1;
    }
    
    return 0;
}

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
uint8_t gt30l32s4w_basic_print_pattern(gt30l32s4w_type_t type, uint8_t *buf, uint8_t len)
{
    /* print pattern */
    if (gt30l32s4w_print_pattern(&gs_handle, type, buf, len) != 0)
    {
        return 1;
    }
    
    return 0;
}

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
uint8_t gt30l32s4w_basic_read_12(uint16_t ch, uint8_t buf[24], uint8_t *len)
{
    uint8_t msb;
    uint8_t lsb;
    
    /* get msb and lsb */
    msb = (ch >> 8) & 0xFF;
    lsb = (ch >> 0) & 0xFF;
    
    /* ascii */
    if ((ch >= 0x20) && (ch <= 0x7E))
    {
        if (gt30l32s4w_read_ascii_6x12(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 12;
        
        return 0;
    }
    /* char */
    else if (((msb >= 0xA1) && (msb <= 0xA9) && (lsb >= 0xA1)) || 
             ((msb >= 0xB0) && (msb <= 0xF7) && (lsb >= 0xA1)))
    {
        if (gt30l32s4w_read_char_12x12(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 24;
        
        return 0;
    }
    /* extend char */
    else if (((ch >= 0xAAA1U) && (ch <= 0xAAFEU)) ||
             ((ch >= 0xABA1U) && (ch <= 0xABC0U))) 
    {
        if (gt30l32s4w_read_char_extend_6x12(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 12;
        
        return 0;
    }
    else
    {
        return 2;
    }
}

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
uint8_t gt30l32s4w_basic_read_16(uint16_t ch, uint8_t buf[32], uint8_t *len)
{
    uint8_t msb;
    uint8_t lsb;
    
    /* get msb and lsb */
    msb = (ch >> 8) & 0xFF;
    lsb = (ch >> 0) & 0xFF;
    
    /* ascii */
    if ((ch >= 0x20) && (ch <= 0x7E))
    {
        if (gt30l32s4w_read_ascii_8x16(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 16;
        
        return 0;
    }
    /* char */
    else if (((msb >= 0xA1) && (msb <= 0xA9) && (lsb >= 0xA1)) || 
             ((msb >= 0xB0) && (msb <= 0xF7) && (lsb >= 0xA1)))
    {
        if (gt30l32s4w_read_char_15x16(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 32;
        
        return 0;
    }
    /* extend char */
    else if (((ch >= 0xAAA1U) && (ch <= 0xAAFEU)) ||
             ((ch >= 0xABA1U) && (ch <= 0xABC0U))) 
    {
        if (gt30l32s4w_read_char_extend_8x16(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 16;
        
        return 0;
    }
    /* special char */
    else if ((ch >= 0xACA1U) && (ch <= 0xACDFU))
    {
        if (gt30l32s4w_read_char_special_8x16(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 16;
        
        return 0;
    }
    else
    {
        return 2;
    }
}

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
uint8_t gt30l32s4w_basic_read_24(uint16_t ch, uint8_t buf[72], uint8_t *len)
{
    uint8_t msb;
    uint8_t lsb;
    
    /* get msb and lsb */
    msb = (ch >> 8) & 0xFF;
    lsb = (ch >> 0) & 0xFF;
    
    /* ascii */
    if ((ch >= 0x20) && (ch <= 0x7E))
    {
        if (gt30l32s4w_read_ascii_12x24(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 48;
        
        return 0;
    }
    /* char */
    else if (((msb >= 0xA1) && (msb <= 0xA9) && (lsb >= 0xA1)) || 
             ((msb >= 0xB0) && (msb <= 0xF7) && (lsb >= 0xA1)))
    {
        if (gt30l32s4w_read_char_24x24(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 72;
        
        return 0;
    }
    /* extend char */
    else if (((ch >= 0xAAA1U) && (ch <= 0xAAFEU)) ||
             ((ch >= 0xABA1U) && (ch <= 0xABC0U))) 
    {
        if (gt30l32s4w_read_char_extend_12x24(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 48;
        
        return 0;
    }
    else
    {
        return 2;
    }
}

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
uint8_t gt30l32s4w_basic_read_32(uint16_t ch, uint8_t buf[128], uint8_t *len)
{
    uint8_t msb;
    uint8_t lsb;
    
    /* get msb and lsb */
    msb = (ch >> 8) & 0xFF;
    lsb = (ch >> 0) & 0xFF;
    
    /* ascii */
    if ((ch >= 0x20) && (ch <= 0x7E))
    {
        if (gt30l32s4w_read_ascii_16x32(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 64;
        
        return 0;
    }
    /* char */
    else if (((msb >= 0xA1) && (msb <= 0xA9) && (lsb >= 0xA1)) || 
             ((msb >= 0xB0) && (msb <= 0xF7) && (lsb >= 0xA1)))
    {
        if (gt30l32s4w_read_char_32x32(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 128;
        
        return 0;
    }
    /* extend char */
    else if (((ch >= 0xAAA1U) && (ch <= 0xAAFEU)) ||
             ((ch >= 0xABA1U) && (ch <= 0xABC0U))) 
    {
        if (gt30l32s4w_read_char_extend_16x32(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        *len = 64;
        
        return 0;
    }
    else
    {
        return 2;
    }
}

/**
 * @brief      read ascii 7
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 7 failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_ascii_7(uint16_t ch, uint8_t buf[8])
{
    /* read ascii 7 */
    if (gt30l32s4w_read_ascii_5x7(&gs_handle, ch, buf) != 0)
    {
        return 1;
    }
    
    return 0;
}

/**
 * @brief      read ascii 8
 * @param[in]  ch read char
 * @param[out] *buf pointer to an output buffer
 * @return     status code
 *             - 0 success
 *             - 1 read ascii 8 failed
 * @note       none
 */
uint8_t gt30l32s4w_basic_read_ascii_8(uint16_t ch, uint8_t buf[8])
{
    /* read ascii 8 */
    if (gt30l32s4w_read_ascii_7x8(&gs_handle, ch, buf) != 0)
    {
        return 1;
    }
    
    return 0;
}

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
uint8_t gt30l32s4w_basic_read_ascii_12_with_length(gt30l32s4w_basic_type_t type, uint16_t ch, uint8_t buf[26])
{
    /* arial type */
    if (type == GT30L32S4W_BASIC_TYPE_ARIAL)
    {
        if (gt30l32s4w_read_ascii_arial_12(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        
        return 0;
    }
    /* times type */
    else
    {
        if (gt30l32s4w_read_ascii_times_12(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        
        return 0;
    }
}

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
uint8_t gt30l32s4w_basic_read_ascii_16_with_length(gt30l32s4w_basic_type_t type, uint16_t ch, uint8_t buf[34])
{
    /* arial type */
    if (type == GT30L32S4W_BASIC_TYPE_ARIAL)
    {
        if (gt30l32s4w_read_ascii_arial_16(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        
        return 0;
    }
    /* times type */
    else
    {
        if (gt30l32s4w_read_ascii_times_16(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        
        return 0;
    }
}

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
uint8_t gt30l32s4w_basic_read_ascii_24_with_length(gt30l32s4w_basic_type_t type, uint16_t ch, uint8_t buf[74])
{
    /* arial type */
    if (type == GT30L32S4W_BASIC_TYPE_ARIAL)
    {
        if (gt30l32s4w_read_ascii_arial_24(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        
        return 0;
    }
    /* times type */
    else
    {
        if (gt30l32s4w_read_ascii_times_24(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        
        return 0;
    }
}

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
uint8_t gt30l32s4w_basic_read_ascii_32_with_length(gt30l32s4w_basic_type_t type, uint16_t ch, uint8_t buf[130])
{
    /* arial type */
    if (type == GT30L32S4W_BASIC_TYPE_ARIAL)
    {
        if (gt30l32s4w_read_ascii_arial_32(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        
        return 0;
    }
    /* times type */
    else
    {
        if (gt30l32s4w_read_ascii_times_32(&gs_handle, ch, buf) != 0)
        {
            return 1;
        }
        
        return 0;
    }
}
