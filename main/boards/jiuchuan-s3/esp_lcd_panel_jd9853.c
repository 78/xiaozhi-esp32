/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "sdkconfig.h"
#include <string.h>
#if CONFIG_LCD_ENABLE_DEBUG_LOG
// The local log level must be defined before including esp_log.h
// Set the maximum log level for this source file
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

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

// JD9853 Command definitions
#define JD9853_CMD_SLPIN         0x10
#define JD9853_CMD_SLPOUT        0x11
#define JD9853_CMD_INVOFF        0x20
#define JD9853_CMD_INVON         0x21
#define JD9853_CMD_DISPOFF       0x28
#define JD9853_CMD_DISPON        0x29
#define JD9853_CMD_CASET         0x2A
#define JD9853_CMD_RASET         0x2B
#define JD9853_CMD_RAMWR         0x2C
#define JD9853_CMD_TEON          0x35
#define JD9853_CMD_MADCTL        0x36
#define JD9853_CMD_COLMOD        0x3A

static const char *TAG = "lcd_panel.jd9853";

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t madctl_val;
    uint8_t colmod_val;
    uint8_t fb_bits_per_pixel;
} jd9853_panel_t;

static esp_err_t panel_jd9853_del(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9853_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9853_init(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9853_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_jd9853_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_jd9853_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_jd9853_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_jd9853_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_jd9853_disp_on_off(esp_lcd_panel_t *panel, bool off);
static esp_err_t panel_jd9853_sleep(esp_lcd_panel_t *panel, bool sleep);

esp_err_t esp_lcd_new_panel_jd9853(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    jd9853_panel_t *jd9853 = NULL;

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid arg");
    
    jd9853 = calloc(1, sizeof(jd9853_panel_t));
    ESP_GOTO_ON_FALSE(jd9853, ESP_ERR_NO_MEM, err, TAG, "no mem");

    // Hardware reset GPIO config
    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "GPIO config failed");
    }

    jd9853->colmod_val = 0x05; // RGB565
    jd9853->fb_bits_per_pixel = 16;
    jd9853->io = io;
    jd9853->reset_gpio_num = panel_dev_config->reset_gpio_num;
    jd9853->reset_level = panel_dev_config->flags.reset_active_high;
    jd9853->x_gap = 0;
    jd9853->y_gap = 0;

    // Function pointers
    jd9853->base.del = panel_jd9853_del;
    jd9853->base.reset = panel_jd9853_reset;
    jd9853->base.init = panel_jd9853_init;
    jd9853->base.draw_bitmap = panel_jd9853_draw_bitmap;
    jd9853->base.invert_color = panel_jd9853_invert_color;
    jd9853->base.set_gap = panel_jd9853_set_gap;
    jd9853->base.mirror = panel_jd9853_mirror;
    jd9853->base.swap_xy = panel_jd9853_swap_xy;
    jd9853->base.disp_on_off = panel_jd9853_disp_on_off;
    jd9853->base.disp_sleep = panel_jd9853_sleep;

    *ret_panel = &(jd9853->base);
    ESP_LOGI(TAG, "New JD9853 panel @%p", jd9853);
    return ESP_OK;

err:
    if (jd9853) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(jd9853);
    }
    return ret;
}

static esp_err_t panel_jd9853_del(esp_lcd_panel_t *panel)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);

    if (jd9853->reset_gpio_num >= 0) {
        gpio_reset_pin(jd9853->reset_gpio_num);
    }
    free(jd9853);
    ESP_LOGI(TAG, "Del JD9853 panel");
    return ESP_OK;
}

