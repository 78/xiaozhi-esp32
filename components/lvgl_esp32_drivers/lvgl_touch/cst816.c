/*
 * Copyright © 2020 Wolfgang Christl
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the “Software”), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <esp_log.h>
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include <lvgl.h>
#else
#include <lvgl/lvgl.h>
#endif
#include "cst816.h"
#include "lvgl_i2c/i2c_manager.h"

#define TAG "CST816"
#define CST816_TOUCH_QUEUE_ELEMENTS 1

static CST816_status_t CST816_status;
static uint8_t current_dev_addr;                                   // set during init
static CST816_touch_t touch_inputs = {-1, -1, LV_INDEV_STATE_REL}; // -1 coordinates to designate it was never touched
#if CONFIG_LV_CST816_COORDINATES_QUEUE
QueueHandle_t CST816_touch_queue_handle;
#endif
static esp_err_t cst816_i2c_write8(uint8_t slave_addr, uint8_t register_addr, uint8_t *data_buf)
{
    return lvgl_i2c_write(CONFIG_LV_I2C_TOUCH_PORT, slave_addr, register_addr, data_buf, 1);
}
static esp_err_t cst816_i2c_read8(uint8_t slave_addr, uint8_t register_addr, uint8_t *data_buf)
{
    return lvgl_i2c_read(CONFIG_LV_I2C_TOUCH_PORT, slave_addr, register_addr, data_buf, 1);
}

/**
 * @brief  Read the CST816 gesture ID. Initialize first!
 * @param  dev_addr: I2C CST816 Slave address.
 * @retval The gesture ID or 0x00 in case of failure
 */
uint8_t cst816_get_gesture_id()
{
    if (!CST816_status.inited)
    {
        ESP_LOGE(TAG, "Init first!");
        return 0x00;
    }
    uint8_t data_buf;
    esp_err_t ret;
    if ((ret = cst816_i2c_read8(current_dev_addr, CST816_GEST_ID_REG, &data_buf) != ESP_OK))
        ESP_LOGE(TAG, "Error reading from device: %s", esp_err_to_name(ret));
    return data_buf;
}

/**
 * @brief  Initialize for CST816 communication via I2C
 * @param  dev_addr: Device address on communication Bus (I2C slave address of CST816).
 * @retval None
 */


void cst816_init(uint16_t dev_addr)
{

    CST816_status.inited = true;
    current_dev_addr = dev_addr;
    uint8_t data_buf=0x01;
    uint8_t standby_value = 0x01;
    uint8_t xy_buf[6] = {0};
    esp_err_t ret;
    ESP_LOGI(TAG, "Found touch panel controller");
    cst816_i2c_write8(dev_addr, TP_REG_DIS_AUTO_SLEEP, &data_buf);

    if ((ret = cst816_i2c_read8(dev_addr, CST816S_ADDRESS, &data_buf) != ESP_OK))
        ESP_LOGE(TAG, "Error reading from device: %s", esp_err_to_name(ret)); // Only show error the first time
    ESP_LOGI(TAG, "\tDevice ID: 0x%02x", data_buf);

    cst816_i2c_read8(dev_addr, TP_REG_CHIPID, &data_buf);
    ESP_LOGI(TAG, "\tChip ID: 0x%02x", data_buf);


#if CONFIG_LV_CST816_COORDINATES_QUEUE
    CST816_touch_queue_handle = xQueueCreate(CST816_TOUCH_QUEUE_ELEMENTS, sizeof(CST816_touch_t));
    if (CST816_touch_queue_handle == NULL)
    {
        ESP_LOGE(TAG, "\tError creating touch input FreeRTOS queue");
        return;
    }
    xQueueSend(CST816_touch_queue_handle, &touch_inputs, 0);
#endif
}

/**
 * @brief  Get the touch screen X and Y positions values. Ignores multi touch
 * @param  drv:
 * @param  data: Store data here
 * @retval Always false
 */
bool cst816_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    if (!CST816_status.inited)
    {
        ESP_LOGE(TAG, "Init first!");
        return 0x00;
    }
    uint8_t data_buf[6]; // 1 byte status, 2 bytes X, 2 bytes Y

    esp_err_t ret = lvgl_i2c_read(CONFIG_LV_I2C_TOUCH_PORT, 0x15, 0x01, &data_buf[0], 6);
    if (ret != ESP_OK)
    {
        //ESP_LOGE(TAG, "Error talking to touch IC: %s", esp_err_to_name(ret));
    }
    uint8_t touch_pnt_cnt = data_buf[1]; // Number of detected touch points

    if (ret != ESP_OK || touch_pnt_cnt != 1)
    { // ignore no touch & multi touch
        if (touch_inputs.current_state != LV_INDEV_STATE_REL)
        {
            touch_inputs.current_state = LV_INDEV_STATE_REL;
#if CONFIG_LV_CST816_COORDINATES_QUEUE
            xQueueOverwrite(CST816_touch_queue_handle, &touch_inputs);
#endif
        }
        data->point.x = touch_inputs.last_x;
        data->point.y = touch_inputs.last_y;
        data->state = touch_inputs.current_state;
        return false;
    }

    touch_inputs.current_state = LV_INDEV_STATE_PR;
    touch_inputs.last_x = ((data_buf[2] & CST816_MSB_MASK) << 8) | (data_buf[3] & CST816_LSB_MASK);
    touch_inputs.last_y = ((data_buf[4] & CST816_MSB_MASK) << 8) | (data_buf[5] & CST816_LSB_MASK);

// #if CONFIG_LV_CST816_SWAPXY
    int16_t swap_buf = touch_inputs.last_x;
    touch_inputs.last_x = touch_inputs.last_y;
    touch_inputs.last_y = swap_buf;
// #endif
// #if CONFIG_LV_CST816_INVERT_X
    touch_inputs.last_x = LV_HOR_RES - touch_inputs.last_x;
// #endif
#if CONFIG_LV_CST816_INVERT_Y
    touch_inputs.last_y = LV_VER_RES - touch_inputs.last_y;
#endif
    data->point.x = touch_inputs.last_x;
    data->point.y = touch_inputs.last_y;
    data->state = touch_inputs.current_state;
    ESP_LOGI(TAG, "X=%d Y=%d", data->point.x, data->point.y);

#if CONFIG_LV_CST816_COORDINATES_QUEUE
    xQueueOverwrite(CST816_touch_queue_handle, &touch_inputs);
#endif

    return false;
}
