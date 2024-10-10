/*
 * @Author: Kevincoooool
 * @Date: 2022-07-27 17:00:17
 * @Description: 
 * @version:  
 * @Filename: Do not Edit
 * @LastEditTime: 2022-07-27 17:39:50
 * @FilePath: \10.adc_button_test\components\lvgl_esp32_drivers\lvgl_touch\touch_driver.c
 */
/**
 * @file touch_driver.c
 */

#include "touch_driver.h"
#include "tp_spi.h"


void touch_driver_init(void)
{
#if defined (CONFIG_LV_TOUCH_CONTROLLER_XPT2046)
    xpt2046_init();
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_FT6X06)
    ft6x06_init(FT6236_I2C_SLAVE_ADDR);
    #elif defined (CONFIG_LV_TOUCH_CONTROLLER_CST816)
    cst816_init(CST816S_ADDRESS);
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_STMPE610)
    stmpe610_init();
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_ADCRAW)
    adcraw_init();
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_FT81X)
    /* nothing to do */
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_RA8875)
    ra8875_touch_init();
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_GT911)
    gt911_init(GT911_I2C_SLAVE_ADDR);
#endif
}

#if LVGL_VERSION_MAJOR >= 8
void touch_driver_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
#else
bool touch_driver_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
#endif
{
    bool res = false;

#if defined (CONFIG_LV_TOUCH_CONTROLLER_XPT2046)
    res = xpt2046_read(drv, data);
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_FT6X06)
    res = ft6x36_read(drv, data);
    #elif defined (CONFIG_LV_TOUCH_CONTROLLER_CST816)
    res = cst816_read(drv, data);
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_STMPE610)
    res = stmpe610_read(drv, data);
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_ADCRAW)
    res = adcraw_read(drv, data);
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_FT81X)
    res = FT81x_read(drv, data);
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_RA8875)
    res = ra8875_touch_read(drv, data);
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_GT911)
    res = gt911_read(drv, data);
#endif

#if LVGL_VERSION_MAJOR >= 8
    data->continue_reading = res;
#else
    return res;
#endif
}

