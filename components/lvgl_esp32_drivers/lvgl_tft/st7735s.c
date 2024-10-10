/**
 * @file st7735s.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "st7735s.h"
#include "disp_spi.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef CONFIG_LV_M5STICKC_HANDLE_AXP192
    #include "lvgl_i2c/i2c_manager.h"
#endif

/*********************
 *      DEFINES
 *********************/
 #define TAG "ST7735S"
 #define AXP192_I2C_ADDRESS                    0x34

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
static void st7735s_send_cmd(uint8_t cmd);
static void st7735s_send_data(void * data, uint16_t length);
static void st7735s_send_color(void * data, uint16_t length);
static void st7735s_set_orientation(uint8_t orientation);
static void axp192_write_byte(uint8_t addr, uint8_t data);
static void axp192_init();
static void axp192_sleep_in();
static void axp192_sleep_out();

/**********************
 *  STATIC VARIABLES
 **********************/
uint8_t st7735s_portrait_mode = 0;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void st7735s_init(void)
{
#ifdef CONFIG_LV_M5STICKC_HANDLE_AXP192
    axp192_init();
#endif

    lcd_init_cmd_t init_cmds[]={
		{ST7735_SWRESET, {0}, 0x80},         		// Software reset, 0 args, w/delay 150
		{ST7735_SLPOUT, {0}, 0x80},                 // Out of sleep mode, 0 args, w/delay 500
		{ST7735_FRMCTR1, {0x01, 0x2C, 0x2D}, 3},    // Frame rate ctrl - normal mode, 3 args: Rate = fosc/(1x2+40) * (LINE+2C+2D)
		{ST7735_FRMCTR2, {0x01, 0x2C, 0x2D}, 3},    // Frame rate control - idle mode, 3 args:Rate = fosc/(1x2+40) * (LINE+2C+2D)
		{ST7735_FRMCTR3, {0x01, 0x2C, 0x2D,0x01, 0x2C, 0x2D}, 6}, //Frame rate ctrl - partial mode, 6 args:Dot inversion mode. Line inversion mode
		{ST7735_INVCTR, {0x07}, 1},                 // Display inversion ctrl, 1 arg, no delay:No inversion
		{ST7735_PWCTR1, {0xA2,0x02, 0x84}, 3},      // Power control, 3 args, no delay:-4.6V AUTO mode
		{ST7735_PWCTR2, {0xC5}, 1},                 // Power control, 1 arg, no delay:VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
		{ST7735_PWCTR3, {0x0A, 0x00}, 2},           // Power control, 2 args, no delay: Opamp current small, Boost frequency
		{ST7735_PWCTR4, {0x8A,0x2A }, 2},           // Power control, 2 args, no delay: BCLK/2, Opamp current small & Medium low
		{ST7735_PWCTR5, {0x8A, 0xEE}, 2},           // Power control, 2 args, no delay:
		{ST7735_VMCTR1, {0x0E}, 1},                 // Power control, 1 arg, no delay:
#if ST7735S_INVERT_COLORS == 1
		{ST7735_INVON, {0}, 0},                     // set inverted mode
#else
 		{ST7735_INVOFF, {0}, 0},                    // set non-inverted mode
#endif
		{ST7735_COLMOD, {0x05}, 1},               	// set color mode, 1 arg, no delay: 16-bit color
		{ST7735_GMCTRP1, {0x02, 0x1c, 0x07, 0x12,
			0x37, 0x32, 0x29, 0x2d,
			0x29, 0x25, 0x2B, 0x39,
			0x00, 0x01, 0x03, 0x10}, 16},           // 16 args, no delay:
		{ST7735_GMCTRN1, {0x03, 0x1d, 0x07, 0x06,
			0x2E, 0x2C, 0x29, 0x2D,
			0x2E, 0x2E, 0x37, 0x3F,
			0x00, 0x00, 0x02, 0x10}, 16},           // 16 args, no delay:
		{ST7735_NORON, {0}, TFT_INIT_DELAY},       	// Normal display on, no args, w/delay 10 ms delay
		{ST7735_DISPON, {0}, TFT_INIT_DELAY},       // Main screen turn on, no args w/delay 100 ms delay
		{0, {0}, 0xff}
    };

	//Initialize non-SPI GPIOs
        gpio_pad_select_gpio(ST7735S_DC);
	gpio_set_direction(ST7735S_DC, GPIO_MODE_OUTPUT);

#if ST7735S_USE_RST
        gpio_pad_select_gpio(ST7735S_RST);
	gpio_set_direction(ST7735S_RST, GPIO_MODE_OUTPUT);

	//Reset the display
	gpio_set_level(ST7735S_RST, 0);
	vTaskDelay(100 / portTICK_RATE_MS);
	gpio_set_level(ST7735S_RST, 1);
	vTaskDelay(100 / portTICK_RATE_MS);
#endif

	ESP_LOGI(TAG, "ST7735S initialization.");

	//Send all the commands
	uint16_t cmd = 0;
	while (init_cmds[cmd].databytes!=0xff) {
		st7735s_send_cmd(init_cmds[cmd].cmd);
		st7735s_send_data(init_cmds[cmd].data, init_cmds[cmd].databytes&0x1F);
		if (init_cmds[cmd].databytes & 0x80) {
			vTaskDelay(100 / portTICK_RATE_MS);
		}
		cmd++;
	}

#if (CONFIG_LV_DISPLAY_ORIENTATION == 0) || (CONFIG_LV_DISPLAY_ORIENTATION == 1)
	st7735s_portrait_mode = 1;
#else
	st7735s_portrait_mode = 0;
#endif

    st7735s_set_orientation(CONFIG_LV_DISPLAY_ORIENTATION);
}

