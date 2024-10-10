#ifndef __cst816_H
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

#define __cst816_H

#include <stdint.h>
#include <stdbool.h>
#if CONFIG_LV_CST816_COORDINATES_QUEUE
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


#define CST816S_ADDRESS     0x15

enum GESTURE {
  NONE = 0x00,
  SWIPE_UP = 0x01,
  SWIPE_DOWN = 0x02,
  SWIPE_LEFT = 0x03,
  SWIPE_RIGHT = 0x04,
  SINGLE_CLICK = 0x05,
  DOUBLE_CLICK = 0x0B,
  LONG_PRESS = 0x0C

};

/* Maximum border values of the touchscreen pad that the chip can handle */
#define  CST816_MAX_WIDTH              ((uint16_t)240)
#define  CST816_MAX_HEIGHT             ((uint16_t)320)

/* Max detectable simultaneous touch points */
#define CST816_MAX_TOUCH_PNTS     2

/* Register of the current mode */
#define CST816_DEV_MODE_REG             0x00

/* Possible modes as of CST816_DEV_MODE_REG */
#define CST816_DEV_MODE_WORKING         0x00
#define CST816_DEV_MODE_FACTORY         0x04

#define CST816_DEV_MODE_MASK            0x70
#define CST816_DEV_MODE_SHIFT           4

/* Gesture ID register */
#define CST816_GEST_ID_REG              0x01

/* Possible values returned by CST816_GEST_ID_REG */
#define CST816_GEST_ID_NO_GESTURE       0x00
#define CST816_GEST_ID_MOVE_UP          0x10
#define CST816_GEST_ID_MOVE_RIGHT       0x14
#define CST816_GEST_ID_MOVE_DOWN        0x18
#define CST816_GEST_ID_MOVE_LEFT        0x1C
#define CST816_GEST_ID_ZOOM_IN          0x48
#define CST816_GEST_ID_ZOOM_OUT         0x49

/* Status register: stores number of active touch points (0, 1, 2) */
#define CST816_TD_STAT_REG              0x02
#define CST816_TD_STAT_MASK             0x0F
#define CST816_TD_STAT_SHIFT            0x00

/* Touch events */
#define CST816_TOUCH_EVT_FLAG_PRESS_DOWN 0x00
#define CST816_TOUCH_EVT_FLAG_LIFT_UP    0x01
#define CST816_TOUCH_EVT_FLAG_CONTACT    0x02
#define CST816_TOUCH_EVT_FLAG_NO_EVENT   0x03

#define CST816_TOUCH_EVT_FLAG_SHIFT     6
#define CST816_TOUCH_EVT_FLAG_MASK      (3 << CST816_TOUCH_EVT_FLAG_SHIFT)

#define CST816_MSB_MASK                 0x0F
#define CST816_MSB_SHIFT                0
#define CST816_LSB_MASK                 0xFF
#define CST816_LSB_SHIFT                0

#define CST816_P1_XH_REG                0x03
#define CST816_P1_XL_REG                0x04
#define CST816_P1_YH_REG                0x05
#define CST816_P1_YL_REG                0x06

#define CST816_P1_WEIGHT_REG            0x07    /* Register reporting touch pressure - read only */
#define CST816_TOUCH_WEIGHT_MASK        0xFF
#define CST816_TOUCH_WEIGHT_SHIFT       0

#define CST816_P1_MISC_REG              0x08    /* Touch area register */

#define CST816_TOUCH_AREA_MASK         (0x04 << 4)  /* Values related to CST816_Pn_MISC_REG */
#define CST816_TOUCH_AREA_SHIFT        0x04

#define CST816_P2_XH_REG               0x09
#define CST816_P2_XL_REG               0x0A
#define CST816_P2_YH_REG               0x0B
#define CST816_P2_YL_REG               0x0C
#define CST816_P2_WEIGHT_REG           0x0D
#define CST816_P2_MISC_REG             0x0E

/* Threshold for touch detection */
#define CST816_TH_GROUP_REG            0x80
#define CST816_THRESHOLD_MASK          0xFF          /* Values CST816_TH_GROUP_REG : threshold related  */
#define CST816_THRESHOLD_SHIFT         0

#define CST816_TH_DIFF_REG             0x85          /* Filter function coefficients */

#define CST816_CTRL_REG                0x86            /* Control register */

#define CST816_CTRL_KEEP_ACTIVE_MODE    0x00        /* Will keep the Active mode when there is no touching */
#define CST816_CTRL_KEEP_AUTO_SWITCH_MONITOR_MODE  0x01 /* Switching from Active mode to Monitor mode automatically when there is no touching */

