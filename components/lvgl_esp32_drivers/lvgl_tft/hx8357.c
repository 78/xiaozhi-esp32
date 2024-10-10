/**
 * @file HX8357.c
 *
 * Roughly based on the Adafruit_HX8357_Library
 *
 * This library should work with:
 * Adafruit 3.5" TFT 320x480 + Touchscreen Breakout
 *    http://www.adafruit.com/products/2050
 *
 * Adafruit TFT FeatherWing - 3.5" 480x320 Touchscreen for Feathers
 *    https://www.adafruit.com/product/3651
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "hx8357.h"
#include "disp_spi.h"
#include "driver/gpio.h"
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*********************
 *      DEFINES
 *********************/
#define TAG        "HX8357"

#define MADCTL_MY  0x80 ///< Bottom to top
#define MADCTL_MX  0x40 ///< Right to left
#define MADCTL_MV  0x20 ///< Reverse Mode
#define MADCTL_ML  0x10 ///< LCD refresh Bottom to top
#define MADCTL_RGB 0x00 ///< Red-Green-Blue pixel order
#define MADCTL_BGR 0x08 ///< Blue-Green-Red pixel order
#define MADCTL_MH  0x04 ///< LCD refresh right to left

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
static void hx8357_send_cmd(uint8_t cmd);
static void hx8357_send_data(void * data, uint16_t length);
static void hx8357_send_color(void * data, uint16_t length);


/**********************
 *  INITIALIZATION ARRAYS
 **********************/
// Taken from the Adafruit driver
static const uint8_t
  initb[] = {
    HX8357B_SETPOWER, 3,
      0x44, 0x41, 0x06,
    HX8357B_SETVCOM, 2,
      0x40, 0x10,
    HX8357B_SETPWRNORMAL, 2,
      0x05, 0x12,
    HX8357B_SET_PANEL_DRIVING, 5,
      0x14, 0x3b, 0x00, 0x02, 0x11,
    HX8357B_SETDISPLAYFRAME, 1,
      0x0c,                      // 6.8mhz
    HX8357B_SETPANELRELATED, 1,
      0x01,                      // BGR
    0xEA, 3,                     // seq_undefined1, 3 args
      0x03, 0x00, 0x00,
    0xEB, 4,                     // undef2, 4 args
      0x40, 0x54, 0x26, 0xdb,
    HX8357B_SETGAMMA, 12,
      0x00, 0x15, 0x00, 0x22, 0x00, 0x08, 0x77, 0x26, 0x66, 0x22, 0x04, 0x00,
    HX8357_MADCTL, 1,
      0xC0,
    HX8357_COLMOD, 1,
      0x55,
    HX8357_PASET, 4,
      0x00, 0x00, 0x01, 0xDF,
    HX8357_CASET, 4,
      0x00, 0x00, 0x01, 0x3F,
    HX8357B_SETDISPMODE, 1,
      0x00,                      // CPU (DBI) and internal oscillation ??
    HX8357_SLPOUT, 0x80 + 120/5, // Exit sleep, then delay 120 ms
    HX8357_DISPON, 0x80 +  10/5, // Main screen turn on, delay 10 ms
    0                            // END OF COMMAND LIST
  }, initd[] = {
    HX8357_SWRESET, 0x80 + 100/5, // Soft reset, then delay 10 ms
    HX8357D_SETC, 3,
      0xFF, 0x83, 0x57,
    0xFF, 0x80 + 500/5,          // No command, just delay 300 ms
    HX8357_SETRGB, 4,
      0x80, 0x00, 0x06, 0x06,    // 0x80 enables SDO pin (0x00 disables)
    HX8357D_SETCOM, 1,
      0x25,                      // -1.52V
    HX8357_SETOSC, 1,
      0x68,                      // Normal mode 70Hz, Idle mode 55 Hz
    HX8357_SETPANEL, 1,
      0x05,                      // BGR, Gate direction swapped
    HX8357_SETPWR1, 6,
      0x00,                      // Not deep standby
      0x15,                      // BT
      0x1C,                      // VSPR
      0x1C,                      // VSNR
      0x83,                      // AP
      0xAA,                      // FS
    HX8357D_SETSTBA, 6,
      0x50,                      // OPON normal
      0x50,                      // OPON idle
      0x01,                      // STBA
      0x3C,                      // STBA
      0x1E,                      // STBA
      0x08,                      // GEN
    HX8357D_SETCYC, 7,
      0x02,                      // NW 0x02
      0x40,                      // RTN
      0x00,                      // DIV
      0x2A,                      // DUM
      0x2A,                      // DUM
      0x0D,                      // GDON
      0x78,                      // GDOFF
    HX8357D_SETGAMMA, 34,
      0x02, 0x0A, 0x11, 0x1d, 0x23, 0x35, 0x41, 0x4b, 0x4b,
      0x42, 0x3A, 0x27, 0x1B, 0x08, 0x09, 0x03, 0x02, 0x0A,
      0x11, 0x1d, 0x23, 0x35, 0x41, 0x4b, 0x4b, 0x42, 0x3A,
      0x27, 0x1B, 0x08, 0x09, 0x03, 0x00, 0x01,
    HX8357_COLMOD, 1,
      0x55,                      // 16 bit
    HX8357_MADCTL, 1,
      0xC0,
    HX8357_TEON, 1,
      0x00,                      // TW off
    HX8357_TEARLINE, 2,
      0x00, 0x02,
    HX8357_SLPOUT, 0x80 + 150/5, // Exit Sleep, then delay 150 ms
    HX8357_DISPON, 0x80 +  50/5, // Main screen turn on, delay 50 ms
    0,                           // END OF COMMAND LIST
  };

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
static uint8_t displayType = HX8357D;

