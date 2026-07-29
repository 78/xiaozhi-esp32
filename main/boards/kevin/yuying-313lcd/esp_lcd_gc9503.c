/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>
#include <esp_lcd_panel_commands.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#include "esp_lcd_gc9503.h"

#define GC9503_CMD_MADCTL (0xB1)         // Memory data access control
#define GC9503_CMD_MADCTL_DEFAULT (0x10) // Default value of Memory data access control
#define GC9503_CMD_SS_BIT (1 << 0)       // Source driver scan direction, 0: top to bottom, 1: bottom to top
#define GC9503_CMD_GS_BIT (1 << 1)       // Gate driver scan direction, 0: left to right, 1: right to left
#define GC9503_CMD_BGR_BIT (1 << 5)      // RGB/BGR order, 0: RGB, 1: BGR

typedef struct
{
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // Save current value of GC9503_CMD_MADCTL register
    uint8_t colmod_val; // Save current value of LCD_CMD_COLMOD register
    const gc9503_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct
    {
        unsigned int mirror_by_cmd : 1;
        unsigned int auto_del_panel_io : 1;
        unsigned int display_on_off_use_cmd : 1;
        unsigned int reset_level : 1;
    } flags;
    // To save the original functions of RGB panel
    esp_err_t (*init)(esp_lcd_panel_t *panel);
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*reset)(esp_lcd_panel_t *panel);
    esp_err_t (*mirror)(esp_lcd_panel_t *panel, bool x_axis, bool y_axis);
    esp_err_t (*disp_on_off)(esp_lcd_panel_t *panel, bool on_off);
} gc9503_panel_t;

static const char *TAG = "gc9503";

static esp_err_t panel_gc9503_send_init_cmds(gc9503_panel_t *gc9503);

