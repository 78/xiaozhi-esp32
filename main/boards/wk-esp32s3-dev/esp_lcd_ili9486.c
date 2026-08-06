/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

#include "esp_lcd_ili9486.h"

#define ESP_LCD_ILI9486_VER_MAJOR 1
#define ESP_LCD_ILI9486_VER_MINOR 2
#define ESP_LCD_ILI9486_VER_PATCH 1

static const char *TAG = "ili9486";

static esp_err_t panel_ili9486_del(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9486_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9486_init(esp_lcd_panel_t *panel);
static esp_err_t panel_ili9486_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_ili9486_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_ili9486_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_ili9486_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_ili9486_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_ili9486_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save current value of LCD_CMD_COLMOD register
    const ili9486_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} ili9486_panel_t;

esp_err_t esp_lcd_new_panel_ili9486(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    ili9486_panel_t *ili9486 = NULL;
    gpio_config_t io_conf = { 0 };

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ili9486 = (ili9486_panel_t *)calloc(1, sizeof(ili9486_panel_t));
    ESP_GOTO_ON_FALSE(ili9486, ESP_ERR_NO_MEM, err, TAG, "no mem for ili9486 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num;
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    switch (panel_dev_config->color_space) {
    case ESP_LCD_COLOR_SPACE_RGB:
        ili9486->madctl_val = 0;
        break;
    case ESP_LCD_COLOR_SPACE_BGR:
        ili9486->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }
#else
    switch (panel_dev_config->rgb_endian) {
    case LCD_RGB_ENDIAN_RGB:
        ili9486->madctl_val = 0;
        break;
    case LCD_RGB_ENDIAN_BGR:
        ili9486->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb endian");
        break;
    }
#endif

    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        ili9486->colmod_val = 0x55;
        ili9486->fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        ili9486->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        ili9486->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    ili9486->io = io;
    ili9486->reset_gpio_num = panel_dev_config->reset_gpio_num;
    ili9486->reset_level = panel_dev_config->flags.reset_active_high;
    if (panel_dev_config->vendor_config) {
        ili9486->init_cmds = ((ili9486_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds;
        ili9486->init_cmds_size = ((ili9486_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds_size;
    }
    ili9486->base.del = panel_ili9486_del;
    ili9486->base.reset = panel_ili9486_reset;
    ili9486->base.init = panel_ili9486_init;
    ili9486->base.draw_bitmap = panel_ili9486_draw_bitmap;
    ili9486->base.invert_color = panel_ili9486_invert_color;
    ili9486->base.set_gap = panel_ili9486_set_gap;
    ili9486->base.mirror = panel_ili9486_mirror;
    ili9486->base.swap_xy = panel_ili9486_swap_xy;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    ili9486->base.disp_off = panel_ili9486_disp_on_off;
#else
    ili9486->base.disp_on_off = panel_ili9486_disp_on_off;
#endif
    *ret_panel = &(ili9486->base);
    ESP_LOGD(TAG, "new ili9486 panel @%p", ili9486);

    ESP_LOGI(TAG, "LCD panel create success, version: %d.%d.%d", ESP_LCD_ILI9486_VER_MAJOR, ESP_LCD_ILI9486_VER_MINOR,
             ESP_LCD_ILI9486_VER_PATCH);

    return ESP_OK;

err:
    if (ili9486) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(ili9486);
    }
    return ret;
}

static esp_err_t panel_ili9486_del(esp_lcd_panel_t *panel)
{
    ili9486_panel_t *ili9486 = __containerof(panel, ili9486_panel_t, base);

    if (ili9486->reset_gpio_num >= 0) {
        gpio_reset_pin(ili9486->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del ili9486 panel @%p", ili9486);
    free(ili9486);
    return ESP_OK;
}

static esp_err_t panel_ili9486_reset(esp_lcd_panel_t *panel)
{
    ili9486_panel_t *ili9486 = __containerof(panel, ili9486_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9486->io;

    // perform hardware reset
    if (ili9486->reset_gpio_num >= 0) {
        gpio_set_level(ili9486->reset_gpio_num, ili9486->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ili9486->reset_gpio_num, !ili9486->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else { // perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5ms before sending new command
    }

    return ESP_OK;
}

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t data_bytes; // Length of data in above data array; 0xFF = end of cmds.
} lcd_init_cmd_t;

static const ili9486_lcd_init_cmd_t vendor_specific_init_default[] = {
//  {cmd, { data }, data_size, delay_ms}

    {0xF9, (uint8_t []){0x00, 0x08}, 2, 0},

    {0xC0, (uint8_t []){0x19, 0x1A}, 2, 0},

    {0xC1, (uint8_t []){0x45, 0x00}, 2, 0},

    // Normal mode, increase can change the display quality, while increasing power consumption
    {0xC2, (uint8_t []){0x33}, 1, 0},   //  Power/Reset on default

    {0xC5, (uint8_t []){0x00, 0x28}, 2, 0}, //VCM_REG[7:0]. <=0X80. //  VCOM control
 
    {0xB1, (uint8_t []){0xA0, 0x11}, 2, 0}, // Sets the frame frequency of full color normal mode, 0XB0 =70HZ, <=0XB0.0xA0=62HZ
 
    {0xB4, (uint8_t []){0x02}, 1, 0}, // 2 DOT FRAME MODE,F<=70HZ.

    {0xB6, (uint8_t []){0x00, 0x42, 0x3B}, 3, 0},

    {0xB7, (uint8_t []){0x07}, 1, 0},

    {0xE0, (uint8_t []){0x1F, 0x25, 0x22, 0x0B, 0x06, 0x0A, 0x4e, 0xC6, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 15, 0},

    {0xE1, (uint8_t []){0x1F, 0x3F, 0x3F, 0x0F, 0x1F, 0x0F, 0x46, 0x49, 0x31, 0x05, 0x09, 0x03, 0x1C, 0x1A, 0x00}, 15, 0},

    {0xF1, (uint8_t []){0x36, 0x04, 0x00, 0x3C, 0x0F, 0x0F, 0xA4, 0x02}, 8, 0},
    
    {0xF2, (uint8_t []){0x18, 0xA3, 0x12, 0x02, 0x32, 0x12, 0xFF, 0x32, 0x00}, 9, 0},

    {0xF4, (uint8_t []){0x40, 0x00, 0x08, 0x91, 0x04}, 5, 0},

    {0xF8, (uint8_t []){0x21, 0x04}, 2, 0},

    {0xF7, (uint8_t []){0x20}, 1, 0},

    /* Set Interface Pixel Format */
    {0x3A, (uint8_t []){0x55}, 1, 0},

    // {0xB6, (uint8_t []){0x00, 0x22}, 2, 0},

     {0x36, (uint8_t []){0x48}, 1, 0}, 

     {0x11, NULL, 0, 120},// Sleep out

     {0x29, NULL, 0, 200},  // Turn on display

};

static esp_err_t panel_ili9486_init(esp_lcd_panel_t *panel)
{
    ili9486_panel_t *ili9486 = __containerof(panel, ili9486_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9486->io;

    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "send command failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        ili9486->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
        ili9486->colmod_val,
    }, 1), TAG, "send command failed");

    const ili9486_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    if (ili9486->init_cmds) {
        init_cmds = ili9486->init_cmds;
        init_cmds_size = ili9486->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(ili9486_lcd_init_cmd_t);
    }

    bool is_cmd_overwritten = false;
    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd) {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            ili9486->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            ili9486->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        default:
            is_cmd_overwritten = false;
            break;
        }

        if (is_cmd_overwritten) {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        }

        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_ili9486_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    ili9486_panel_t *ili9486 = __containerof(panel, ili9486_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = ili9486->io;

    x_start += ili9486->x_gap;
    x_end += ili9486->x_gap;
    y_start += ili9486->y_gap;
    y_end += ili9486->y_gap;

    // define an area of frame memory where MCU can access
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * ili9486->fb_bits_per_pixel / 8;
    esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len);

    return ESP_OK;
}

static esp_err_t panel_ili9486_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    ili9486_panel_t *ili9486 = __containerof(panel, ili9486_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9486->io;
    int command = 0;
    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_ili9486_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    ili9486_panel_t *ili9486 = __containerof(panel, ili9486_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9486->io;
    if (mirror_x) {
        ili9486->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        ili9486->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        ili9486->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        ili9486->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        ili9486->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_ili9486_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    ili9486_panel_t *ili9486 = __containerof(panel, ili9486_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9486->io;
    if (swap_axes) {
        ili9486->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        ili9486->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        ili9486->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_ili9486_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    ili9486_panel_t *ili9486 = __containerof(panel, ili9486_panel_t, base);
    ili9486->x_gap = x_gap;
    ili9486->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_ili9486_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    ili9486_panel_t *ili9486 = __containerof(panel, ili9486_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9486->io;
    int command = 0;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    on_off = !on_off;
#endif

    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
