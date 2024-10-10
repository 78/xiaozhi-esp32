/**
@file il3820.c
@brief   Waveshare e-paper 2.9in b/w display
@version 1.0
@date    2020-05-29
@author  Juergen Kienhoefer


@section LICENSE

MIT License

Copyright (c) 2020 Juergen Kienhoefer

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

/*********************
 *      INCLUDES
 *********************/
#include "disp_spi.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "il3820.h"

/*********************
 *      DEFINES
 *********************/
 #define TAG "IL3820"

/**
 * SSD1673, SSD1608 compatible EPD controller driver.
 */

#define BIT_SET(a,b)                    ((a) |= (1U<<(b)))
#define BIT_CLEAR(a,b)                  ((a) &= ~(1U<<(b)))

/* Number of pixels? */
#define IL3820_PIXEL                    (LV_HOR_RES_MAX * LV_VER_RES_MAX)

#define EPD_PANEL_NUMOF_COLUMS		EPD_PANEL_WIDTH
#define EPD_PANEL_NUMOF_ROWS_PER_PAGE	8

/* Are pages the number of bytes to represent the panel width? in bytes */
#define EPD_PANEL_NUMOF_PAGES	        (EPD_PANEL_HEIGHT / EPD_PANEL_NUMOF_ROWS_PER_PAGE)

#define IL3820_PANEL_FIRST_PAGE	        0
#define IL3820_PANEL_LAST_PAGE	        (EPD_PANEL_NUMOF_PAGES - 1)
#define IL3820_PANEL_FIRST_GATE	        0
#define IL3820_PANEL_LAST_GATE	        (EPD_PANEL_NUMOF_COLUMS - 1)

#define IL3820_PIXELS_PER_BYTE		8

uint8_t il3820_scan_mode = IL3820_DATA_ENTRY_XIYIY;

