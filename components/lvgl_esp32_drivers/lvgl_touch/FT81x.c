/**
 * @file FT81x.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include <stddef.h>

#include "FT81x.h"

#include "../lvgl_tft/EVE.h"
#include "../lvgl_tft/EVE_commands.h"


/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/


/**
 * Get the current position and state of the touchpad
 * @param data store the read data here
 * @return false: because no more data to be read
 */
bool FT81x_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    static int16_t last_x = 0;
    static int16_t last_y = 0;
   	bool touched = true;

	uint32_t XY = EVE_memRead32(REG_TOUCH_SCREEN_XY);
	uint16_t Y = XY & 0xffff;
	uint16_t X = XY >> 16;

	// is it not touched (or invalid because of calibration range)
	if(X == 0x8000 || Y == 0x8000 || X > LV_HOR_RES_MAX || Y > LV_VER_RES_MAX)
	{
		touched = false;
		X = last_x;
		Y = last_y;
	}
	else
	{
		last_x = X;
		last_y = Y;
	}

    data->point.x = X;
    data->point.y = Y;
    data->state = (touched == false ? LV_INDEV_STATE_REL : LV_INDEV_STATE_PR);

    return false;
}


/**********************
 *   STATIC FUNCTIONS
 **********************/