void st7735s_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
	uint8_t data[4];

	/*Column addresses*/
	st7735s_send_cmd(0x2A);
	data[0] = (area->x1 >> 8) & 0xFF;
	data[1] = (area->x1 & 0xFF) + (st7735s_portrait_mode ? COLSTART : ROWSTART);
	data[2] = (area->x2 >> 8) & 0xFF;
	data[3] = (area->x2 & 0xFF) + (st7735s_portrait_mode ? COLSTART : ROWSTART);
	st7735s_send_data(data, 4);

	/*Page addresses*/
	st7735s_send_cmd(0x2B);
	data[0] = (area->y1 >> 8) & 0xFF;
	data[1] = (area->y1 & 0xFF) + (st7735s_portrait_mode ? ROWSTART : COLSTART);
	data[2] = (area->y2 >> 8) & 0xFF;
	data[3] = (area->y2 & 0xFF) + (st7735s_portrait_mode ? ROWSTART : COLSTART);
	st7735s_send_data(data, 4);

	/*Memory write*/
	st7735s_send_cmd(0x2C);

	uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
	st7735s_send_color((void*)color_map, size * 2);
}

void st7735s_sleep_in()
{
	st7735s_send_cmd(0x10);
    #ifdef CONFIG_LV_M5STICKC_HANDLE_AXP192
    	axp192_sleep_in();
    #endif
}

void st7735s_sleep_out()
{
    #ifdef CONFIG_LV_M5STICKC_HANDLE_AXP192
    	axp192_sleep_out();
    #endif
	st7735s_send_cmd(0x11);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void st7735s_send_cmd(uint8_t cmd)
{
	disp_wait_for_pending_transactions();
	gpio_set_level(ST7735S_DC, 0);	 /*Command mode*/
	disp_spi_send_data(&cmd, 1);
}

static void st7735s_send_data(void * data, uint16_t length)
{
	disp_wait_for_pending_transactions();
	gpio_set_level(ST7735S_DC, 1);	 /*Data mode*/
	disp_spi_send_data(data, length);
}

static void st7735s_send_color(void * data, uint16_t length)
{
	disp_wait_for_pending_transactions();
	gpio_set_level(ST7735S_DC, 1);   /*Data mode*/
	disp_spi_send_colors(data, length);
}

static void st7735s_set_orientation(uint8_t orientation)
{
    const char *orientation_str[] = {
        "PORTRAIT", "PORTRAIT_INVERTED", "LANDSCAPE", "LANDSCAPE_INVERTED"
    };

    ESP_LOGD(TAG, "Display orientation: %s", orientation_str[orientation]);

    /*
        Portrait:  0xC8 = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_BGR
        Landscape: 0xA8 = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_BGR
        Remark: "inverted" is ignored here
    */
    uint8_t data[] = {0xC8, 0xC8, 0xA8, 0xA8};

    ESP_LOGD(TAG, "0x36 command value: 0x%02X", data[orientation]);

    st7735s_send_cmd(ST7735_MADCTL);
    st7735s_send_data((void *) &data[orientation], 1);
}

#ifdef CONFIG_LV_M5STICKC_HANDLE_AXP192

    static void axp192_write_byte(uint8_t addr, uint8_t data)
    {
        err = lvgl_i2c_write(CONFIG_LV_I2C_DISPLAY_PORT, AXP192_I2C_ADDRESS, addr, &data, 1);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "AXP192 send failed. code: 0x%.2X", ret);
        }
    }

    static void axp192_init()
    {
        // information on how to init and use AXP192 ifor M5StickC taken from
        // 	https://forum.m5stack.com/topic/1025/m5stickc-turn-off-screen-completely

        axp192_write_byte(0x10, 0xFF);			// OLED_VPP Enable
        axp192_write_byte(0x28, 0xCC);			// Enable LDO2&LDO3, LED&TFT 3.0V
        axp192_sleep_out();
        ESP_LOGI(TAG, "AXP192 initialized, power enabled for LDO2 and LDO3");
    }

    static void axp192_sleep_in()
    {
        axp192_write_byte(0x12, 0x4b);
    }

    static void axp192_sleep_out()
    {
        axp192_write_byte(0x12, 0x4d);
    }

#endif
