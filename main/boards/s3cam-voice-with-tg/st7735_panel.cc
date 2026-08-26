/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Minimal esp_lcd panel driver for the Sitronix ST7735S TFT controller
 * (SPI, 128x160 RGB565), modeled on esp_lcd_panel_st7789.c from ESP-IDF.
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "sdkconfig.h"

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
#include "esp_compiler.h"

static const char *TAG = "lcd_panel.st7735";

static esp_err_t panel_st7735_del(esp_lcd_panel_t *panel);
static esp_err_t panel_st7735_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_st7735_init(esp_lcd_panel_t *panel);
static esp_err_t panel_st7735_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                          const void *color_data);
static esp_err_t panel_st7735_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_st7735_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_st7735_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_st7735_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_st7735_disp_on_off(esp_lcd_panel_t *panel, bool on_off);
static esp_err_t panel_st7735_sleep(esp_lcd_panel_t *panel, bool sleep);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    gpio_num_t reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;    // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val;    // save current value of LCD_CMD_COLMOD register
} st7735_panel_t;

static esp_err_t st7735_tx_cmd(st7735_panel_t *st7735, uint8_t cmd)
{
    return esp_lcd_panel_io_tx_param(st7735->io, cmd, NULL, 0);
}

static esp_err_t st7735_tx_cmd_data(st7735_panel_t *st7735, uint8_t cmd, const uint8_t *data, size_t data_size)
{
    return esp_lcd_panel_io_tx_param(st7735->io, cmd, data, data_size);
}

extern "C" esp_err_t
esp_lcd_new_panel_st7735(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                         esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    st7735_panel_t *st7735 = (st7735_panel_t *)calloc(1, sizeof(st7735_panel_t));
    ESP_RETURN_ON_FALSE(st7735, ESP_ERR_NO_MEM, TAG, "no mem for st7735 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {};
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num;
        esp_err_t ret = gpio_config(&io_conf);
        if (ret != ESP_OK) {
            free(st7735);
            ESP_LOGE(TAG, "configure GPIO for RST line failed");
            return ret;
        }
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        st7735->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        st7735->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        free(st7735);
        ESP_LOGE(TAG, "unsupported RGB element order");
        return ESP_ERR_NOT_SUPPORTED;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        st7735->colmod_val = 0x05;
        st7735->fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        st7735->colmod_val = 0x06;
        st7735->fb_bits_per_pixel = 24;
        break;
    default:
        free(st7735);
        ESP_LOGE(TAG, "unsupported pixel width");
        return ESP_ERR_NOT_SUPPORTED;
    }

    st7735->io = io;
    st7735->reset_gpio_num = (gpio_num_t)panel_dev_config->reset_gpio_num;
    st7735->reset_level = panel_dev_config->flags.reset_active_high;
    st7735->base.del = panel_st7735_del;
    st7735->base.reset = panel_st7735_reset;
    st7735->base.init = panel_st7735_init;
    st7735->base.draw_bitmap = panel_st7735_draw_bitmap;
    st7735->base.invert_color = panel_st7735_invert_color;
    st7735->base.set_gap = panel_st7735_set_gap;
    st7735->base.set_brightness = NULL;
    st7735->base.mirror = panel_st7735_mirror;
    st7735->base.swap_xy = panel_st7735_swap_xy;
    st7735->base.disp_on_off = panel_st7735_disp_on_off;
    st7735->base.disp_sleep = panel_st7735_sleep;
    *ret_panel = &(st7735->base);
    ESP_LOGD(TAG, "new st7735 panel @%p", st7735);

    return ESP_OK;
}

