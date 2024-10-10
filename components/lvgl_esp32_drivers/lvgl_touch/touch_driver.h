/*
 * @Author: Kevincoooool
 * @Date: 2022-07-27 17:00:17
 * @Description: 
 * @version:  
 * @Filename: Do not Edit
 * @LastEditTime: 2022-07-27 17:40:07
 * @FilePath: \10.adc_button_test\components\lvgl_esp32_drivers\lvgl_touch\touch_driver.h
 */
/**
 * @file touch_driver.h
 */

#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <stdbool.h>
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#if defined (CONFIG_LV_TOUCH_CONTROLLER_XPT2046)
#include "xpt2046.h"
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_FT6X06)
#include "ft6x36.h"
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_CST816)
#include "cst816.h"
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_STMPE610)
#include "stmpe610.h"
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_ADCRAW)
#include "adcraw.h"
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_FT81X)
#include "FT81x.h"
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_RA8875)
#include "ra8875_touch.h"
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_GT911)
#include "gt911.h"
#endif

/*********************
*      DEFINES
*********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void touch_driver_init(void);

#if LVGL_VERSION_MAJOR >= 8
void touch_driver_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
#else
bool touch_driver_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TOUCH_DRIVER_H */

