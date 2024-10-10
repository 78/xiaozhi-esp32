/**
 * @file RA8875_TOUCH.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stddef.h>

#include "ra8875_touch.h"

#include "../lvgl_tft/ra8875.h"

#ifndef CONFIG_LV_TFT_DISPLAY_CONTROLLER_RA8875
    #error "Display controller must be RA8875"
#endif

/*********************
 *      DEFINES
 *********************/
#define DEBUG false
#define TAG "RA8875-Touch"

#define DIV_ROUND_UP(n, d) (((n)+(d)-1)/(d))

#define INTC2_TP_INT (0x04)

#define TPCR0_ADC_TIMING ((CONFIG_LV_TOUCH_RA8875_SAMPLE_TIME << 4) | CONFIG_LV_TOUCH_RA8875_ADC_CLOCK)
#if LVGL_TOUCH_RA8875_WAKEUP_ENABLE
    #define TPCR0_VAL (0x08 | TPCR0_ADC_TIMING)
#else
    #define TPCR0_VAL (TPCR0_ADC_TIMING)
#endif

#if LVGL_TOUCH_RA8875_EXTERNAL_VREF
    #if LVGL_TOUCH_RA8875_DEBOUNCE_ENABLE
        #define TPCR1_VAL (0x24)
    #else
        #define TPCR1_VAL (0x20)
    #endif
#else
    #if LVGL_TOUCH_RA8875_DEBOUNCE_ENABLE
        #define TPCR1_VAL (0x04)
    #else
        #define TPCR1_VAL (0x00)
    #endif
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void ra8875_corr(int * x, int * y);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ra8875_touch_init(void)
{
    struct {
        uint8_t cmd;                                       // Register address of command
        uint8_t data;                                      // Value to write to register
    } init_cmds[] = {
        {RA8875_REG_TPCR0, TPCR0_VAL},                     // Touch Panel Control Register 0 (TPCR0)
        {RA8875_REG_TPCR1, TPCR1_VAL},                     // Touch Panel Control Register 1 (TPCR1)
    };
    #define INIT_CMDS_SIZE (sizeof(init_cmds)/sizeof(init_cmds[0]))

    ESP_LOGI(TAG, "Initializing RA8875 Touch...");

    // Send all the commands
    for (unsigned int i = 0; i < INIT_CMDS_SIZE; i++) {
        ra8875_write_cmd(init_cmds[i].cmd, init_cmds[i].data);
    }
    ra8875_touch_enable(true);
}

void ra8875_touch_enable(bool enable)
{
    ESP_LOGI(TAG, "%s touch.", enable ? "Enabling" : "Disabling");
    uint8_t val = enable ? (0x80 | TPCR0_VAL) : (TPCR0_VAL);
    ra8875_write_cmd(RA8875_REG_TPCR0, val);
}

/**
 * Get the current position and state of the touchscreen
 * @param data store the read data here
 * @return false: because no more data to be read
 */
bool ra8875_touch_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    static int x = 0;
    static int y = 0;

    int intr = ra8875_read_cmd(RA8875_REG_INTC2);          // Interrupt Control Register2 (INTC2)

    data->state = (intr & INTC2_TP_INT) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

    if (data->state == LV_INDEV_STATE_PR) {
        x = ra8875_read_cmd(RA8875_REG_TPXH);              // Touch Panel X High Byte Data Register (TPXH)
        y = ra8875_read_cmd(RA8875_REG_TPYH);              // Touch Panel Y High Byte Data Register (TPYH)
        int xy = ra8875_read_cmd(RA8875_REG_TPXYL);        // Touch Panel X/Y Low Byte Data Register (TPXYL)

        x = (x << 2) | (xy & 0x03);
        y = (y << 2) | ((xy >> 2) & 0x03);

#if DEBUG
        ESP_LOGI(TAG, "Touch Poll Raw: %d,%d", x, y);
#endif

        // Convert to display coordinates
        ra8875_corr(&x, &y);

        // Clear interrupt
        ra8875_write_cmd(RA8875_REG_INTC2, INTC2_TP_INT);  // Interrupt Control Register2 (INTC2)
    }

    data->point.x = x;
    data->point.y = y;

#if DEBUG
        ESP_LOGI(TAG, "Touch Poll - Event: %d; %d,%d", data->state, data->point.x, data->point.y);
#endif

    return false;
}


/**********************
 *   STATIC FUNCTIONS
 **********************/

static void ra8875_corr(int * x, int * y)
{
#if RA8875_XY_SWAP != 0
    int tmp = *x;
    *x = *y;
    *y = tmp;
#endif

    if ((*x) <= RA8875_X_MIN) {
        (*x) = 0;
    } else if ((*x) >= RA8875_X_MAX) {
        (*x) = LV_HOR_RES-1;
    } else {
        (*x) = (((*x) - RA8875_X_MIN) * (LV_HOR_RES-1)) / (RA8875_X_MAX - RA8875_X_MIN);
    }

    if ((*y) <= RA8875_Y_MIN) {
        (*y) = 0;
    } else if ((*y) >= RA8875_Y_MAX) {
        (*y) = LV_VER_RES-1;
    } else {
        (*y) = (((*y) - RA8875_Y_MIN) * (LV_VER_RES-1)) / (RA8875_Y_MAX - RA8875_Y_MIN);
    }

#if RA8875_X_INV != 0
    (*x) = (LV_HOR_RES-1) - (*x);
#endif

#if RA8875_Y_INV != 0
    (*y) = (LV_VER_RES-1) - (*y);
#endif
}
