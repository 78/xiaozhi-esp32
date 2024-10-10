/**
 * @file ILI9163C.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "ili9163c.h"
#include "disp_spi.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "assert.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "ILI9163C"

// ILI9163C specific commands used in init
#define ILI9163C_NOP 0x00
#define ILI9163C_SWRESET 0x01
#define ILI9163C_RDDID 0x04
#define ILI9163C_RDDST 0x09

#define ILI9163C_SLPIN 0x10
#define ILI9163C_SLPOUT 0x11
#define ILI9163C_PTLON 0x12
#define ILI9163C_NORON 0x13

#define ILI9163C_INVOFF 0x20
#define ILI9163C_INVON 0x21
#define ILI9163C_CMD_GAMST 0x26
#define ILI9163C_DISPOFF 0x28
#define ILI9163C_DISPON 0x29
#define ILI9163C_CASET 0x2A
#define ILI9163C_RASET 0x2B
#define ILI9163C_RAMWR 0x2C
#define ILI9163C_COLORSET 0x2D
#define ILI9163C_RAMRD 0x2E

#define ILI9163C_PTLAR 0x30
#define ILI9163C_VSCRDEF 0x33
#define ILI9163C_COLMOD 0x3A
#define ILI9163C_MADCTL 0x36
#define ILI9163C_VSCRSADD 0x37

#define ILI9163C_FRMCTR1 0xB1
#define ILI9163C_FRMCTR2 0xB2
#define ILI9163C_FRMCTR3 0xB3
#define ILI9163C_INVCTR 0xB4
#define ILI9163C_DISSET5 0xB6
#define ILI9163C_SDDC 0xB7

#define ILI9163C_PWCTR1 0xC0
#define ILI9163C_PWCTR2 0xC1
#define ILI9163C_PWCTR3 0xC2
#define ILI9163C_PWCTR4 0xC3
#define ILI9163C_PWCTR5 0xC4
#define ILI9163C_VMCTR1 0xC5
#define ILI9163C_VMCOFFS 0xC7

#define ILI9163C_GAMCTL 0xF2

#define ILI9163C_GMCTRP1 0xE0
#define ILI9163C_GMCTRN1 0xE1

#define ST77XX_MADCTL_MY 0x80
#define ST77XX_MADCTL_MX 0x40
#define ST77XX_MADCTL_MV 0x20 #define
#define ST77XX_MADCTL_ML 0x10
#define ST77XX_MADCTL_RGB 0x00
#define ST77XX_MADCTL_BGR 0x08

/**********************
 *      TYPEDEFS
 **********************/