static uint8_t il3820_lut_initial[] = {
    0x50, 0xAA, 0x55, 0xAA, 0x11, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x1F, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static uint8_t il3820_lut_default[] = {
    0x10, 0x18, 0x18, 0x08, 0x18, 0x18,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x13, 0x14, 0x44, 0x12,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t il3820_softstart[] = {0xd7, 0xd6, 0x9d};
static uint8_t il3820_vcom[] = {0xa8};
/* 4 dummy lines per gate */
static uint8_t il3820_dummyline[] = {0x1a};
/* 2us per line  */
static uint8_t il3820_gatetime[] = {0x08};
static uint8_t il3820_border[] = {0x03};

static bool il3820_partial = false;

/* Static functions */
static void il3820_clear_cntlr_mem(uint8_t ram_cmd, bool update);
static void il3820_waitbusy(int wait_ms);
static inline void il3820_command_mode(void);
static inline void il3820_data_mode(void);
static inline void il3820_write_cmd(uint8_t cmd, uint8_t *data, size_t len);
static inline void il3820_send_cmd(uint8_t cmd);
static void il3820_send_data(uint8_t *data, uint16_t length);
static inline void il3820_set_window( uint16_t sx, uint16_t ex, uint16_t ys, uint16_t ye);
static inline void il3820_set_cursor(uint16_t sx, uint16_t ys);
static void il3820_update_display(void);
static void il3820_clear_cntlr_mem(uint8_t ram_cmd, bool update);

/* Required by LVGL */
void il3820_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    /* Each byte holds the data of 8 pixels, linelen is the number of bytes
     * we need to cover a line of the display. */
    size_t linelen = EPD_PANEL_WIDTH / 8;

    uint8_t *buffer = (uint8_t*) color_map;
    uint16_t x_addr_counter = 0;
    uint16_t y_addr_counter = 0;

    /* Configure entry mode  */
    il3820_write_cmd(IL3820_CMD_ENTRY_MODE, &il3820_scan_mode, 1);

    /* Configure the window based on the coordinates got from LVGL
     * It looks like this epaper display controller doesn't support partial update,
     * so the window is always the same, the display size. */
    il3820_set_window(0, EPD_PANEL_WIDTH - 1, 0, EPD_PANEL_HEIGHT - 1);

    /* Set the cursor at the beginning of the graphic RAM */
#if defined (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT)
    x_addr_counter = EPD_PANEL_WIDTH - 1;
    y_addr_counter = EPD_PANEL_HEIGHT - 1;
#endif

    il3820_set_cursor(x_addr_counter, y_addr_counter);

    il3820_send_cmd(IL3820_CMD_WRITE_RAM);

    /* Write the pixel data to graphic RAM, linelen bytes at the time. */
    for(size_t row = 0; row <= (EPD_PANEL_HEIGHT - 1); row++){
	il3820_send_data(buffer, linelen);
	buffer += IL3820_COLUMNS;
    }

    il3820_set_window(0, EPD_PANEL_WIDTH - 1, 0, EPD_PANEL_HEIGHT - 1);

    il3820_update_display();

    /* IMPORTANT!!!
     * Inform the graphics library that you are ready with the flushing */
    lv_disp_flush_ready(drv);
}


/* Rotate the display by "software" when using PORTRAIT orientation.
 * BIT_SET(byte_index, bit_index) clears the bit_index pixel at byte_index of
 * the display buffer.
 * BIT_CLEAR(byte_index, bit_index) sets the bit_index pixel at the byte_index
 * of the display buffer. */
void il3820_set_px_cb(lv_disp_drv_t * disp_drv, uint8_t* buf,
    lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
    lv_color_t color, lv_opa_t opa)
{
    uint16_t byte_index = 0;
    uint8_t  bit_index = 0;

#if defined (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT)
    byte_index = x + ((y >> 3) * EPD_PANEL_HEIGHT);
    bit_index  = y & 0x7;

    if (color.full) {
        BIT_SET(buf[byte_index], 7 - bit_index);
    } else {
        uint16_t mirrored_idx = (EPD_PANEL_HEIGHT - x) + ((y >> 3) * EPD_PANEL_HEIGHT);
        BIT_CLEAR(buf[mirrored_idx], 7 - bit_index);
    }
#elif defined (CONFIG_LV_DISPLAY_ORIENTATION_LANDSCAPE)
    byte_index = y + ((x >> 3) * EPD_PANEL_HEIGHT);
    bit_index  = x & 0x7;

    if (color.full) {
        BIT_SET(buf[byte_index], 7 - bit_index);
    } else {
        BIT_CLEAR(buf[byte_index], 7 - bit_index);
    }
#else
#error "Unsupported orientation used"
#endif
}

/* Required by LVGL */
void il3820_rounder(lv_disp_drv_t * disp_drv, lv_area_t *area) {
    area->x1 = area->x1 & ~(0x7);
    area->x2 = area->x2 |  (0x7);
}

/* main initialization routine */
void il3820_init(void)
{
    uint8_t tmp[3] = {0};

    /* Initialize non-SPI GPIOs */
    gpio_pad_select_gpio(IL3820_DC_PIN);
    gpio_set_direction(IL3820_DC_PIN, GPIO_MODE_OUTPUT);

#if IL3820_USE_RST
    gpio_pad_select_gpio(IL3820_RST_PIN);
    gpio_set_direction(IL3820_RST_PIN, GPIO_MODE_OUTPUT);
#endif

    gpio_pad_select_gpio(IL3820_BUSY_PIN);
    gpio_set_direction(IL3820_BUSY_PIN,  GPIO_MODE_INPUT);

#if IL3820_USE_RST
    /* Harware reset */
    gpio_set_level( IL3820_RST_PIN, 0);
    vTaskDelay(IL3820_RESET_DELAY / portTICK_RATE_MS);
    gpio_set_level( IL3820_RST_PIN, 1);
    vTaskDelay(IL3820_RESET_DELAY / portTICK_RATE_MS);
#endif

    /* Software reset */
    il3820_write_cmd(IL3820_CMD_SW_RESET, NULL, 0);

    /* Busy wait for the BUSY signal to go low */
    il3820_waitbusy(IL3820_WAIT);

    /**/
    tmp[0] = ( EPD_PANEL_HEIGHT - 1) & 0xFF;
    tmp[1] = ( EPD_PANEL_HEIGHT >> 8 );
    tmp[2] = 0; // GD = 0; SM = 0; TB = 0;
    il3820_write_cmd(IL3820_CMD_GDO_CTRL, tmp, 3);

    /**/
    il3820_write_cmd(IL3820_CMD_SOFTSTART, il3820_softstart, sizeof(il3820_softstart));

    /* Write VCOM register */
    il3820_write_cmd(IL3820_CMD_VCOM_VOLTAGE, il3820_vcom, 1);
    /* Set dummy line period (in term of TGate) */
    il3820_write_cmd(IL3820_CMD_DUMMY_LINE, il3820_dummyline, 1);
    /* Set gate line width (TGate) in us */
    il3820_write_cmd(IL3820_CMD_GATE_LINE_WIDTH, il3820_gatetime, 1);
    /* Select border waveform for VBD */
    il3820_write_cmd(IL3820_CMD_BWF_CTRL, il3820_border, 1);
    /**/
    il3820_write_cmd(IL3820_CMD_UPDATE_LUT, il3820_lut_initial, sizeof(il3820_lut_initial));
    /* Clear control memory and update */
    il3820_clear_cntlr_mem(IL3820_CMD_WRITE_RAM, true);

    // allow partial updates now
    il3820_partial = true;

    /* Update LUT */
    il3820_write_cmd(IL3820_CMD_UPDATE_LUT, il3820_lut_default, sizeof(il3820_lut_default));

    /* Clear control memory and update */
    il3820_clear_cntlr_mem(IL3820_CMD_WRITE_RAM, true);
}

/* Enter deep sleep mode */
void il3820_sleep_in(void)
{
    uint8_t data[] = {0x01};

    /* Wait for the BUSY signal to go low */
    il3820_waitbusy(IL3820_WAIT);

    il3820_write_cmd(IL3820_CMD_SLEEP_MODE, data, 1);
}

/* TODO: Remove the busy waiting */
static void il3820_waitbusy(int wait_ms)
{
    int i = 0;

    vTaskDelay(10 / portTICK_RATE_MS); // 10ms delay

    for(i = 0; i < (wait_ms * 10); i++) {
	if(gpio_get_level(IL3820_BUSY_PIN) != IL3820_BUSY_LEVEL) {
            return;
        }

        vTaskDelay(10 / portTICK_RATE_MS);
    }

    ESP_LOGE( TAG, "busy exceeded %dms", i*10 );
}

/* Set DC signal to command mode */
static inline void il3820_command_mode(void)
{
    gpio_set_level(IL3820_DC_PIN, 0);
}

/* Set DC signal to data mode */
static inline void il3820_data_mode(void)
{
    gpio_set_level(IL3820_DC_PIN, 1);
}

static inline void il3820_write_cmd(uint8_t cmd, uint8_t *data, size_t len)
{
    disp_wait_for_pending_transactions();

    il3820_command_mode();
    disp_spi_send_data(&cmd, 1);

    if (data != NULL) {
	il3820_data_mode();
	disp_spi_send_data(data, len);
    }
}

/* Send cmd to the display */
static inline void il3820_send_cmd(uint8_t cmd)
{
    disp_wait_for_pending_transactions();

    il3820_command_mode();
    disp_spi_send_data(&cmd, 1);
}

/* Send length bytes of data to the display */
static void il3820_send_data(uint8_t *data, uint16_t length)
{
    disp_wait_for_pending_transactions();

    il3820_data_mode();
    disp_spi_send_colors(data, length);
}

/* Specify the start/end positions of the window address in the X and Y
 * directions by an address unit.
 *
 * @param sx: X Start position.
 * @param ex: X End position.
 * @param ys: Y Start position.
 * @param ye: Y End position.
 */
static inline void il3820_set_window( uint16_t sx, uint16_t ex, uint16_t ys, uint16_t ye)
{
    uint8_t tmp[4] = {0};

    tmp[0] = sx / 8;
    tmp[1] = ex / 8;

    /* Set X address start/end */
    il3820_write_cmd(IL3820_CMD_RAM_XPOS_CTRL, tmp, 2);

    tmp[0] = ys % 256;
    tmp[1] = ys / 256;
    tmp[2] = ye % 256;
    tmp[3] = ye / 256;
    /* Set Y address start/end */
    il3820_write_cmd(IL3820_CMD_RAM_YPOS_CTRL, tmp, 4);
}

/* Make initial settings for the RAM X and Y addresses in the address counter
 * (AC).
 *
 * @param sx: RAM X address counter.
 * @param ys: RAM Y address counter.
 */
static inline void il3820_set_cursor(uint16_t sx, uint16_t ys)
{
    uint8_t tmp[2] = {0};

    tmp[0] = sx / 8;
    il3820_write_cmd(IL3820_CMD_RAM_XPOS_CNTR, tmp, 1);

    tmp[0] = ys % 256;
    tmp[1] = ys / 256;
    il3820_write_cmd(IL3820_CMD_RAM_YPOS_CNTR, tmp, 2);
}

/* After sending the RAM content we need to send the commands:
 * - Display Update Control 2
 * - Master Activation
 *
 * NOTE: Currently we poll for the BUSY signal to go inactive,
 * we might want not to do it. */
static void il3820_update_display(void)
{
    uint8_t tmp = 0;

    if(il3820_partial) {
	tmp = IL3820_CTRL2_TO_PATTERN;
    } else {
	tmp =  (IL3820_CTRL2_ENABLE_CLK | IL3820_CTRL2_ENABLE_ANALOG | IL3820_CTRL2_TO_PATTERN);
    }

    il3820_write_cmd(IL3820_CMD_UPDATE_CTRL2, &tmp, 1);

    il3820_write_cmd(IL3820_CMD_MASTER_ACTIVATION, NULL, 0);
    /* Poll BUSY signal. */
    il3820_waitbusy(IL3820_WAIT);
    /* XXX: Figure out what does this command do. */
    il3820_write_cmd(IL3820_CMD_TERMINATE_FRAME_RW, NULL, 0);
}

/* Clear the graphic RAM. */
static void il3820_clear_cntlr_mem(uint8_t ram_cmd, bool update)
{
    /* Arrays used by SPI must be word alligned */
    WORD_ALIGNED_ATTR uint8_t clear_page[IL3820_COLUMNS];
    memset(clear_page, 0xff, sizeof clear_page);

    /* Configure entry mode */
    il3820_write_cmd(IL3820_CMD_ENTRY_MODE, &il3820_scan_mode, 1);

    /* Configure the window */
    il3820_set_window(0, EPD_PANEL_WIDTH - 1, 0, EPD_PANEL_HEIGHT - 1);

    /* Send clear_page buffer to the display */
    for(int j = 0; j < EPD_PANEL_HEIGHT; j++) {
	il3820_set_cursor(0, j);
	il3820_write_cmd(ram_cmd, clear_page, sizeof clear_page);
    }

    if (update) {
	il3820_set_window( 0, EPD_PANEL_WIDTH - 1, 0, EPD_PANEL_HEIGHT - 1);
	il3820_update_display();
    }
}
