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
 * @file      driver_gt30l32s4w_interface_template.c
 * @brief     driver gt30l32s4w interface template source file
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

#include "driver_gt30l32s4w_interface.h"
#include <string.h>
#include "esp_log.h"
#include <stdint.h>
#include "driver/spi_master.h"
#include "Display_EPD_W21_spi.h"
#include "Arduino.h"
#include "SPI.h"
static const char *TAG = "GT30_DEMO";
/**
 * @brief  interface spi bus init
 * @return status code
 *         - 0 success
 *         - 1 spi init failed
 * @note   none
 */
uint8_t gt30l32s4w_interface_spi_init(void)
{
    return 0;
}

/**
 * @brief  interface spi bus deinit
 * @return status code
 *         - 0 success
 *         - 1 spi deinit failed
 * @note   none
 */
uint8_t gt30l32s4w_interface_spi_deinit(void)
{
    return 0;
}

/**
 * @brief      interface spi bus write read
 * @param[in]  *in_buf pointer to an input buffer
 * @param[in]  in_len input buffer length
 * @param[out] *out_buf pointer to an output buffer
 * @param[in]  out_len output buffer length
 * @return     status code
 *             - 0 success
 *             - 1 write read failed
 * @note       none
 */
uint8_t gt30l32s4w_interface_spi_write_read(uint8_t *in_buf, uint32_t in_len, uint8_t *out_buf, uint32_t out_len)
{
   
    // esp_err_t ret;


    // if (in_buf == NULL || out_buf == NULL || in_len == 0 || out_len == 0)
    //     return 1;

    // GT30_W21_CS_0;
    // //write data
    // spi_transaction_t t;
    // memset(&t, 0, sizeof(t));
    // t.tx_buffer = in_buf;
    // t.rx_buffer = NULL;
    // t.length = in_len * 8;
    // if ((ret=spi_device_transmit(SPI, &t)) != ESP_OK)
    // {
            
    //     ESP_LOGE("SPI", "GT30 write failed: %s", esp_err_to_name(ret));
    //     GT30_W21_CS_1;
    //     return 1;
    // }

    // //read data
    // memset(&t, 0, sizeof(t));
    // t.tx_buffer = NULL;
    // t.rx_buffer = out_buf;
    // t.length = out_len * 8;
    // if ((ret=spi_device_transmit(SPI, &t)) != ESP_OK)
    // {
    //     ESP_LOGE("SPI", "GT30 read failed: %s", esp_err_to_name(ret));
    //     GT30_W21_CS_1;
    //     return 1;
    // }
    // GT30_W21_CS_1;
    // return 0;

  if (in_buf == NULL || out_buf == NULL || in_len == 0 || out_len == 0)
        return 1;

    GT30_W21_CS_0;

    //write data
    for (uint32_t i = 0; i < in_len; i++)
    {
        SPI.transfer(in_buf[i]);
    }

    //read data
    for (uint32_t i = 0; i < out_len; i++)
    {
        out_buf[i] = SPI.transfer(0x00);
    }

    GT30_W21_CS_1;

    return 0;


}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 */
void gt30l32s4w_interface_delay_ms(uint32_t ms)
{

}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 */
void gt30l32s4w_interface_debug_print(const char *const fmt, ...)
{
    
}