#define CST816_TIME_ENTER_MONITOR_REG     0x87       /* The time period of switching from Active mode to Monitor mode when there is no touching */

#define CST816_PERIOD_ACTIVE_REG         0x88        /* Report rate in Active mode */
#define CST816_PERIOD_MONITOR_REG        0x89        /* Report rate in Monitor mode */

#define CST816_RADIAN_VALUE_REG         0x91        /* The value of the minimum allowed angle while Rotating gesture mode */

#define CST816_OFFSET_LEFT_RIGHT_REG    0x92        /* Maximum offset while Moving Left and Moving Right gesture */
#define CST816_OFFSET_UP_DOWN_REG       0x93        /* Maximum offset while Moving Up and Moving Down gesture */

#define CST816_DISTANCE_LEFT_RIGHT_REG  0x94        /* Minimum distance while Moving Left and Moving Right gesture */
#define CST816_DISTANCE_UP_DOWN_REG     0x95        /* Minimum distance while Moving Up and Moving Down gesture */

#define CST816_LIB_VER_H_REG            0xA1        /* High 8-bit of LIB Version info */
#define CST816_LIB_VER_L_REG            0xA2        /* Low 8-bit of LIB Version info */

#define CST816_CHIPSELECT_REG            0xA3       /* 0x36 for ft6236; 0x06 for ft6206 */

#define CST816_POWER_MODE_REG            0xA5
#define CST816_FIRMWARE_ID_REG           0xA6
#define CST816_RELEASECODE_REG           0xAF
#define CST816_PANEL_ID_REG              0xA8
#define CST816_OPMODE_REG                0xBC


#define TP_REG_GESTURE				0x01
#define TP_REG_FINGER_NUM			0x02
#define TP_REG_XPOS_H				0x03
#define TP_REG_XPOS_L				0x04
#define TP_REG_YPOS_H				0x05
#define TP_REG_YPOS_L				0x06
#define TP_REG_BPC0_H				0xB0
#define TP_REG_BPC0_L				0xB1
#define TP_REG_BPC1_H				0xB2
#define TP_REG_BPC1_L				0xB3
#define TP_REG_CHIPID				0xA7
#define TP_REG_PROJID				0xA8
#define TP_REG_FW_VER				0xA9
#define TP_REG_SLEEP_MODE			0xE5
#define TP_REG_ERR_RESET			0xEA
#define TP_REG_LONG_PRESS_TICK		0xEB
#define TP_REG_MOTION_MASK			0xEC
#define TP_REG_IRQ_PLUSE_WIDTH		0xED
#define TP_REG_NOR_SCAN_PER			0xEE
#define TP_REG_MOTION_SL_ANGLE		0xEF
#define TP_REG_LP_SCAN_RAW1_H		0xF0
#define TP_REG_LP_SCAN_RAW1_L		0xF1
#define TP_REG_LP_SCAN_RAW2_H		0xF2
#define TP_REG_LP_SCAN_RAW2_L		0xF3
#define TP_REG_LP_AUTO_WAKE_TIME	0xF4
#define TP_REG_LP_SCAN_TH			0xF5
#define TP_REG_LP_SCAN_WIN			0xF6
#define TP_REG_LP_SCAN_FREQ			0xF7
#define TP_REG_LP_SCAN_IDAC			0xF8
#define TP_REG_AUTO_SLEEP_TIME		0xF9
#define TP_REG_IRQ_CTL				0xFA
#define TP_REG_AUTO_RESET			0xFB
#define TP_REG_LONG_PRESS_TIME		0xFC
#define TP_REG_IO_CTL				0xFD
#define TP_REG_DIS_AUTO_SLEEP		0xFE

typedef struct {
    bool inited;
} CST816_status_t;

typedef struct
{
  int16_t last_x;
  int16_t last_y;
  lv_indev_state_t current_state;
} CST816_touch_t;

#if CONFIG_LV_CST816_COORDINATES_QUEUE
extern QueueHandle_t CST816_touch_queue_handle;
#endif
/**
  * @brief  Initialize for CST816 communication via I2C
  * @param  dev_addr: Device address on communication Bus (I2C slave address of CST816).
  * @retval None
  */
void cst816_init(uint16_t dev_addr);

uint8_t cst816_get_gesture_id();

/**
  * @brief  Get the touch screen X and Y positions values. Ignores multi touch
  * @param  drv:
  * @param  data: Store data here
  * @retval Always false
  */
bool cst816_read(lv_indev_drv_t *drv, lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif
#endif /* __cst816_H */