void hx8357_init(void)
{
	//Initialize non-SPI GPIOs
        gpio_pad_select_gpio(HX8357_DC);
	gpio_set_direction(HX8357_DC, GPIO_MODE_OUTPUT);

#if HX8357_USE_RST
        gpio_pad_select_gpio(HX8357_RST);
	gpio_set_direction(HX8357_RST, GPIO_MODE_OUTPUT);
#endif

#if HX8357_ENABLE_BACKLIGHT_CONTROL
        gpio_pad_select_gpio(HX8357_BCKL);
	gpio_set_direction(HX8357_BCKL, GPIO_MODE_OUTPUT);
#endif

#if HX8357_USE_RST
	//Reset the display
	gpio_set_level(HX8357_RST, 0);
	vTaskDelay(10 / portTICK_RATE_MS);
	gpio_set_level(HX8357_RST, 1);
	vTaskDelay(120 / portTICK_RATE_MS);
#endif

	ESP_LOGI(TAG, "Initialization.");

	//Send all the commands
	const uint8_t *addr = (displayType == HX8357B) ? initb : initd;
	uint8_t        cmd, x, numArgs;
	while((cmd = *addr++) > 0) { // '0' command ends list
		x = *addr++;
		numArgs = x & 0x7F;
		if (cmd != 0xFF) { // '255' is ignored
			if (x & 0x80) {  // If high bit set, numArgs is a delay time
				hx8357_send_cmd(cmd);
			} else {
				hx8357_send_cmd(cmd);
				hx8357_send_data((void *) addr, numArgs);
				addr += numArgs;
			}
		}
		if (x & 0x80) {       // If high bit set...
			vTaskDelay(numArgs * 5 / portTICK_RATE_MS); // numArgs is actually a delay time (5ms units)
		}
	}

	hx8357_set_rotation(1);

#if HX8357_INVERT_COLORS
	hx8357_send_cmd(HX8357_INVON);
#else
    hx8357_send_cmd(HX8357_INVOFF);
#endif

	hx8357_enable_backlight(true);
}


void hx8357_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
	uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);

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
	hx8357_send_cmd(HX8357_CASET);
	hx8357_send_data(xb, 4);

	/*Page addresses*/
	hx8357_send_cmd(HX8357_PASET);
	hx8357_send_data(yb, 4);

	/*Memory write*/
	hx8357_send_cmd(HX8357_RAMWR);
	hx8357_send_color((void*)color_map, size * 2);
}

void hx8357_enable_backlight(bool backlight)
{
#if HX8357_ENABLE_BACKLIGHT_CONTROL
    ESP_LOGD(TAG, "%s backlight.\n", backlight ? "Enabling" : "Disabling");
    uint32_t tmp = 0;

#if (HX8357_BCKL_ACTIVE_LVL==1)
    tmp = backlight ? 1 : 0;
#else
    tmp = backlight ? 0 : 1;
#endif

    gpio_set_level(HX8357_BCKL, tmp);
#endif
}


void hx8357_set_rotation(uint8_t r)
{
	r = r & 3; // can't be higher than 3

	switch(r) {
		case 0:
			r = MADCTL_MX | MADCTL_MY | MADCTL_RGB;
			break;
		case 1:
			r = MADCTL_MV | MADCTL_MY | MADCTL_RGB;
      		break;
		case 2:
			r = MADCTL_RGB;
  			break;
		case 3:
			r = MADCTL_MX | MADCTL_MV | MADCTL_RGB;
		break;
	}

	hx8357_send_cmd(HX8357_MADCTL);
	hx8357_send_data(&r, 1);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/


static void hx8357_send_cmd(uint8_t cmd)
{
	disp_wait_for_pending_transactions();
	gpio_set_level(HX8357_DC, 0);	 /*Command mode*/
	disp_spi_send_data(&cmd, 1);
}


static void hx8357_send_data(void * data, uint16_t length)
{
	disp_wait_for_pending_transactions();
	gpio_set_level(HX8357_DC, 1);	 /*Data mode*/
	disp_spi_send_data(data, length);
}


static void hx8357_send_color(void * data, uint16_t length)
{
	disp_wait_for_pending_transactions();
	gpio_set_level(HX8357_DC, 1);   /*Data mode*/
	disp_spi_send_colors(data, length);
}
