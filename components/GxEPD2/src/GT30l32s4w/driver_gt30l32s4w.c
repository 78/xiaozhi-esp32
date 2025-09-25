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
 * @file      driver_gt30l32s4w.c
 * @brief     driver gt30l32s4w source file
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

#include "driver_gt30l32s4w.h"

/**
 * @brief chip information definition
 */
#define CHIP_NAME                 "Genitop GT30L32S4W"        /**< chip name */
#define MANUFACTURER_NAME         "Genitop"                   /**< manufacturer name */
#define SUPPLY_VOLTAGE_MIN        2.7f                        /**< chip min supply voltage */
#define SUPPLY_VOLTAGE_MAX        3.3f                        /**< chip max supply voltage */
#define MAX_CURRENT               20.0f                       /**< chip max current */
#define TEMPERATURE_MIN           -40.0f                      /**< chip min operating temperature */
#define TEMPERATURE_MAX           85.0f                       /**< chip max operating temperature */
#define DRIVER_VERSION            1000                        /**< driver version */

/**
 * @brief chip address definition
 */
#define GT30L32S4W_ADDRESS_12X12_GB2312         0x00000        /**< 12x12 gb2312 */
#define GT30L32S4W_ADDRESS_15X16_GB2312         0x2C9D0        /**< 15x16 gb2312 */
#define GT30L32S4W_ADDRESS_24X24_GB2312         0x68190        /**< 24x24 gb2312 */
#define GT30L32S4W_ADDRESS_32X32_GB2312         0xEDF00        /**< 32x32 gb2312 */
#define GT30L32S4W_ADDRESS_6X12_GB2312          0x1DBE0C       /**< 6x12 gb2312 */
#define GT30L32S4W_ADDRESS_6X12_ASCII           0x1DBE00       /**< 6x12 ascii */
#define GT30L32S4W_ADDRESS_12_ARIAL_ASCII       0x1DC400       /**< 12 arial ascii */
#define GT30L32S4W_ADDRESS_12_TIMES_ASCII       0x1DCDC0       /**< 12 times ascii */
#define GT30L32S4W_ADDRESS_8X16_GB2312          0x1DD790       /**< 8x16 gb2312 */
#define GT30L32S4W_ADDRESS_8X16_ASCII           0x1DD780       /**< 8x16 ascii */
#define GT30L32S4W_ADDRESS_5X7_ASCII            0x1DDF80       /**< 5x7 ascii */
#define GT30L32S4W_ADDRESS_7X8_ASCII            0x1DE280       /**< 7x8 ascii */
#define GT30L32S4W_ADDRESS_16_ARIAL_ASCII       0x1DE580       /**< 16 arial ascii */
#define GT30L32S4W_ADDRESS_16_TIMES_ASCII       0x1DF240       /**< 16 times ascii */
#define GT30L32S4W_ADDRESS_12X24_GB2312         0x1DFF30       /**< 12x24 gb2312 */
#define GT30L32S4W_ADDRESS_12X24_ASCII          0x1DFF00       /**< 12x24 ascii */
#define GT30L32S4W_ADDRESS_24_ARIAL_ASCII       0x1E22D0       /**< 24 arial ascii */
#define GT30L32S4W_ADDRESS_24_TIMES_ASCII       0x1E3E90       /**< 24 times ascii */
#define GT30L32S4W_ADDRESS_16X32_GB2312         0x1E5A90       /**< 16x32 gb2312 */
#define GT30L32S4W_ADDRESS_16X32_ASCII          0x1E5A50       /**< 16x32 ascii */
#define GT30L32S4W_ADDRESS_32_ARIAL_ASCII       0x1E99D0       /**< 32 arial ascii */
#define GT30L32S4W_ADDRESS_32_TIMES_ASCII       0x1ECA90       /**< 32 times ascii */
#define GT30L32S4W_ADDRESS_8X16_GB2312_SP       0x1F2880       /**< 8x16 gb2312 special */