static esp_err_t panel_gc9503_init(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9503_del(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9503_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9503_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_gc9503_disp_on_off(esp_lcd_panel_t *panel, bool off);

esp_err_t esp_lcd_new_panel_gc9503(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    gc9503_vendor_config_t *vendor_config = (gc9503_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->rgb_config, ESP_ERR_INVALID_ARG, TAG, "`verndor_config` and `rgb_config` are necessary");
    ESP_RETURN_ON_FALSE(!vendor_config->flags.auto_del_panel_io || !vendor_config->flags.mirror_by_cmd,
                        ESP_ERR_INVALID_ARG, TAG, "`mirror_by_cmd` and `auto_del_panel_io` cannot work together");

    esp_err_t ret = ESP_OK;
    gpio_config_t io_conf = {0};

    gc9503_panel_t *gc9503 = (gc9503_panel_t *)calloc(1, sizeof(gc9503_panel_t));
    ESP_RETURN_ON_FALSE(gc9503, ESP_ERR_NO_MEM, TAG, "no mem for gc9503 panel");

    if (panel_dev_config->reset_gpio_num >= 0)
    {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num;
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    gc9503->madctl_val = GC9503_CMD_MADCTL_DEFAULT;
    switch (panel_dev_config->rgb_ele_order)
    {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        gc9503->madctl_val &= ~GC9503_CMD_BGR_BIT;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        gc9503->madctl_val |= GC9503_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color element order");
        break;
    }

    gc9503->colmod_val = 0;
    switch (panel_dev_config->bits_per_pixel)
    {
    case 16: // RGB565
        gc9503->colmod_val = 0x50;
        break;
    case 18: // RGB666
        gc9503->colmod_val = 0x60;
        break;
    case 24: // RGB888
        gc9503->colmod_val = 0x70;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    gc9503->io = io;
    gc9503->init_cmds = vendor_config->init_cmds;
    gc9503->init_cmds_size = vendor_config->init_cmds_size;
    gc9503->reset_gpio_num = panel_dev_config->reset_gpio_num;
    gc9503->flags.reset_level = panel_dev_config->flags.reset_active_high;
    gc9503->flags.auto_del_panel_io = vendor_config->flags.auto_del_panel_io;
    gc9503->flags.mirror_by_cmd = vendor_config->flags.mirror_by_cmd;
    gc9503->flags.display_on_off_use_cmd = (vendor_config->rgb_config->disp_gpio_num >= 0) ? 0 : 1;

    if (gc9503->flags.auto_del_panel_io)
    {
        if (gc9503->reset_gpio_num >= 0)
        { // Perform hardware reset
            gpio_set_level(gc9503->reset_gpio_num, gc9503->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_set_level(gc9503->reset_gpio_num, !gc9503->flags.reset_level);
        }
        else
        { // Perform software reset
            ESP_GOTO_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), err, TAG, "send command failed");
        }
        vTaskDelay(pdMS_TO_TICKS(120));

        /**
         * In order to enable the 3-wire SPI interface pins (such as SDA and SCK) to share other pins of the RGB interface
         * (such as HSYNC) and save GPIOs, we need to send LCD initialization commands via the 3-wire SPI interface before
         * `esp_lcd_new_rgb_panel()` is called.
         */
        ESP_GOTO_ON_ERROR(panel_gc9503_send_init_cmds(gc9503), err, TAG, "send init commands failed");
        // After sending the initialization commands, the 3-wire SPI interface can be deleted
        ESP_GOTO_ON_ERROR(esp_lcd_panel_io_del(io), err, TAG, "delete panel IO failed");
        gc9503->io = NULL;
        ESP_LOGD(TAG, "delete panel IO");
    }

    // Create RGB panel
    ESP_GOTO_ON_ERROR(esp_lcd_new_rgb_panel(vendor_config->rgb_config, ret_panel), err, TAG, "create RGB panel failed");
    ESP_LOGD(TAG, "new RGB panel @%p", ret_panel);

    // Save the original functions of RGB panel
    gc9503->init = (*ret_panel)->init;
    gc9503->del = (*ret_panel)->del;
    gc9503->reset = (*ret_panel)->reset;
    gc9503->mirror = (*ret_panel)->mirror;
    gc9503->disp_on_off = (*ret_panel)->disp_on_off;
    // Overwrite the functions of RGB panel
    (*ret_panel)->init = panel_gc9503_init;
    (*ret_panel)->del = panel_gc9503_del;
    (*ret_panel)->reset = panel_gc9503_reset;
    (*ret_panel)->mirror = panel_gc9503_mirror;
    (*ret_panel)->disp_on_off = panel_gc9503_disp_on_off;
    (*ret_panel)->user_data = gc9503;
    ESP_LOGD(TAG, "new gc9503 panel @%p", gc9503);

    // ESP_LOGI(TAG, "LCD panel create success, version: %d.%d.%d", ESP_LCD_GC9503_VER_MAJOR, ESP_LCD_GC9503_VER_MINOR,
    //          ESP_LCD_GC9503_VER_PATCH);
    return ESP_OK;

err:
    if (gc9503)
    {
        if (panel_dev_config->reset_gpio_num >= 0)
        {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(gc9503);
    }
    return ret;
}

// *INDENT-OFF*
// static const gc9503_lcd_init_cmd_t vendor_specific_init_default[] = {
// //  {cmd, { data }, data_size, delay_ms}
//     {0x11, (uint8_t []){0x00}, 0, 120},

//     {0xf0, (uint8_t []){0x55, 0xaa, 0x52, 0x08, 0x00}, 5, 0},
//     {0xf6, (uint8_t []){0x5a, 0x87}, 2, 0},
//     {0xc1, (uint8_t []){0x3f}, 1, 0},
//     {0xc2, (uint8_t []){0x0e}, 1, 0},
//     {0xc6, (uint8_t []){0xf8}, 1, 0},
//     {0xc9, (uint8_t []){0x10}, 1, 0},
//     {0xcd, (uint8_t []){0x25}, 1, 0},
//     {0xf8, (uint8_t []){0x8a}, 1, 0},
//     {0xac, (uint8_t []){0x45}, 1, 0},
//     {0xa0, (uint8_t []){0xdd}, 1, 0},
//     {0xa7, (uint8_t []){0x47}, 1, 0},
//     {0xfa, (uint8_t []){0x00, 0x00, 0x00, 0x04}, 4, 0},
//     {0x86, (uint8_t []){0x99, 0xa3, 0xa3, 0x51}, 4, 0},
//     {0xa3, (uint8_t []){0xee}, 1, 0},
//     {0xfd, (uint8_t []){0x3c, 0x3c, 0x00}, 3, 0},
//     {0x71, (uint8_t []){0x48}, 1, 0},
//     {0x72, (uint8_t []){0x48}, 1, 0},
//     {0x73, (uint8_t []){0x00, 0x44}, 2, 0},
//     {0x97, (uint8_t []){0xee}, 1, 0},
//     {0x83, (uint8_t []){0x93}, 1, 0},
//     {0x9a, (uint8_t []){0x72}, 1, 0},
//     {0x9b, (uint8_t []){0x5a}, 1, 0},
//     {0x82, (uint8_t []){0x2c, 0x2c}, 2, 0},
//     {0x6d, (uint8_t []){0x00, 0x1f, 0x19, 0x1a, 0x10, 0x0e, 0x0c, 0x0a, 0x02, 0x07, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
//                         0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x08, 0x01, 0x09, 0x0b, 0x0d, 0x0f, 0x1a, 0x19, 0x1f, 0x00}, 32, 0},
//     {0x64, (uint8_t []){0x38, 0x05, 0x01, 0xdb, 0x03, 0x03, 0x38, 0x04, 0x01, 0xdc, 0x03, 0x03, 0x7a, 0x7a, 0x7a, 0x7a}, 16, 0},
//     {0x65, (uint8_t []){0x38, 0x03, 0x01, 0xdd, 0x03, 0x03, 0x38, 0x02, 0x01, 0xde, 0x03, 0x03, 0x7a, 0x7a, 0x7a, 0x7a}, 16, 0},
//     {0x66, (uint8_t []){0x38, 0x01, 0x01, 0xdf, 0x03, 0x03, 0x38, 0x00, 0x01, 0xe0, 0x03, 0x03, 0x7a, 0x7a, 0x7a, 0x7a}, 16, 0},
//     {0x67, (uint8_t []){0x30, 0x01, 0x01, 0xe1, 0x03, 0x03, 0x30, 0x02, 0x01, 0xe2, 0x03, 0x03, 0x7a, 0x7a, 0x7a, 0x7a}, 16, 0},
//     {0x68, (uint8_t []){0x00, 0x08, 0x15, 0x08, 0x15, 0x7a, 0x7a, 0x08, 0x15, 0x08, 0x15, 0x7a, 0x7a}, 13, 0},
//     {0x60, (uint8_t []){0x38, 0x08, 0x7a, 0x7a, 0x38, 0x09, 0x7a, 0x7a}, 8, 0},
//     {0x63, (uint8_t []){0x31, 0xe4, 0x7a, 0x7a, 0x31, 0xe5, 0x7a, 0x7a}, 8, 0},
//     {0x69, (uint8_t []){0x04, 0x22, 0x14, 0x22, 0x14, 0x22, 0x08}, 7, 0},
//     {0x6b, (uint8_t []){0x07}, 1, 0},
//     {0x7a, (uint8_t []){0x08, 0x13}, 2, 0},
//     {0x7b, (uint8_t []){0x08, 0x13}, 2, 0},
//     {0xd1, (uint8_t []){0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18, 0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47, 0x00,
//             0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68, 0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6, 0x02, 0xee,
//             0x03, 0x48, 0x03, 0xa0, 0x03, 0xba, 0x03, 0xc5, 0x03, 0xd0, 0x03, 0xe0, 0x03, 0xea, 0x03, 0xfa, 0x03,
//             0xff}, 52, 0},
//     {0xd2, (uint8_t []){0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18, 0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47, 0x00,
//             0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68, 0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6, 0x02, 0xee,
//             0x03, 0x48, 0x03, 0xa0, 0x03, 0xba, 0x03, 0xc5, 0x03, 0xd0, 0x03, 0xe0, 0x03, 0xea, 0x03, 0xfa, 0x03,
//             0xff}, 52, 0},
//     {0xd3, (uint8_t []){0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18, 0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47, 0x00,
//             0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68, 0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6, 0x02, 0xee,
//             0x03, 0x48, 0x03, 0xa0, 0x03, 0xba, 0x03, 0xc5, 0x03, 0xd0, 0x03, 0xe0, 0x03, 0xea, 0x03, 0xfa, 0x03,
//             0xff}, 52, 0},
//     {0xd4, (uint8_t []){0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18, 0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47, 0x00,
//             0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68, 0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6, 0x02, 0xee,
//             0x03, 0x48, 0x03, 0xa0, 0x03, 0xba, 0x03, 0xc5, 0x03, 0xd0, 0x03, 0xe0, 0x03, 0xea, 0x03, 0xfa, 0x03,
//             0xff}, 52, 0},
//     {0xd5, (uint8_t []){0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18, 0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47, 0x00,
//             0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68, 0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6, 0x02, 0xee,
//             0x03, 0x48, 0x03, 0xa0, 0x03, 0xba, 0x03, 0xc5, 0x03, 0xd0, 0x03, 0xe0, 0x03, 0xea, 0x03, 0xfa, 0x03,
//             0xff}, 52, 0},
//     {0xd6, (uint8_t []){0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18, 0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47, 0x00,
//             0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68, 0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6, 0x02, 0xee,
//             0x03, 0x48, 0x03, 0xa0, 0x03, 0xba, 0x03, 0xc5, 0x03, 0xd0, 0x03, 0xe0, 0x03, 0xea, 0x03, 0xfa, 0x03,
//             0xff}, 52, 0},
//     {0x11, (uint8_t []){0x00}, 0, 120},
//     {0x29, (uint8_t []){0x00}, 0, 20},
// };
static const gc9503_lcd_init_cmd_t vendor_specific_init_default[] = {
    // {0x11, (uint8_t[]){}, 0, 20},

    {0xF0, (uint8_t[]){0x55, 0xAA, 0x52, 0x08, 0x00}, 5, 0},
    {0xF6, (uint8_t[]){0x5A, 0x87}, 2, 0},
    {0xC1, (uint8_t[]){0x3F}, 1, 0},
    {0xCD, (uint8_t[]){0x25}, 1, 0},
    {0xC9, (uint8_t[]){0x10}, 1, 0},
    {0xF8, (uint8_t[]){0x8A}, 1, 0},
    {0xAC, (uint8_t[]){0x45}, 1, 0},
    {0xA7, (uint8_t[]){0x47}, 1, 0},
    {0xA0, (uint8_t[]){0x88}, 1, 0},
    {0x86, (uint8_t[]){0x99, 0xA3, 0xA3, 0x51}, 4, 0},
    {0xFA, (uint8_t[]){0x08, 0x08, 0x00, 0x04}, 4, 0},
    {0xA3, (uint8_t[]){0x6E}, 1, 0},
    {0xFD, (uint8_t[]){0x28, 0x3C, 0x00}, 3, 0},
    {0x9A, (uint8_t[]){0x4B}, 1, 0},
    {0x9B, (uint8_t[]){0x4B}, 1, 0},
    {0x82, (uint8_t[]){0x20, 0x20}, 2, 0},
    {0xB1, (uint8_t[]){0x10}, 1, 0},
    {0x7A, (uint8_t[]){0x0F, 0x13}, 2, 0},
    {0x7B, (uint8_t[]){0x0F, 0x13}, 2, 0},
    {0x6D, (uint8_t[]){0x1e, 0x1e, 0x04, 0x02, 0x0d, 0x1e, 0x12, 0x11, 0x14, 0x13, 0x05, 0x06, 0x1d, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1d, 0x06, 0x05, 0x0b, 0x0c, 0x09, 0x0a, 0x1e, 0x0d, 0x01, 0x03, 0x1e, 0x1e}, 32, 0},
    {0x64, (uint8_t[]){0x38, 0x08, 0x03, 0xc0, 0x03, 0x03, 0x38, 0x06, 0x03, 0xc2, 0x03, 0x03, 0x20, 0x6d, 0x20, 0x6d}, 16, 0},
    {0x65, (uint8_t[]){0x38, 0x04, 0x03, 0xc4, 0x03, 0x03, 0x38, 0x02, 0x03, 0xc6, 0x03, 0x03, 0x20, 0x6d, 0x20, 0x6d}, 16, 0},
    {0x66, (uint8_t[]){0x83, 0xcf, 0x03, 0xc8, 0x03, 0x03, 0x83, 0xd3, 0x03, 0xd2, 0x03, 0x03, 0x20, 0x6d, 0x20, 0x6d}, 16, 0},
    {0x60, (uint8_t[]){0x38, 0x0C, 0x20, 0x6D, 0x38, 0x0B, 0x20, 0x6D}, 8, 0},
    {0x61, (uint8_t[]){0x38, 0x0A, 0x20, 0x6D, 0x38, 0x09, 0x20, 0x6D}, 8, 0},
    {0x62, (uint8_t[]){0x38, 0x25, 0x20, 0x6D, 0x63, 0xC9, 0x20, 0x6D}, 8, 0},
    {0x69, (uint8_t[]){0x14, 0x22, 0x14, 0x22, 0x14, 0x22, 0x08}, 7, 0},
    {0x6B, (uint8_t[]){0x07}, 1, 0},
    {0xD1, (uint8_t[]){0x00, 0x00, 0x00, 0x70, 0x00, 0x8f, 0x00, 0xab, 0x00, 0xbf, 0x00, 0xdf, 0x00, 0xfa, 0x01, 0x2a, 0x01, 0x52, 0x01, 0x90, 0x01, 0xc1, 0x02, 0x0e, 0x02, 0x4f, 0x02, 0x51, 0x02, 0x8d, 0x02, 0xd3, 0x02, 0xff, 0x03, 0x3c, 0x03, 0x64, 0x03, 0xa1, 0x03, 0xf1, 0x03, 0xff, 0x03, 0xfF, 0x03, 0xff, 0x03, 0xFf, 0x03, 0xFF}, 52, 0},
    {0xD2, (uint8_t[]){0x00, 0x00, 0x00, 0x70, 0x00, 0x8f, 0x00, 0xab, 0x00, 0xbf, 0x00, 0xdf, 0x00, 0xfa, 0x01, 0x2a, 0x01, 0x52, 0x01, 0x90, 0x01, 0xc1, 0x02, 0x0e, 0x02, 0x4f, 0x02, 0x51, 0x02, 0x8d, 0x02, 0xd3, 0x02, 0xff, 0x03, 0x3c, 0x03, 0x64, 0x03, 0xa1, 0x03, 0xf1, 0x03, 0xff, 0x03, 0xfF, 0x03, 0xff, 0x03, 0xFf, 0x03, 0xFF}, 52, 0},
    {0xD3, (uint8_t[]){0x00, 0x00, 0x00, 0x70, 0x00, 0x8f, 0x00, 0xab, 0x00, 0xbf, 0x00, 0xdf, 0x00, 0xfa, 0x01, 0x2a, 0x01, 0x52, 0x01, 0x90, 0x01, 0xc1, 0x02, 0x0e, 0x02, 0x4f, 0x02, 0x51, 0x02, 0x8d, 0x02, 0xd3, 0x02, 0xff, 0x03, 0x3c, 0x03, 0x64, 0x03, 0xa1, 0x03, 0xf1, 0x03, 0xff, 0x03, 0xfF, 0x03, 0xff, 0x03, 0xFf, 0x03, 0xFF}, 52, 0},
    {0xD4, (uint8_t[]){0x00, 0x00, 0x00, 0x70, 0x00, 0x8f, 0x00, 0xab, 0x00, 0xbf, 0x00, 0xdf, 0x00, 0xfa, 0x01, 0x2a, 0x01, 0x52, 0x01, 0x90, 0x01, 0xc1, 0x02, 0x0e, 0x02, 0x4f, 0x02, 0x51, 0x02, 0x8d, 0x02, 0xd3, 0x02, 0xff, 0x03, 0x3c, 0x03, 0x64, 0x03, 0xa1, 0x03, 0xf1, 0x03, 0xff, 0x03, 0xfF, 0x03, 0xff, 0x03, 0xFf, 0x03, 0xFF}, 52, 0},
    {0xD5, (uint8_t[]){0x00, 0x00, 0x00, 0x70, 0x00, 0x8f, 0x00, 0xab, 0x00, 0xbf, 0x00, 0xdf, 0x00, 0xfa, 0x01, 0x2a, 0x01, 0x52, 0x01, 0x90, 0x01, 0xc1, 0x02, 0x0e, 0x02, 0x4f, 0x02, 0x51, 0x02, 0x8d, 0x02, 0xd3, 0x02, 0xff, 0x03, 0x3c, 0x03, 0x64, 0x03, 0xa1, 0x03, 0xf1, 0x03, 0xff, 0x03, 0xfF, 0x03, 0xff, 0x03, 0xFf, 0x03, 0xFF}, 52, 0},
    {0xD6, (uint8_t[]){0x00, 0x00, 0x00, 0x70, 0x00, 0x8f, 0x00, 0xab, 0x00, 0xbf, 0x00, 0xdf, 0x00, 0xfa, 0x01, 0x2a, 0x01, 0x52, 0x01, 0x90, 0x01, 0xc1, 0x02, 0x0e, 0x02, 0x4f, 0x02, 0x51, 0x02, 0x8d, 0x02, 0xd3, 0x02, 0xff, 0x03, 0x3c, 0x03, 0x64, 0x03, 0xa1, 0x03, 0xf1, 0x03, 0xff, 0x03, 0xfF, 0x03, 0xff, 0x03, 0xFf, 0x03, 0xFF}, 52, 0},
    // {0x3A, (uint8_t[]){0x55}, 1, 0},

    {0x11, NULL, 0, 120}, // Delay 120ms
    {0x29, NULL, 0, 120}};

// *INDENT-OFF*

static esp_err_t panel_gc9503_send_init_cmds(gc9503_panel_t *gc9503)
{
    esp_lcd_panel_io_handle_t io = gc9503->io;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, GC9503_CMD_MADCTL, (uint8_t[]){
                                                                             gc9503->madctl_val,
                                                                         },
                                                  1),
                        TAG, "send command failed");
    ;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]){
                                                                          gc9503->colmod_val,
                                                                      },
                                                  1),
                        TAG, "send command failed");
    ;

    // Vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    const gc9503_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    if (gc9503->init_cmds)
    {
        init_cmds = gc9503->init_cmds;
        init_cmds_size = gc9503->init_cmds_size;
    }
    else
    {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(gc9503_lcd_init_cmd_t);
    }

    bool is_cmd_overwritten = false;
    for (int i = 0; i < init_cmds_size; i++)
    {
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd)
        {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            gc9503->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            gc9503->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        default:
            is_cmd_overwritten = false;
            break;
        }

        if (is_cmd_overwritten)
        {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence",
                     init_cmds[i].cmd);
        }

        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes),
                            TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_gc9503_init(esp_lcd_panel_t *panel)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;

    if (!gc9503->flags.auto_del_panel_io)
    {
        ESP_RETURN_ON_ERROR(panel_gc9503_send_init_cmds(gc9503), TAG, "send init commands failed");
    }
    // Init RGB panel
    ESP_RETURN_ON_ERROR(gc9503->init(panel), TAG, "init RGB panel failed");

    return ESP_OK;
}

