/**
 * @file ili9481.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "ili9481.h"
#include "disp_spi.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*********************
 *      DEFINES
 *********************/
 #define TAG "ILI9481"

/**********************
 *      TYPEDEFS
 **********************/

/*The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct. */
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void ili9481_set_orientation(uint8_t orientation);
static void ili9481_send_cmd(uint8_t cmd);
static void ili9481_send_data(void * data, uint16_t length);
static void ili9481_send_color(void * data, uint16_t length);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ili9481_init(void)
{
    lcd_init_cmd_t ili_init_cmds[]={
        {ILI9481_CMD_SLEEP_OUT, {0x00}, 0x80},
        {ILI9481_CMD_POWER_SETTING, {0x07, 0x42, 0x18}, 3},
        {ILI9481_CMD_VCOM_CONTROL, {0x00, 0x07, 0x10}, 3},
        {ILI9481_CMD_POWER_CONTROL_NORMAL, {0x01, 0x02}, 2},
        {ILI9481_CMD_PANEL_DRIVE, {0x10, 0x3B, 0x00, 0x02, 0x11}, 5},
        {ILI9481_CMD_FRAME_RATE, {0x03}, 1},
        {ILI9481_CMD_FRAME_MEMORY_ACCESS, {0x0, 0x0, 0x0, 0x0}, 4},
        //{ILI9481_CMD_DISP_TIMING_NORMAL, {0x10, 0x10, 0x22}, 3},
        {ILI9481_CMD_GAMMA_SETTING, {0x00, 0x32, 0x36, 0x45, 0x06, 0x16, 0x37, 0x75, 0x77, 0x54, 0x0C, 0x00}, 12},
        {ILI9481_CMD_MEMORY_ACCESS_CONTROL, {0x0A}, 1},
#if ILI9481_INVERT_COLORS
        {ILI9481_CMD_DISP_INVERSION_ON, {}, 0},
#endif
        {ILI9481_CMD_COLMOD_PIXEL_FORMAT_SET, {0x66}, 1},
        {ILI9481_CMD_NORMAL_DISP_MODE_ON, {}, 0x80},
        {ILI9481_CMD_DISPLAY_ON, {}, 0x80},
        {0, {0}, 0xff},
    };

    //Initialize non-SPI GPIOs
    gpio_pad_select_gpio(ILI9481_DC);
    gpio_set_direction(ILI9481_DC, GPIO_MODE_OUTPUT);

#if ILI9481_USE_RST
    gpio_pad_select_gpio(ILI9481_RST);
    gpio_set_direction(ILI9481_RST, GPIO_MODE_OUTPUT);
#endif

#if ILI9481_ENABLE_BACKLIGHT_CONTROL
    gpio_pad_select_gpio(ILI9481_BCKL);
    gpio_set_direction(ILI9481_BCKL, GPIO_MODE_OUTPUT);
#endif

#if ILI9481_USE_RST
    //Reset the display
    gpio_set_level(ILI9481_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(ILI9481_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
#endif

    ESP_LOGI(TAG, "ILI9481 initialization.");

    // Exit sleep
    ili9481_send_cmd(0x01);	/* Software reset */
    vTaskDelay(100 / portTICK_RATE_MS);

    //Send all the commands
    uint16_t cmd = 0;
    while (ili_init_cmds[cmd].databytes!=0xff) {
        ili9481_send_cmd(ili_init_cmds[cmd].cmd);
        ili9481_send_data(ili_init_cmds[cmd].data, ili_init_cmds[cmd].databytes&0x1F);
        if (ili_init_cmds[cmd].databytes & 0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }

    ili9481_enable_backlight(true);

    ili9481_set_orientation(ILI9481_DISPLAY_ORIENTATION);
}

// Flush function based on mvturnho repo
void ili9481_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
    uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);

    lv_color16_t *buffer_16bit = (lv_color16_t *) color_map;
    uint8_t *mybuf;
    do {
        mybuf = (uint8_t *) heap_caps_malloc(3 * size * sizeof(uint8_t), MALLOC_CAP_DMA);
        if (mybuf == NULL)  ESP_LOGW(TAG, "Could not allocate enough DMA memory!");
    } while (mybuf == NULL);

    uint32_t LD = 0;
    uint32_t j = 0;

    for (uint32_t i = 0; i < size; i++) {
        LD = buffer_16bit[i].full;
        mybuf[j] = (uint8_t) (((LD & 0xF800) >> 8) | ((LD & 0x8000) >> 13));
        j++;
        mybuf[j] = (uint8_t) ((LD & 0x07E0) >> 3);
        j++;
        mybuf[j] = (uint8_t) (((LD & 0x001F) << 3) | ((LD & 0x0010) >> 2));
        j++;
    }

    /* Column addresses  */
    uint8_t xb[] = {
        (uint8_t) (area->x1 >> 8) & 0xFF,
        (uint8_t) (area->x1) & 0xFF,
        (uint8_t) (area->x2 >> 8) & 0xFF,
        (uint8_t) (area->x2) & 0xFF,
    };

    /* Page addresses  */
    uint8_t yb[] = {
        (uint8_t) (area->y1 >> 8) & 0xFF,
        (uint8_t) (area->y1) & 0xFF,
        (uint8_t) (area->y2 >> 8) & 0xFF,
        (uint8_t) (area->y2) & 0xFF,
    };

    /*Column addresses*/
    ili9481_send_cmd(ILI9481_CMD_COLUMN_ADDRESS_SET);
    ili9481_send_data(xb, 4);

    /*Page addresses*/
    ili9481_send_cmd(ILI9481_CMD_PAGE_ADDRESS_SET);
    ili9481_send_data(yb, 4);

    /*Memory write*/
    ili9481_send_cmd(ILI9481_CMD_MEMORY_WRITE);

    ili9481_send_color((void *) mybuf, size * 3);
    heap_caps_free(mybuf);
}

void ili9481_enable_backlight(bool backlight)
{
#if ILI9481_ENABLE_BACKLIGHT_CONTROL
    ESP_LOGI(TAG, "%s backlight.", backlight ? "Enabling" : "Disabling");
    uint32_t tmp = 0;

#if (ILI9481_BCKL_ACTIVE_LVL==1)
    tmp = backlight ? 1 : 0;
#else
    tmp = backlight ? 0 : 1;
#endif

    gpio_set_level(ILI9481_BCKL, tmp);
#endif
}

/**********************
 *   STATIC FUNCTIONS
 **********************/


static void ili9481_send_cmd(uint8_t cmd)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ILI9481_DC, 0);	 /*Command mode*/
    disp_spi_send_data(&cmd, 1);
}

static void ili9481_send_data(void * data, uint16_t length)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ILI9481_DC, 1);	 /*Data mode*/
    disp_spi_send_data(data, length);
}

static void ili9481_send_color(void * data, uint16_t length)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ILI9481_DC, 1);   /*Data mode*/
    disp_spi_send_colors(data, length);
}

static void ili9481_set_orientation(uint8_t orientation)
{
    const char *orientation_str[] = {
        "PORTRAIT", "PORTRAIT_INVERTED", "LANDSCAPE", "LANDSCAPE_INVERTED"
    };

    ESP_LOGI(TAG, "Display orientation: %s", orientation_str[orientation]);

    uint8_t data[] = {0x48, 0x4B, 0x28, 0x2B};
    ili9481_send_cmd(ILI9481_CMD_MEMORY_ACCESS_CONTROL);
    ili9481_send_data((void *) &data[orientation], 1);
}
