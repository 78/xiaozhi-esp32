/**
 * @file ssd1306.c
 *
 * Code from https://github.com/yanbe/ssd1306-esp-idf-i2c.git is used as a starting point,
 * in addition to code from https://github.com/espressif/esp-iot-solution.
 *
 * Definitions are borrowed from:
 * http://robotcantalk.blogspot.com/2015/03/interfacing-arduino-with-ssd1306-driven.html
 *
 * For LVGL the forum has been used, in particular: https://blog.littlevgl.com/2019-05-06/oled
 */

/*********************
 *      INCLUDES
 *********************/
#include "assert.h"

#include "lvgl_i2c/i2c_manager.h"

#include "ssd1306.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "SSD1306"

#define OLED_I2C_PORT                       (CONFIG_LV_I2C_DISPLAY_PORT)
// SLA (0x3C) + WRITE_MODE (0x00) =  0x78 (0b01111000)
#define OLED_I2C_ADDRESS                    0x3C
#define OLED_WIDTH                          128
#define OLED_HEIGHT                         64
#define OLED_COLUMNS                        128
#define OLED_PAGES                          8
#define OLED_PIXEL_PER_PAGE                 8

// Control byte
#define OLED_CONTROL_BYTE_CMD_SINGLE        0x80
#define OLED_CONTROL_BYTE_CMD_STREAM        0x00
#define OLED_CONTROL_BYTE_DATA_STREAM       0x40

// Fundamental commands (pg.28)
#define OLED_CMD_SET_CONTRAST               0x81    // follow with 0x7F
#define OLED_CMD_DISPLAY_RAM                0xA4
#define OLED_CMD_DISPLAY_ALLON              0xA5
#define OLED_CMD_DISPLAY_NORMAL             0xA6
#define OLED_CMD_DISPLAY_INVERTED           0xA7
#define OLED_CMD_DISPLAY_OFF                0xAE
#define OLED_CMD_DISPLAY_ON                 0xAF

// Addressing Command Table (pg.30)
#define OLED_CMD_SET_MEMORY_ADDR_MODE       0x20    // follow with 0x00 = HORZ mode
#define OLED_CMD_SET_COLUMN_RANGE           0x21    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F = COL127
#define OLED_CMD_SET_PAGE_RANGE             0x22    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7

// Hardware Config (pg.31)
#define OLED_CMD_SET_DISPLAY_START_LINE     0x40
#define OLED_CMD_SET_SEGMENT_REMAP          0xA1
#define OLED_CMD_SET_MUX_RATIO              0xA8    // follow with 0x3F = 64 MUX
#define OLED_CMD_SET_COM_SCAN_MODE_NORMAL   0xC0
#define OLED_CMD_SET_COM_SCAN_MODE_REMAP    0xC8
#define OLED_CMD_SET_DISPLAY_OFFSET         0xD3    // follow with 0x00
#define OLED_CMD_SET_COM_PIN_MAP            0xDA    // follow with 0x12
#define OLED_CMD_NOP                        0xE3    // NOP

// Timing and Driving Scheme (pg.32)
#define OLED_CMD_SET_DISPLAY_CLK_DIV        0xD5    // follow with 0x80
#define OLED_CMD_SET_PRECHARGE              0xD9    // follow with 0xF1
#define OLED_CMD_SET_VCOMH_DESELCT          0xDB    // follow with 0x30

// Charge Pump (pg.62)
#define OLED_CMD_SET_CHARGE_PUMP            0x8D    // follow with 0x14

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static uint8_t send_data(lv_disp_drv_t *disp_drv, void *bytes, size_t bytes_len);
static uint8_t send_pixels(lv_disp_drv_t *disp_drv, void *color_buffer, size_t buffer_len);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