static esp_err_t panel_jd9853_reset(esp_lcd_panel_t *panel)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);

    if (jd9853->reset_gpio_num >= 0) {
        // Hardware reset
        gpio_set_level(jd9853->reset_gpio_num, jd9853->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(jd9853->reset_gpio_num, !jd9853->reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else {
        // Software reset if no GPIO configured
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    return ESP_OK;
}

static esp_err_t panel_jd9853_init(esp_lcd_panel_t *panel)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9853->io;

    // JD9853 initialization sequence from BOE datasheet
    // Unlock command sequence
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xDF, (uint8_t[]){0x98, 0x53}, 2), TAG, "Unlock failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xDE, (uint8_t[]){0x00}, 1), TAG, "DE failed");

    // Power control settings
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB2, (uint8_t[]){0x25}, 1), TAG, "B2 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB7, (uint8_t[]){0x00, 0x21, 0x00, 0x49}, 4), TAG, "B7 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xBB, (uint8_t[]){0x4F, 0x1A, 0x55, 0x73, 0x6F, 0xF0}, 6), TAG, "BB failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC0, (uint8_t[]){0x44, 0xA4}, 2), TAG, "C0 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC1, (uint8_t[]){0x12}, 1), TAG, "C1 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC3, (uint8_t[]){0x7D, 0x07, 0x14, 0x06, 0xC8, 0x71, 0x6C, 0x77}, 8), TAG, "C3 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC4, (uint8_t[]){0x00, 0x00, 0x94, 0x79, 0x25, 0x0A, 0x16, 0x79, 0x25, 0x0A, 0x16, 0x82}, 12), TAG, "C4 failed");

    // Gamma settings
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC8, (uint8_t[]){
        0x3F, 0x34, 0x2B, 0x20, 0x2A, 0x2C, 0x24, 0x24, 0x21, 0x22, 0x20, 0x15, 0x10, 0x0B, 0x06, 0x00,
        0x3F, 0x34, 0x2B, 0x20, 0x2A, 0x2C, 0x24, 0x24, 0x21, 0x22, 0x20, 0x15, 0x10, 0x0B, 0x06, 0x00
    }, 32), TAG, "Gamma failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xD0, (uint8_t[]){0x04, 0x06, 0x6B, 0x0F, 0x00}, 5), TAG, "D0 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xD7, (uint8_t[]){0x00, 0x30}, 2), TAG, "D7 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE6, (uint8_t[]){0x14}, 1), TAG, "E6 failed");

    // Page 1 settings
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xDE, (uint8_t[]){0x01}, 1), TAG, "DE1 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB7, (uint8_t[]){0x03, 0x13, 0xEF, 0x35, 0x35}, 5), TAG, "B7_P1 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC1, (uint8_t[]){0x14, 0x15, 0xC0}, 3), TAG, "C1_P1 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC2, (uint8_t[]){0x06, 0x3A, 0xC7}, 3), TAG, "C2 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC4, (uint8_t[]){0x72, 0x12}, 2), TAG, "C4_P1 failed");
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xBE, (uint8_t[]){0x00}, 1), TAG, "BE failed");

    // Back to page 0
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xDE, (uint8_t[]){0x00}, 1), TAG, "DE0 failed");

    // TE (Tearing Effect) on
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x35, (uint8_t[]){0x00}, 1), TAG, "TE failed");

    // Pixel format: RGB565
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x3A, (uint8_t[]){0x05}, 1), TAG, "COLMOD failed");

    // Column address set: 0-239 (0x00EF)
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2A, (uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4), TAG, "CASET failed");

    // Row address set: 0-295 (0x0127)
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0x27}, 4), TAG, "RASET failed");

    // Sleep out
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x11, NULL, 0), TAG, "Sleep out failed");
    vTaskDelay(pdMS_TO_TICKS(120));

    // Display on
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x29, NULL, 0), TAG, "Display on failed");
    vTaskDelay(pdMS_TO_TICKS(1));

    return ESP_OK;
}

static esp_err_t panel_jd9853_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9853->io;

    x_start += jd9853->x_gap;
    x_end += jd9853->x_gap;
    y_start += jd9853->y_gap;
    y_end += jd9853->y_gap;

    // Set column address
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4), TAG, "CASET failed");

    // Set row address
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4), TAG, "RASET failed");

    // Transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * jd9853->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), TAG, "RAMWR failed");

    return ESP_OK;
}

static esp_err_t panel_jd9853_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9853->io;
    int command = invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "Invert failed");
    return ESP_OK;
}

static esp_err_t panel_jd9853_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9853->io;
    
    if (mirror_x) {
        jd9853->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        jd9853->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    
    if (mirror_y) {
        jd9853->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        jd9853->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        jd9853->madctl_val
    }, 1), TAG, "MADCTL failed");
    
    return ESP_OK;
}

static esp_err_t panel_jd9853_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9853->io;
    
    if (swap_axes) {
        jd9853->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        jd9853->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        jd9853->madctl_val
    }, 1), TAG, "MADCTL failed");
    
    return ESP_OK;
}

static esp_err_t panel_jd9853_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    jd9853->x_gap = x_gap;
    jd9853->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_jd9853_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    uint8_t cmd = on_off ? JD9853_CMD_DISPON : JD9853_CMD_DISPOFF;
    return esp_lcd_panel_io_tx_param(jd9853->io, cmd, NULL, 0);
}

static esp_err_t panel_jd9853_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    jd9853_panel_t *jd9853 = __containerof(panel, jd9853_panel_t, base);
    uint8_t cmd = sleep ? JD9853_CMD_SLPIN : JD9853_CMD_SLPOUT;
    esp_err_t ret = esp_lcd_panel_io_tx_param(jd9853->io, cmd, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    return ret;
}


