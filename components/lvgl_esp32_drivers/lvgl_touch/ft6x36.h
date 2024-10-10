#ifndef __FT6X06_H
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

#define __FT6X06_H

#include <stdint.h>
#include <stdbool.h>
#if CONFIG_LV_FT6X36_COORDINATES_QUEUE
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#endif
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define FT6236_I2C_SLAVE_ADDR   0x38

/* Maximum border values of the touchscreen pad that the chip can handle */
#define  FT6X36_MAX_WIDTH              ((uint16_t)240)
#define  FT6X36_MAX_HEIGHT             ((uint16_t)320)

/* Max detectable simultaneous touch points */
#define FT6X36_MAX_TOUCH_PNTS     2

/* Register of the current mode */
#define FT6X36_DEV_MODE_REG             0x00

/* Possible modes as of FT6X36_DEV_MODE_REG */
#define FT6X36_DEV_MODE_WORKING         0x00
#define FT6X36_DEV_MODE_FACTORY         0x04

#define FT6X36_DEV_MODE_MASK            0x70
#define FT6X36_DEV_MODE_SHIFT           4

/* Gesture ID register */
#define FT6X36_GEST_ID_REG              0x01

/* Possible values returned by FT6X36_GEST_ID_REG */
#define FT6X36_GEST_ID_NO_GESTURE       0x00
#define FT6X36_GEST_ID_MOVE_UP          0x10
#define FT6X36_GEST_ID_MOVE_RIGHT       0x14
#define FT6X36_GEST_ID_MOVE_DOWN        0x18
#define FT6X36_GEST_ID_MOVE_LEFT        0x1C
#define FT6X36_GEST_ID_ZOOM_IN          0x48
#define FT6X36_GEST_ID_ZOOM_OUT         0x49

/* Status register: stores number of active touch points (0, 1, 2) */
#define FT6X36_TD_STAT_REG              0x02
#define FT6X36_TD_STAT_MASK             0x0F
#define FT6X36_TD_STAT_SHIFT            0x00

/* Touch events */
#define FT6X36_TOUCH_EVT_FLAG_PRESS_DOWN 0x00
#define FT6X36_TOUCH_EVT_FLAG_LIFT_UP    0x01
#define FT6X36_TOUCH_EVT_FLAG_CONTACT    0x02
#define FT6X36_TOUCH_EVT_FLAG_NO_EVENT   0x03

#define FT6X36_TOUCH_EVT_FLAG_SHIFT     6
#define FT6X36_TOUCH_EVT_FLAG_MASK      (3 << FT6X36_TOUCH_EVT_FLAG_SHIFT)

#define FT6X36_MSB_MASK                 0x0F
#define FT6X36_MSB_SHIFT                0
#define FT6X36_LSB_MASK                 0xFF
#define FT6X36_LSB_SHIFT                0

#define FT6X36_P1_XH_REG                0x03
#define FT6X36_P1_XL_REG                0x04
#define FT6X36_P1_YH_REG                0x05
#define FT6X36_P1_YL_REG                0x06

#define FT6X36_P1_WEIGHT_REG            0x07    /* Register reporting touch pressure - read only */
#define FT6X36_TOUCH_WEIGHT_MASK        0xFF
#define FT6X36_TOUCH_WEIGHT_SHIFT       0

#define FT6X36_P1_MISC_REG              0x08    /* Touch area register */

#define FT6X36_TOUCH_AREA_MASK         (0x04 << 4)  /* Values related to FT6X36_Pn_MISC_REG */
#define FT6X36_TOUCH_AREA_SHIFT        0x04

#define FT6X36_P2_XH_REG               0x09
#define FT6X36_P2_XL_REG               0x0A
#define FT6X36_P2_YH_REG               0x0B
#define FT6X36_P2_YL_REG               0x0C
#define FT6X36_P2_WEIGHT_REG           0x0D
#define FT6X36_P2_MISC_REG             0x0E

/* Threshold for touch detection */
#define FT6X36_TH_GROUP_REG            0x80
#define FT6X36_THRESHOLD_MASK          0xFF          /* Values FT6X36_TH_GROUP_REG : threshold related  */
#define FT6X36_THRESHOLD_SHIFT         0

#define FT6X36_TH_DIFF_REG             0x85          /* Filter function coefficients */

#define FT6X36_CTRL_REG                0x86            /* Control register */

#define FT6X36_CTRL_KEEP_ACTIVE_MODE    0x00        /* Will keep the Active mode when there is no touching */
#define FT6X36_CTRL_KEEP_AUTO_SWITCH_MONITOR_MODE  0x01 /* Switching from Active mode to Monitor mode automatically when there is no touching */

#define FT6X36_TIME_ENTER_MONITOR_REG     0x87       /* The time period of switching from Active mode to Monitor mode when there is no touching */

#define FT6X36_PERIOD_ACTIVE_REG         0x88        /* Report rate in Active mode */
#define FT6X36_PERIOD_MONITOR_REG        0x89        /* Report rate in Monitor mode */

#define FT6X36_RADIAN_VALUE_REG         0x91        /* The value of the minimum allowed angle while Rotating gesture mode */

#define FT6X36_OFFSET_LEFT_RIGHT_REG    0x92        /* Maximum offset while Moving Left and Moving Right gesture */
#define FT6X36_OFFSET_UP_DOWN_REG       0x93        /* Maximum offset while Moving Up and Moving Down gesture */

#define FT6X36_DISTANCE_LEFT_RIGHT_REG  0x94        /* Minimum distance while Moving Left and Moving Right gesture */
#define FT6X36_DISTANCE_UP_DOWN_REG     0x95        /* Minimum distance while Moving Up and Moving Down gesture */

#define FT6X36_LIB_VER_H_REG            0xA1        /* High 8-bit of LIB Version info */
#define FT6X36_LIB_VER_L_REG            0xA2        /* Low 8-bit of LIB Version info */

#define FT6X36_CHIPSELECT_REG            0xA3       /* 0x36 for ft6236; 0x06 for ft6206 */

#define FT6X36_POWER_MODE_REG            0xA5
#define FT6X36_FIRMWARE_ID_REG           0xA6
#define FT6X36_RELEASECODE_REG           0xAF
#define FT6X36_PANEL_ID_REG              0xA8
#define FT6X36_OPMODE_REG                0xBC


typedef struct {
    bool inited;
} ft6x36_status_t;

typedef struct
{
  int16_t last_x;
  int16_t last_y;
  lv_indev_state_t current_state;
} ft6x36_touch_t;

#if CONFIG_LV_FT6X36_COORDINATES_QUEUE
extern QueueHandle_t ft6x36_touch_queue_handle;
#endif
/**
  * @brief  Initialize for FT6x36 communication via I2C
  * @param  dev_addr: Device address on communication Bus (I2C slave address of FT6X36).
  * @retval None
  */
void ft6x06_init(uint16_t dev_addr);

uint8_t ft6x36_get_gesture_id();

/**
  * @brief  Get the touch screen X and Y positions values. Ignores multi touch
  * @param  drv:
  * @param  data: Store data here
  * @retval Always false
  */
bool ft6x36_read(lv_indev_drv_t *drv, lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif
#endif /* __FT6X06_H */
