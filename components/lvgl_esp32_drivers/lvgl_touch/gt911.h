#ifndef __GT911_H
/*
* Copyright © 2021 Sturnus Inc.

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

#define __GT911_H

#include <stdint.h>
#include <stdbool.h>
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GT911_I2C_SLAVE_ADDR   0x14

#define GT911_PRODUCT_ID_LEN   4

/* Register Map of GT911 */
#define GT911_PRODUCT_ID1             0x8140
#define GT911_PRODUCT_ID2             0x8141
#define GT911_PRODUCT_ID3             0x8142
#define GT911_PRODUCT_ID4             0x8143
#define GT911_FIRMWARE_VER_L          0x8144
#define GT911_FIRMWARE_VER_H          0x8145
#define GT911_X_COORD_RES_L           0x8146
#define GT911_X_COORD_RES_H           0x8147
#define GT911_Y_COORD_RES_L           0x8148
#define GT911_Y_COORD_RES_H           0x8149
#define GT911_VENDOR_ID               0x814A

#define GT911_STATUS_REG              0x814E
#define   GT911_STATUS_REG_BUF        0x80
#define   GT911_STATUS_REG_LARGE      0x40
#define   GT911_STATUS_REG_PROX_VALID 0x20
#define   GT911_STATUS_REG_HAVEKEY    0x10
#define   GT911_STATUS_REG_PT_MASK    0x0F

#define GT911_TRACK_ID1               0x814F
#define GT911_PT1_X_COORD_L           0x8150
#define GT911_PT1_X_COORD_H           0x8151
#define GT911_PT1_Y_COORD_L           0x8152
#define GT911_PT1_Y_COORD_H           0x8153
#define GT911_PT1_X_SIZE_L            0x8154
#define GT911_PT1_X_SIZE_H            0x8155

typedef struct {
    bool inited;
    char product_id[GT911_PRODUCT_ID_LEN];
    uint16_t max_x_coord;
    uint16_t max_y_coord;
    uint8_t i2c_dev_addr;
} gt911_status_t;

/**
  * @brief  Initialize for GT911 communication via I2C
  * @param  dev_addr: Device address on communication Bus (I2C slave address of GT911).
  * @retval None
  */
void gt911_init(uint8_t dev_addr);

/**
  * @brief  Get the touch screen X and Y positions values. Ignores multi touch
  * @param  drv:
  * @param  data: Store data here
  * @retval Always false
  */
bool gt911_read(lv_indev_drv_t *drv, lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif
#endif /* __GT911_H */