/**
 * @brief      spi read
 * @param[in]  *handle pointer to a gt30l32s4w handle structure
 * @param[in]  addr chip address
 * @param[out] *out_buf pointer to an output buffer
 * @param[in]  out_len output buffer length
 * @return     status code
 *             - 0 success
 *             - 1 write read failed
 * @note       none
 */
static uint8_t a_gt30l32s4w_spi_read(gt30l32s4w_handle_t *handle, uint32_t addr, uint8_t *out_buf, uint32_t out_len)
{
    uint8_t reg[5];
    
    if (handle->mode == GT30L32S4W_MODE_READ)                                /* read mode */
    {
        reg[0] = GT30L32S4W_MODE_READ;                                       /* read mode */
        reg[1] = (addr >> 16) & 0xFF;                                        /* addr 2 */
        reg[2] = (addr >> 8) & 0xFF;                                         /* addr 1 */
        reg[3] = (addr >> 0) & 0xFF;                                         /* addr 0 */
        
        if (handle->spi_write_read(reg, 4, out_buf, out_len) != 0)           /* read data */
        {
            return 1;                                                        /* return error */
        }
        
        return 0;                                                            /* success return 0 */
    }
    else if (handle->mode == GT30L32S4W_MODE_FAST_MODE)                      /* fast read mode */
    {
        reg[0] = GT30L32S4W_MODE_FAST_MODE;                                  /* fast read mode */
        reg[1] = (addr >> 16) & 0xFF;                                        /* addr 2 */
        reg[2] = (addr >> 8) & 0xFF;                                         /* addr 1 */
        reg[3] = (addr >> 0) & 0xFF;                                         /* addr 0 */
        reg[4] = 0x00;                                                       /* dummy */
        
        if (handle->spi_write_read(reg, 5, out_buf, out_len) != 0)           /* read data */
        {
            return 1;                                                        /* return error */
        }
        
        return 0;                                                            /* success return 0 */
    }
    else
    {
        handle->debug_print("gt30l32s4w: mode is invalid.\n");               /* mode is invalid */
       
        return 1;                                                            /* return error */
    }
}

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
uint8_t gt30l32s4w_init(gt30l32s4w_handle_t *handle)
{
    const char buf_check[] = {0x00, 0x10, 0x28, 0x28, 0x28, 0x44, 0x44, 0x7C, 
                              0x82, 0x82, 0x82, 0x82, 0x00, 0x00, 0x00, 0x00};
    uint8_t buf[16];
    
    if (handle == NULL)                                                      /* check handle */
    {
        return 2;                                                            /* return error */
    }
    if (handle->debug_print == NULL)                                         /* check debug_print */
    {
        return 3;                                                            /* return error */
    }
    if (handle->spi_init == NULL)                                            /* check spi_init */
    {
        handle->debug_print("gt30l32s4w: spi_init is null.\n");              /* spi_init is null */
       
        return 3;                                                            /* return error */
    }
    if (handle->spi_deinit == NULL)                                          /* check spi_init */
    {
        handle->debug_print("gt30l32s4w: spi_deinit is null.\n");            /* spi_deinit is null */
       
        return 3;                                                            /* return error */
    }
    if (handle->spi_write_read == NULL)                                      /* check spi_write_read */
    {
        handle->debug_print("gt30l32s4w: spi_write_read is null.\n");        /* spi_write_read is null */
       
        return 3;                                                            /* return error */
    }
    if (handle->delay_ms == NULL)                                            /* check delay_ms */
    {
        handle->debug_print("gt30l32s4w: delay_ms is null.\n");              /* delay_ms is null */
       
        return 3;                                                            /* return error */
    }
    
    if (handle->spi_init() != 0)                                             /* spi init */
    { 
        handle->debug_print("gt30l32s4w: spi init failed.\n");               /* spi init failed */
       
        return 3;                                                            /* return error */
    }
    handle->mode = GT30L32S4W_MODE_READ;                                     /* read mode */
    if (a_gt30l32s4w_spi_read(handle, 
                              GT30L32S4W_ADDRESS_8X16_ASCII + 
                              ((uint8_t)('A') - 0x20) * 16, 
                              buf, 16) != 0)                                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");               /* spi read failed */
        (void)handle->spi_deinit();                                          /* spi deinit */
        
        return 4;                                                            /* return error */
    }
    if (strncmp((const char *)buf,  buf_check, 16) != 0)                     /* check buffer */
    {
        handle->debug_print("gt30l32s4w: spi check error.\n");               /* spi check error */
        (void)handle->spi_deinit();                                          /* spi deinit */
        
        return 4;                                                            /* return error */
    }
    handle->inited = 1;                                                      /* flag initialization */

    return 0;                                                                /* success return 0 */
}

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
uint8_t gt30l32s4w_deinit(gt30l32s4w_handle_t *handle)
{
    if (handle == NULL)                                                 /* check handle */
    {
        return 2;                                                       /* return error */
    }
    if (handle->inited != 1)                                            /* check handle initialization */
    {
        return 3;                                                       /* return error */
    }
    
    if (handle->spi_deinit() != 0)                                      /* spi deinit */
    {
        handle->debug_print("gt30l32s4w: spi deinit failed.\n");        /* spi deinit failed */
        
        return 1;                                                       /* return error */
    }         
    handle->inited = 0;                                                 /* flag close */
    
    return 0;                                                           /* success return 0 */
}

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
uint8_t gt30l32s4w_set_mode(gt30l32s4w_handle_t *handle, gt30l32s4w_mode_t mode)
{
    if (handle == NULL)             /* check handle */
    {
        return 2;                   /* return error */
    }
    if (handle->inited != 1)        /* check handle initialization */
    {
        return 3;                   /* return error */
    }
    
    handle->mode = mode;            /* set mode */
    
    return 0;                       /* success return 0 */
}

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
uint8_t gt30l32s4w_get_mode(gt30l32s4w_handle_t *handle, gt30l32s4w_mode_t *mode)
{
    if (handle == NULL)                               /* check handle */
    {
        return 2;                                     /* return error */
    }
    if (handle->inited != 1)                          /* check handle initialization */
    {
        return 3;                                     /* return error */
    }
    
    *mode = (gt30l32s4w_mode_t)(handle->mode);        /* get mode */
    
    return 0;                                         /* success return 0 */
}

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
uint8_t gt30l32s4w_read_char_12x12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[24])
{
    uint8_t msb;
    uint8_t lsb;
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    msb = (ch >> 8) & 0xFF;                                                /* msb */
    lsb = (ch >> 0) & 0xFF;                                                /* lsb */
    addr = GT30L32S4W_ADDRESS_12X12_GB2312;                                /* base address */
    if ((msb >= 0xA1) && (msb <= 0xA9) && (lsb >= 0xA1))                   /* range 1 */
    {
        addr =((msb - 0xA1) * 94 + (lsb - 0xA1)) * 24 + addr;              /* set address */
    }
    else if ((msb >= 0xB0) && (msb <= 0xF7) && (lsb >= 0xA1))              /* range 2 */
    {
        addr = ((msb - 0xB0) * 94 + (lsb - 0xA1) + 846) * 24 + addr;       /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 24) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_char_15x16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[32])
{
    uint8_t msb;
    uint8_t lsb;
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    msb = (ch >> 8) & 0xFF;                                                /* msb */
    lsb = (ch >> 0) & 0xFF;                                                /* lsb */
    addr = GT30L32S4W_ADDRESS_15X16_GB2312;                                /* base address */
    if ((msb >= 0xA1) && (msb <= 0xA9) && (lsb >= 0xA1))                   /* range 1 */
    {
        addr =((msb - 0xA1) * 94 + (lsb - 0xA1)) * 32 + addr;              /* set address */
    }
    else if ((msb >= 0xB0) && (msb <= 0xF7) && (lsb >= 0xA1))              /* range 2 */
    {
        addr = ((msb - 0xB0) * 94 + (lsb - 0xA1) + 846) * 32 + addr;       /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 32) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_char_24x24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[72])
{
    uint8_t msb;
    uint8_t lsb;
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    msb = (ch >> 8) & 0xFF;                                                /* msb */
    lsb = (ch >> 0) & 0xFF;                                                /* lsb */
    addr = GT30L32S4W_ADDRESS_24X24_GB2312;                                /* base address */
    if ((msb >= 0xA1) && (msb <= 0xA9) && (lsb >= 0xA1))                   /* range 1 */
    {
        addr =((msb - 0xA1) * 94 + (lsb - 0xA1)) * 72 + addr;              /* set address */
    }
    else if ((msb >= 0xB0) && (msb <= 0xF7) && (lsb >= 0xA1))              /* range 2 */
    {
        addr = ((msb - 0xB0) * 94 + (lsb - 0xA1) + 846) * 72 + addr;       /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 72) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_char_32x32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[128])
{
    uint8_t msb;
    uint8_t lsb;
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    msb = (ch >> 8) & 0xFF;                                                /* msb */
    lsb = (ch >> 0) & 0xFF;                                                /* lsb */
    addr = GT30L32S4W_ADDRESS_32X32_GB2312;                                /* base address */
    if ((msb >= 0xA1) && (msb <= 0xA9) && (lsb >= 0xA1))                   /* range 1 */
    {
        addr =((msb - 0xA1) * 94 + (lsb - 0xA1)) * 128 + addr;             /* set address */
    }
    else if ((msb >= 0xB0) && (msb <= 0xF7) && (lsb >= 0xA1))              /* range 2 */
    {
        addr = ((msb - 0xB0) * 94 + (lsb - 0xA1) + 846) * 128 + addr;      /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 128) != 0)                /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_char_extend_6x12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[12])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_6X12_GB2312;                                 /* base address */
    if ((ch >= 0xAAA1U) && (ch <= 0xAAFEU))                                /* range 1 */
    {
        addr =(ch - 0xAAA1U) * 12 + addr;                                  /* set address */
    }
    else if ((ch >= 0xABA1U) && (ch <= 0xABC0U))                           /* range 2 */
    {
        addr = (ch - 0xABA1U + 95) * 12 + addr;                            /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 12) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_char_extend_8x16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[16])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_8X16_GB2312;                                 /* base address */
    if ((ch >= 0xAAA1U) && (ch <= 0xAAFEU))                                /* range 1 */
    {
        addr =(ch - 0xAAA1U) * 16 + addr;                                  /* set address */
    }
    else if ((ch >= 0xABA1U) && (ch <= 0xABC0U))                           /* range 2 */
    {
        addr = (ch - 0xABA1U + 95) * 16 + addr;                            /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 16) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_char_special_8x16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[16])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_8X16_GB2312_SP;                              /* base address */
    if ((ch >= 0xACA1U) && (ch <= 0xACDFU))                                /* range 1 */
    {
        addr = (ch  - 0xACA1U) * 16 + addr;                                /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 16) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_char_extend_12x24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[48])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_12X24_GB2312;                                /* base address */
    if ((ch >= 0xAAA1U) && (ch <= 0xAAFEU))                                /* range 1 */
    {
        addr =(ch - 0xAAA1U) * 48 + addr;                                  /* set address */
    }
    else if ((ch >= 0xABA1U) && (ch <= 0xABC0U))                           /* range 2 */
    {
        addr = (ch - 0xABA1U + 95) * 48 + addr;                            /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 48) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_char_extend_16x32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[64])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_16X32_GB2312;                                /* base address */
    if ((ch >= 0xAAA1U) && (ch <= 0xAAFEU))                                /* range 1 */
    {
        addr =(ch - 0xAAA1U) * 64 + addr;                                  /* set address */
    }
    else if ((ch >= 0xABA1U) && (ch <= 0xABC0U))                           /* range 2 */
    {
        addr = (ch - 0xABA1U + 95) * 64 + addr;                            /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 64) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_5x7(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[8])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_5X7_ASCII;                                   /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 8 + addr;                                     /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 8) != 0)                  /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_7x8(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[8])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_7X8_ASCII;                                   /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 8 + addr;                                     /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 8) != 0)                  /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_6x12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[12])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_6X12_ASCII;                                  /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 12 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 12) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_8x16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[16])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_8X16_ASCII;                                  /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 16 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 16) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_12x24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[48])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_12X24_ASCII;                                 /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 48 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 48) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_16x32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[64])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_16X32_ASCII;                                 /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 64 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 64) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_arial_12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[26])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_12_ARIAL_ASCII;                              /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 26 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 26) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_times_12(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[26])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_12_TIMES_ASCII;                              /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 26 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 26) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_arial_16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[34])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_16_ARIAL_ASCII;                              /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 34 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 34) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_times_16(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[34])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_16_TIMES_ASCII;                              /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 34 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 34) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_arial_24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[74])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_24_ARIAL_ASCII;                              /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 74 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 74) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_times_24(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[74])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_24_TIMES_ASCII;                              /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 74 + addr;                                    /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 74) != 0)                 /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_arial_32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[130])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_32_ARIAL_ASCII;                              /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 130 + addr;                                   /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 130) != 0)                /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_read_ascii_times_32(gt30l32s4w_handle_t *handle, uint16_t ch, uint8_t buf[130])
{
    uint32_t addr;
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    addr = GT30L32S4W_ADDRESS_32_TIMES_ASCII;                              /* base address */
    if ((ch >= 0x20) && (ch <= 0x7E))                                      /* range 1 */
    {
        addr = (ch - 0x20) * 130 + addr;                                   /* set address */
    }
    else
    {
        handle->debug_print("gt30l32s4w: char is invalid.\n");             /* char is invalid */
        
        return 4;                                                          /* return error */
    }
    
    if (a_gt30l32s4w_spi_read(handle, addr, buf, 130) != 0)                /* spi read */
    {
        handle->debug_print("gt30l32s4w: spi read failed.\n");             /* spi read failed */
        
        return 1;                                                          /* return error */
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_print_pattern(gt30l32s4w_handle_t *handle, gt30l32s4w_type_t type, uint8_t *buf, uint8_t len)
{
    uint16_t i;
    uint16_t j;
    uint16_t point;
    char str_buf[129];
    
    if (handle == NULL)                                                    /* check handle */
    {
        return 2;                                                          /* return error */
    }
    if (handle->inited != 1)                                               /* check handle initialization */
    {
        return 3;                                                          /* return error */
    }
    
    switch (type)                                                          /* check type */
    {
        case GT30L32S4W_TYPE_12X12_GB2312 :                                /* 12x12 gb2312 */
        {
            if (len != 24)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 12; i++)                                       /* 12 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 12; j++)                                   /* 12 */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_15X16_GB2312 :                                /* 15x16 gb2312 */
        {
            if (len != 32)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 16; i++)                                       /* 16 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 15; j++)                                   /* 15 */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_24X24_GB2312 :                                /* 24x24 gb2312 */
        {
            if (len != 72)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 24; i++)                                       /* 24 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 24; j++)                                   /* 24 */
                {
                    point = i * 24 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_32X32_GB2312 :                                /* 32x32 gb2312 */
        {
            if (len != 128)                                                /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 32; i++)                                       /* 32 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 32; j++)                                   /* 32 */
                {
                    point = i * 32 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_6X12_GB2312_EX :                              /* 6x12 gb2312 extend */
        {
            if (len != 12)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 12; i++)                                       /* 12 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 6; j++)                                    /* 6 */
                {
                    point = i * 8 + j;                                     /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_8X16_GB2312_EX :                              /* 8x16 gb2312 extend */
        {
            if (len != 16)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 16; i++)                                       /* 16 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 8; j++)                                    /* 8 */
                {
                    point = i * 8 + j;                                     /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_8X16_GB2312_SP :                              /* 8x16 gb2312 special */
        {
            if (len != 16)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 16; i++)                                       /* 16 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 8; j++)                                    /* 8 */
                {
                    point = i * 8 + j;                                     /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_12X24_GB2312_EX :                             /* 12x24 gb2312 extend */
        {
            if (len != 48)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 24; i++)                                       /* 24 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 12; j++)                                   /* 12 */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_16X32_GB2312_EX :                             /* 16x32 gb2312 extend */
        {
            if (len != 64)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 32; i++)                                       /* 32 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 16; j++)                                   /* 16 */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_5X7_ASCII :                                   /* 5x7 ascii */
        {
            if (len != 8)                                                  /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 7; i++)                                        /* 7 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 5; j++)                                    /* 5 */
                {
                    point = i * 8 + j;                                     /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_7X8_ASCII :                                   /* 7x8 ascii */
        {
            if (len != 8)                                                  /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 8; i++)                                        /* 8 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 7; j++)                                    /* 7 */
                {
                    point = i * 8 + j;                                     /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_6X12_ASCII :                                  /* 6x12 ascii */
        {
            if (len != 12)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 12; i++)                                       /* 12 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 6; j++)                                    /* 6 */
                {
                    point = i * 8 + j;                                     /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_8X16_ASCII :                                  /* 8x16 ascii */
        {
            if (len != 16)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 16; i++)                                       /* 16 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 8; j++)                                    /* 8 */
                {
                    point = i * 8 + j;                                     /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_12X24_ASCII :                                 /* 12x24 ascii */
        {
            if (len != 48)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 24; i++)                                       /* 24 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 12; j++)                                   /* 12 */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_16X32_ASCII :                                 /* 16x32 ascii */
        {
            if (len != 64)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            for (i = 0; i < 32; i++)                                       /* 32 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < 16; j++)                                   /* 16 */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_12_ARIAL_ASCII :                              /* 12 arial ascii */
        {
            uint16_t l;
            
            if (len != 26)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            l = (uint16_t)buf[0] << 8 | buf[1];                            /* get length */
            buf += 2;                                                      /* set to data part */
            for (i = 0; i < 12; i++)                                       /* 12 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < l; j++)                                    /* l */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_12_TIMES_ASCII :                              /* 12 times ascii */
        {
            uint16_t l;
            
            if (len != 26)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            l = (uint16_t)buf[0] << 8 | buf[1];                            /* get length */
            buf += 2;                                                      /* set to data part */
            for (i = 0; i < 12; i++)                                       /* 12 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < l; j++)                                    /* l */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_16_ARIAL_ASCII :                              /* 16 arial ascii */
        {
            uint16_t l;
            
            if (len != 34)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            l = (uint16_t)buf[0] << 8 | buf[1];                            /* get length */
            buf += 2;                                                      /* set to data part */
            for (i = 0; i < 16; i++)                                       /* 16 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < l; j++)                                    /* l */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_16_TIMES_ASCII :                              /* 16 times ascii */
        {
            uint16_t l;
            
            if (len != 34)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            l = (uint16_t)buf[0] << 8 | buf[1];                            /* get length */
            buf += 2;                                                      /* set to data part */
            for (i = 0; i < 16; i++)                                       /* 16 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < l; j++)                                    /* l */
                {
                    point = i * 16 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_24_ARIAL_ASCII :                              /* 24 arial ascii */
        {
            uint16_t l;
            
            if (len != 74)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            l = (uint16_t)buf[0] << 8 | buf[1];                            /* get length */
            buf += 2;                                                      /* set to data part */
            for (i = 0; i < 24; i++)                                       /* 24 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < l; j++)                                    /* l */
                {
                    point = i * 24 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_24_TIMES_ASCII :                              /* 24 times ascii */
        {
            uint16_t l;
            
            if (len != 74)                                                 /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            l = (uint16_t)buf[0] << 8 | buf[1];                            /* get length */
            buf += 2;                                                      /* set to data part */
            for (i = 0; i < 24; i++)                                       /* 24 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < l; j++)                                    /* l */
                {
                    point = i * 24 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_32_ARIAL_ASCII :                              /* 32 arial ascii */
        {
            uint16_t l;
            
            if (len != 130)                                                /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            l = (uint16_t)buf[0] << 8 | buf[1];                            /* get length */
            buf += 2;                                                      /* set to data part */
            for (i = 0; i < 32; i++)                                       /* 32 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < l; j++)                                    /* l */
                {
                    point = i * 32 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        case GT30L32S4W_TYPE_32_TIMES_ASCII :                              /* 32 times ascii */
        {
            uint16_t l;
            
            if (len != 130)                                                /* check length */
            {
                handle->debug_print("gt30l32s4w: len is invalid.\n");      /* len is invalid */
                
                return 4;                                                  /* return error */
            }
            l = (uint16_t)buf[0] << 8 | buf[1];                            /* get length */
            buf += 2;                                                      /* set to data part */
            for (i = 0; i < 32; i++)                                       /* 32 */
            {
                memset(str_buf, 0, sizeof(char) * 129);                    /* clear buffer */
                for (j = 0; j < l; j++)                                    /* l */
                {
                    point = i * 32 + j;                                    /* get point */
                    if ((buf[point / 8] >> (7 - (point % 8))) != 0)        /* check point */
                    {
                        str_buf[j * 3 + 0] = '#';                          /* # */
                        str_buf[j * 3 + 1] = '#';                          /* # */
                        str_buf[j * 3 + 2] = '#';                          /* # */
                    }
                    else
                    {
                        str_buf[j * 3 + 0] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 1] = ' ';                          /* 0x20 */
                        str_buf[j * 3 + 2] = ' ';                          /* 0x20 */
                    }
                }
                handle->debug_print("%s\n", str_buf);                      /* print buffer */
            }
            
            break;                                                         /* break */
        }
        default :
        {
            break;                                                         /* break */
        }
    }
    
    return 0;                                                              /* success return 0 */
}

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
uint8_t gt30l32s4w_get_reg(gt30l32s4w_handle_t *handle, uint8_t *in_buf, uint32_t in_len, uint8_t *out_buf, uint32_t out_len)
{
    if (handle == NULL)                                                     /* check handle */
    {
        return 2;                                                           /* return error */
    }
    if (handle->inited != 1)                                                /* check handle initialization */
    {
        return 3;                                                           /* return error */
    }
    
    return handle->spi_write_read(in_buf, in_len, out_buf, out_len);        /* write read reg */
}

/**
 * @brief      get chip's information
 * @param[out] *info pointer to a gt30l32s4w info structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t gt30l32s4w_info(gt30l32s4w_info_t *info)
{
    if (info == NULL)                                               /* check handle */
    {
        return 2;                                                   /* return error */
    }
    
    memset(info, 0, sizeof(gt30l32s4w_info_t));                     /* initialize gt30l32s4w info structure */
    strncpy(info->chip_name, CHIP_NAME, 32);                        /* copy chip name */
    strncpy(info->manufacturer_name, MANUFACTURER_NAME, 32);        /* copy manufacturer name */
    strncpy(info->interface, "SPI", 8);                             /* copy interface name */
    info->supply_voltage_min_v = SUPPLY_VOLTAGE_MIN;                /* set minimal supply voltage */
    info->supply_voltage_max_v = SUPPLY_VOLTAGE_MAX;                /* set maximum supply voltage */
    info->max_current_ma = MAX_CURRENT;                             /* set maximum current */
    info->temperature_max = TEMPERATURE_MAX;                        /* set minimal temperature */
    info->temperature_min = TEMPERATURE_MIN;                        /* set maximum temperature */
    info->driver_version = DRIVER_VERSION;                          /* set driver version */
    
    return 0;                                                       /* success return 0 */
}