#define BIT_SET(a,b) ((a) |= (1U<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1U<<(b)))

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void ssd1306_init(void)
{
    uint8_t orientation_1 = 0;
    uint8_t orientation_2 = 0;

#if defined (CONFIG_DISPLAY_ORIENTATION_LANDSCAPE)
    orientation_1 = OLED_CMD_SET_SEGMENT_REMAP;
    orientation_2 = OLED_CMD_SET_COM_SCAN_MODE_REMAP;
#elif defined (CONFIG_DISPLAY_ORIENTATION_LANDSCAPE_INVERTED)
    orientation_1 = 0xA0;
    orientation_2 = OLED_CMD_SET_COM_SCAN_MODE_NORMAL;
#else
    #error "Unsupported orientation"
#endif

    uint8_t display_mode = 0;

#if defined CONFIG_LV_INVERT_COLORS
    display_mode = OLED_CMD_DISPLAY_INVERTED;
#else
    display_mode = OLED_CMD_DISPLAY_NORMAL;
#endif

    uint8_t conf[] = {
        OLED_CONTROL_BYTE_CMD_STREAM,
        OLED_CMD_SET_CHARGE_PUMP,
        0x14,
        orientation_1,
        orientation_2,
        OLED_CMD_SET_CONTRAST,
        0xFF,
        display_mode,
        OLED_CMD_DISPLAY_ON
    };

    uint8_t err = send_data(NULL, conf, sizeof(conf));
    assert(0 == err);
}

void ssd1306_set_px_cb(lv_disp_drv_t * disp_drv, uint8_t * buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
        lv_color_t color, lv_opa_t opa)
{
    uint16_t byte_index = x + (( y>>3 ) * buf_w);
    uint8_t  bit_index  = y & 0x7;

    if ((color.full == 0) && (LV_OPA_TRANSP != opa)) {
        BIT_SET(buf[byte_index], bit_index);
    } else {
        BIT_CLEAR(buf[byte_index], bit_index);
    }
}

void ssd1306_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    /* Divide by 8 */
    uint8_t row1 = area->y1 >> 3;
    uint8_t row2 = area->y2 >> 3;

    uint8_t conf[] = {
        OLED_CONTROL_BYTE_CMD_STREAM,
        OLED_CMD_SET_MEMORY_ADDR_MODE,
        0x00,
        OLED_CMD_SET_COLUMN_RANGE,
        (uint8_t) area->x1,
        (uint8_t) area->x2,
        OLED_CMD_SET_PAGE_RANGE,
        row1,
        row2,
    };

    uint8_t err = send_data(disp_drv, conf, sizeof(conf));
    assert(0 == err);
    err = send_pixels(disp_drv, color_p, OLED_COLUMNS * (1 + row2 - row1));
    assert(0 == err);

    lv_disp_flush_ready(disp_drv);
}

void ssd1306_rounder(lv_disp_drv_t * disp_drv, lv_area_t *area)
{
    uint8_t hor_max = disp_drv->hor_res;
    uint8_t ver_max = disp_drv->ver_res;

    area->x1 = 0;
    area->y1 = 0;
    area->x2 = hor_max - 1;
    area->y2 = ver_max - 1;
}

void ssd1306_sleep_in(void)
{
    uint8_t conf[] = {
        OLED_CONTROL_BYTE_CMD_STREAM,
        OLED_CMD_DISPLAY_OFF
    };

    uint8_t err = send_data(NULL, conf, sizeof(conf));
    assert(0 == err);
}

void ssd1306_sleep_out(void)
{
    uint8_t conf[] = {
        OLED_CONTROL_BYTE_CMD_STREAM,
        OLED_CMD_DISPLAY_ON
    };

    uint8_t err = send_data(NULL, conf, sizeof(conf));
    assert(0 == err);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static uint8_t send_data(lv_disp_drv_t *disp_drv, void *bytes, size_t bytes_len)
{
    (void) disp_drv;

    uint8_t *data = (uint8_t *) bytes;

    return lvgl_i2c_write(OLED_I2C_PORT, OLED_I2C_ADDRESS, data[0], data + 1, bytes_len - 1 );
}

static uint8_t send_pixels(lv_disp_drv_t *disp_drv, void *color_buffer, size_t buffer_len)
{
    (void) disp_drv;

    return lvgl_i2c_write(OLED_I2C_PORT, OLED_I2C_ADDRESS, OLED_CONTROL_BYTE_DATA_STREAM, color_buffer, buffer_len);
}