/*The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct. */
typedef struct
{
	uint8_t cmd;
	uint8_t data[16];
	uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void ili9163c_set_orientation(uint8_t orientation);

static void ili9163c_send_cmd(uint8_t cmd);
static void ili9163c_send_data(void *data, uint16_t length);
static void ili9163c_send_color(void *data, uint16_t length);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ili9163c_init(void)
{
	ESP_LOGD(TAG, "Init");

	lcd_init_cmd_t ili_init_cmds[] = {
		{ILI9163C_SWRESET, {0}, 0x80},		 // Software reset, 0 args, w/delay 120ms
		{ILI9163C_SLPOUT, {0}, 0x80},		 // Out of sleep mode, 0 args, w/delay 5ms
		{ILI9163C_CMD_GAMST, {0x04}, 1},	 // Gamma Curve
		{ILI9163C_FRMCTR1, {0x0C, 0x14}, 2}, // Frame rate ctrl - normal mode
		{ILI9163C_INVCTR, {0x07}, 1},		 // Display inversion ctrl, 1 arg, no delay:No inversion
		{ILI9163C_PWCTR1, {0x0C, 0x05}, 2},	 // Power control, 2 args, no delay
		{ILI9163C_PWCTR2, {0x02}, 1},		 // Power control, 1 arg
		{ILI9163C_PWCTR3, {0x02}, 1},		 // Power control, 1 arg
		{ILI9163C_VMCTR1, {0x20, 0x55}, 2},	 // Power control, 1 arg, no delay:
		{ILI9163C_VMCOFFS, {0x40}, 1},		 // VCOM Offset
#if ILI9163C_INVERT_COLORS == 1
		{ILI9163C_INVON, {0}, 0}, // set inverted mode
#else
		{ILI9163C_INVOFF, {0}, 0}, // set non-inverted mode
#endif
		{ILI9163C_COLMOD, {0x5}, 1}, // set color mode, 1 arg, no delay: 16-bit color
		{ILI9163C_SDDC, {0}, 1},	  // set source driver direction control
		{ILI9163C_GAMCTL, {0x01}, 1}, // set source driver direction control
		{ILI9163C_GMCTRP1, {0x36, 0x29, 0x12, 0x22, 0x1C, 0x15, 0x42, 0xB7, 0x2F, 0x13, 0x12, 0x0A, 0x11, 0x0B, 0x06}, 16}, // 16 args, no delay:
		{ILI9163C_GMCTRN1, {0x09, 0x16, 0x2D, 0x0D, 0x13, 0x15, 0x40, 0x48, 0x53, 0x0C, 0x1D, 0x25, 0x2E, 0x34, 0x39}, 16}, // 16 args, no delay:
		{ILI9163C_NORON, {0}, 0x80},																						// Normal display on, no args, w/delay 10 ms delay
		{ILI9163C_DISPON, {0}, 0x80},																						// Main screen turn on, no args w/delay 100 ms delay
		{0, {0}, 0xff}
	};

	//Initialize non-SPI GPIOs
	gpio_pad_select_gpio(ILI9163C_DC);
	gpio_set_direction(ILI9163C_DC, GPIO_MODE_OUTPUT);
	gpio_pad_select_gpio(ILI9163C_RST);
	gpio_set_direction(ILI9163C_RST, GPIO_MODE_OUTPUT);

#if ILI9163C_ENABLE_BACKLIGHT_CONTROL
	gpio_pad_select_gpio(ILI9163C_BCKL);
	gpio_set_direction(ILI9163C_BCKL, GPIO_MODE_OUTPUT);
#endif
	//Reset the display
	gpio_set_level(ILI9163C_RST, 0);
	vTaskDelay(100 / portTICK_RATE_MS);
	gpio_set_level(ILI9163C_RST, 1);
	vTaskDelay(150 / portTICK_RATE_MS);

	//Send all the commands
	uint16_t cmd = 0;
	while (ili_init_cmds[cmd].databytes != 0xff)
	{
		ili9163c_send_cmd(ili_init_cmds[cmd].cmd);
		ili9163c_send_data(ili_init_cmds[cmd].data, ili_init_cmds[cmd].databytes & 0x1F);
		if (ili_init_cmds[cmd].databytes & 0x80)
		{
			vTaskDelay(150 / portTICK_RATE_MS);
		}
		cmd++;
	}

	ili9163c_enable_backlight(true);

	ili9163c_set_orientation(CONFIG_LV_DISPLAY_ORIENTATION);
}

void ili9163c_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
	uint8_t data[4];

	/*Column addresses*/
	ili9163c_send_cmd(ILI9163C_CASET);
	data[0] = (area->x1 >> 8) & 0xFF;
	data[1] = area->x1 & 0xFF;
	data[2] = (area->x2 >> 8) & 0xFF;
	data[3] = area->x2 & 0xFF;
	ili9163c_send_data(data, 4);

	/*Page addresses*/
	ili9163c_send_cmd(ILI9163C_RASET);
	data[0] = (area->y1 >> 8) & 0xFF;
	data[1] = area->y1 & 0xFF;
	data[2] = (area->y2 >> 8) & 0xFF;
	data[3] = area->y2 & 0xFF;
	ili9163c_send_data(data, 4);

	/*Memory write*/
	ili9163c_send_cmd(ILI9163C_RAMWR);

	uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);

	ili9163c_send_color((void *)color_map, size * 2);
}

void ili9163c_enable_backlight(bool backlight)
{
#if ILI9163C_ENABLE_BACKLIGHT_CONTROL
	ESP_LOGD(TAG, "%s backlight.", backlight ? "Enabling" : "Disabling");
	uint32_t tmp = 0;

#if (ILI9163C_BCKL_ACTIVE_LVL == 1)
	tmp = backlight ? 1 : 0;
#else
	tmp = backlight ? 0 : 1;
#endif

	gpio_set_level(ILI9163C_BCKL, tmp);
#endif
}

void ili9163c_sleep_in()
{
	uint8_t data[] = {0x08};
	ili9163c_send_cmd(ILI9163C_SLPIN);
	ili9163c_send_data(&data, 1);
}

void ili9163c_sleep_out()
{
	uint8_t data[] = {0x08};
	ili9163c_send_cmd(ILI9163C_SLPOUT);
	ili9163c_send_data(&data, 1);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void ili9163c_send_cmd(uint8_t cmd)
{
	disp_wait_for_pending_transactions();
	gpio_set_level(ILI9163C_DC, 0); /*Command mode*/
	disp_spi_send_data(&cmd, 1);
}

static void ili9163c_send_data(void *data, uint16_t length)
{
	disp_wait_for_pending_transactions();
	gpio_set_level(ILI9163C_DC, 1); /*Data mode*/
	disp_spi_send_data(data, length);
}

static void ili9163c_send_color(void *data, uint16_t length)
{
	disp_wait_for_pending_transactions();
	gpio_set_level(ILI9163C_DC, 1); /*Data mode*/
	disp_spi_send_colors(data, length);
}

static void ili9163c_set_orientation(uint8_t orientation)
{
	assert(orientation < 4);

	const char *orientation_str[] = {
		"PORTRAIT", "PORTRAIT_INVERTED", "LANDSCAPE", "LANDSCAPE_INVERTED"};

	ESP_LOGD(TAG, "Display orientation: %s", orientation_str[orientation]);

	uint8_t data[] = {0x48, 0x88, 0xA8, 0x68};

	ili9163c_send_cmd(ILI9163C_MADCTL);
	ili9163c_send_data((void *)&data[orientation], 1);
}