static esp_err_t panel_gc9503_del(esp_lcd_panel_t *panel)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;

    if (gc9503->reset_gpio_num >= 0)
    {
        gpio_reset_pin(gc9503->reset_gpio_num);
    }
    // Delete RGB panel
    gc9503->del(panel);
    free(gc9503);
    ESP_LOGD(TAG, "del gc9503 panel @%p", gc9503);
    return ESP_OK;
}

static esp_err_t panel_gc9503_reset(esp_lcd_panel_t *panel)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = gc9503->io;

    // Perform hardware reset
    if (gc9503->reset_gpio_num >= 0)
    {
        gpio_set_level(gc9503->reset_gpio_num, gc9503->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(gc9503->reset_gpio_num, !gc9503->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    else if (io)
    { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    // Reset RGB panel
    ESP_RETURN_ON_ERROR(gc9503->reset(panel), TAG, "reset RGB panel failed");

    return ESP_OK;
}

static esp_err_t panel_gc9503_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = gc9503->io;

    if (gc9503->flags.mirror_by_cmd)
    {
        ESP_RETURN_ON_FALSE(io, ESP_FAIL, TAG, "Panel IO is deleted, cannot send command");
        // Control mirror through LCD command
        if (mirror_x)
        {
            gc9503->madctl_val |= GC9503_CMD_GS_BIT;
        }
        else
        {
            gc9503->madctl_val &= ~GC9503_CMD_GS_BIT;
        }
        if (mirror_y)
        {
            gc9503->madctl_val |= GC9503_CMD_SS_BIT;
        }
        else
        {
            gc9503->madctl_val &= ~GC9503_CMD_SS_BIT;
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, GC9503_CMD_MADCTL, (uint8_t[]){
                                                                                 gc9503->madctl_val,
                                                                             },
                                                      1),
                            TAG, "send command failed");
        ;
    }
    else
    {
        // Control mirror through RGB panel
        ESP_RETURN_ON_ERROR(gc9503->mirror(panel, mirror_x, mirror_y), TAG, "RGB panel mirror failed");
    }
    return ESP_OK;
}

static esp_err_t panel_gc9503_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = gc9503->io;
    int command = 0;

    if (gc9503->flags.display_on_off_use_cmd)
    {
        ESP_RETURN_ON_FALSE(io, ESP_FAIL, TAG, "Panel IO is deleted, cannot send command");
        // Control display on/off through LCD command
        if (on_off)
        {
            command = LCD_CMD_DISPON;
        }
        else
        {
            command = LCD_CMD_DISPOFF;
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    }
    else
    {
        // Control display on/off through display control signal
        ESP_RETURN_ON_ERROR(gc9503->disp_on_off(panel, on_off), TAG, "RGB panel disp_on_off failed");
    }
    return ESP_OK;
}