static esp_err_t panel_st7735_del(esp_lcd_panel_t *panel)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);

    if (st7735->reset_gpio_num >= 0) {
        gpio_reset_pin(st7735->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del st7735 panel @%p", st7735);
    free(st7735);
    return ESP_OK;
}

static esp_err_t panel_st7735_reset(esp_lcd_panel_t *panel)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);

    // perform hardware reset
    if (st7735->reset_gpio_num >= 0) {
        gpio_set_level(st7735->reset_gpio_num, st7735->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(st7735->reset_gpio_num, !st7735->reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else { // perform software reset
        ESP_RETURN_ON_ERROR(st7735_tx_cmd(st7735, LCD_CMD_SWRESET), TAG, "io tx param failed");
        vTaskDelay(pdMS_TO_TICKS(150)); // spec, wait at least 120ms before sending new command
    }

    return ESP_OK;
}

static esp_err_t panel_st7735_init(esp_lcd_panel_t *panel)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);

    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    ESP_RETURN_ON_ERROR(st7735_tx_cmd(st7735, LCD_CMD_SLPOUT), TAG, "io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(120));

    // Frame Rate Control
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xB1, (uint8_t[]) {0x01, 0x2C, 0x2D}, 3), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xB2, (uint8_t[]) {0x01, 0x2C, 0x2D}, 3), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xB3, (uint8_t[]) {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 6), TAG, "io tx param failed");
    // Display Inversion Control
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xB4, (uint8_t[]) {0x07}, 1), TAG, "io tx param failed");
    // Power Control
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xC0, (uint8_t[]) {0xA2, 0x02, 0x84}, 3), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xC1, (uint8_t[]) {0xC5}, 1), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xC2, (uint8_t[]) {0x0A, 0x00}, 2), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xC3, (uint8_t[]) {0x8A, 0x2A}, 2), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xC4, (uint8_t[]) {0x8A, 0xEE}, 2), TAG, "io tx param failed");
    // VCOM Control 1
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xC5, (uint8_t[]) {0x0E}, 1), TAG, "io tx param failed");
    // Gamma Correction
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xE0,
        (uint8_t[]) {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10}, 16),
        TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, 0xE1,
        (uint8_t[]) {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10}, 16),
        TAG, "io tx param failed");
    // Pixel Format (16-bit RGB565)
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, LCD_CMD_COLMOD, (uint8_t[]) {st7735->colmod_val}, 1), TAG, "io tx param failed");
    // Memory Data Access Control
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, LCD_CMD_MADCTL, (uint8_t[]) {st7735->madctl_val}, 1), TAG, "io tx param failed");
    // Enter normal mode
    ESP_RETURN_ON_ERROR(st7735_tx_cmd(st7735, LCD_CMD_NORON), TAG, "io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(10));

    return ESP_OK;
}

static esp_err_t panel_st7735_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                          const void *color_data)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);
    esp_lcd_panel_io_handle_t io = st7735->io;

    x_start += st7735->x_gap;
    x_end += st7735->x_gap;
    y_start += st7735->y_gap;
    y_end += st7735->y_gap;

    // define an area of frame memory where MCU can access
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
        (uint8_t)((x_start >> 8) & 0xFF),
        (uint8_t)(x_start & 0xFF),
        (uint8_t)(((x_end - 1) >> 8) & 0xFF),
        (uint8_t)((x_end - 1) & 0xFF),
    }, 4), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
        (uint8_t)((y_start >> 8) & 0xFF),
        (uint8_t)(y_start & 0xFF),
        (uint8_t)(((y_end - 1) >> 8) & 0xFF),
        (uint8_t)((y_end - 1) & 0xFF),
    }, 4), TAG, "io tx param failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * st7735->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), TAG, "io tx color failed");

    return ESP_OK;
}

static esp_err_t panel_st7735_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);
    int command = 0;
    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(st7735_tx_cmd(st7735, command), TAG, "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_st7735_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);
    if (mirror_x) {
        st7735->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        st7735->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        st7735->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        st7735->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, LCD_CMD_MADCTL, (uint8_t[]) {
        st7735->madctl_val
    }, 1), TAG, "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_st7735_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);
    if (swap_axes) {
        st7735->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        st7735->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(st7735_tx_cmd_data(st7735, LCD_CMD_MADCTL, (uint8_t[]) {
        st7735->madctl_val
    }, 1), TAG, "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_st7735_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);
    st7735->x_gap = x_gap;
    st7735->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_st7735_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);
    int command = 0;
    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(st7735_tx_cmd(st7735, command), TAG, "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_st7735_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    st7735_panel_t *st7735 = __containerof(panel, st7735_panel_t, base);
    int command = 0;
    if (sleep) {
        command = LCD_CMD_SLPIN;
    } else {
        command = LCD_CMD_SLPOUT;
    }
    ESP_RETURN_ON_ERROR(st7735_tx_cmd(st7735, command), TAG, "io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}