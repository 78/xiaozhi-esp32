/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_lcd_st7123.h"

#define ST7123_PAD_CONTROL  (0xB7)
#define ST7123_DSI_2_LANE   (0x03)
#define ST7123_DSI_3_4_LANE (0x02)

#define ST7123_CMD_GS_BIT   (1 << 0)
#define ST7123_CMD_SS_BIT   (1 << 1)

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save surrent value of LCD_CMD_COLMOD register
    const st7123_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    uint8_t lane_num;
    struct {
        unsigned int reset_level: 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} st7123_panel_t;

static const char *TAG = "st7123";

static esp_err_t panel_st7123_del(esp_lcd_panel_t *panel);
static esp_err_t panel_st7123_init(esp_lcd_panel_t *panel);
static esp_err_t panel_st7123_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_st7123_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_st7123_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_st7123_disp_on_off(esp_lcd_panel_t *panel, bool on_off);
static esp_err_t panel_st7123_sleep(esp_lcd_panel_t *panel, bool sleep);

esp_err_t esp_lcd_new_panel_st7123(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                            esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    st7123_vendor_config_t *vendor_config = (st7123_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus, ESP_ERR_INVALID_ARG, TAG,
                    "invalid vendor config");

    esp_err_t ret = ESP_OK;
    st7123_panel_t *st7123 = (st7123_panel_t *)calloc(1, sizeof(st7123_panel_t));
    ESP_RETURN_ON_FALSE(st7123, ESP_ERR_NO_MEM, TAG, "no mem for st7123 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
        case LCD_RGB_ELEMENT_ORDER_RGB:
        st7123->madctl_val = 0;
        break;
        case LCD_RGB_ELEMENT_ORDER_BGR:
        st7123->madctl_val |= LCD_CMD_BGR_BIT;
        break;
        default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
        case 16: // RGB565
        st7123->colmod_val = 0x55;
        break;
        case 18: // RGB666
        st7123->colmod_val = 0x66;
        break;
        case 24: // RGB888
        st7123->colmod_val = 0x77;
        break;
        default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    st7123->io = io;
    st7123->init_cmds = vendor_config->init_cmds;
    st7123->init_cmds_size = vendor_config->init_cmds_size;
    st7123->lane_num = vendor_config->mipi_config.lane_num;
    st7123->reset_gpio_num = panel_dev_config->reset_gpio_num;
    st7123->flags.reset_level = panel_dev_config->flags.reset_active_high;

    // Create MIPI DPI panel
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, ret_panel), err, TAG,
                "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", *ret_panel);

    // Save the original functions of MIPI DPI panel
    st7123->del = (*ret_panel)->del;
    st7123->init = (*ret_panel)->init;
    // Overwrite the functions of MIPI DPI panel
    (*ret_panel)->del = panel_st7123_del;
    (*ret_panel)->init = panel_st7123_init;
    (*ret_panel)->reset = panel_st7123_reset;
    (*ret_panel)->mirror = panel_st7123_mirror;
    (*ret_panel)->invert_color = panel_st7123_invert_color;
    (*ret_panel)->disp_on_off = panel_st7123_disp_on_off;
    (*ret_panel)->disp_sleep = panel_st7123_sleep;
    (*ret_panel)->user_data = st7123;
    ESP_LOGD(TAG, "new st7123 panel @%p", st7123);

    return ESP_OK;

    err:
        if (st7123) {
            if (panel_dev_config->reset_gpio_num >= 0) {
                gpio_reset_pin(panel_dev_config->reset_gpio_num);
            }
            free(st7123);
        }
    return ret;
}
 
static const st7123_lcd_init_cmd_t vendor_specific_init_default[] = {
    // {cmd, { data }, data_size, delay_ms}
    // TODO: 
    {0x60, (uint8_t []){0x71,0x23,0xa2}, 3, 0},
    {0x60, (uint8_t []){0x71,0x23,0xa3}, 3, 0},
    {0x60, (uint8_t []){0x71,0x23,0xa4}, 3, 0},
    {0xA4, (uint8_t []){0x31}, 1, 0},
    {0xD7, (uint8_t []){0x10,0x0A,0x10,0x2A,0x80,0x80}, 6, 0},
    {0x90, (uint8_t []){0x71,0x23,0x5A,0x20,0x24,0x09,0x09}, 7, 0},
    {0xA3, (uint8_t []){0x80,0x01,0x88,0x30,0x05,0x00,0x00,0x00,0x00,0x00,0x46,0x00,0x00,0x1E,0x5C,0x1E,0x80,0x00,0x4F,0x05,0x00,0x00,0x00,0x00,0x00,0x46,0x00,0x00,0x1E,0x5C,0x1E,0x80,0x00,0x6F,0x58,0x00,0x00,0x00,0xFF}, 40, 0},
    {0xA6, (uint8_t []){0x03,0x00,0x24,0x55,0x36,0x00,0x39,0x00,0x6E,0x6E,0x91,0xFF,0x00,0x24,0x55,0x38,0x00,0x37,0x00,0x6E,0x6E,0x91,0xFF,0x00,0x24,0x11,0x00,0x00,0x00,0x00,0x6E,0x6E,0x91,0xFF,0x00,0xEC,0x11,0x00,0x03,0x00,0x03,0x6E,0x6E,0xFF,0xFF,0x00,0x08,0x80,0x08,0x80,0x06,0x00,0x00,0x00,0x00}, 55, 0},
    {0xA7, (uint8_t []){0x19,0x19,0x80,0x64,0x40,0x07,0x16,0x40,0x00,0x44,0x03,0x6E,0x6E,0x91,0xFF,0x08,0x80,0x64,0x40,0x25,0x34,0x40,0x00,0x02,0x01,0x6E,0x6E,0x91,0xFF,0x08,0x80,0x64,0x40,0x00,0x00,0x40,0x00,0x00,0x00,0x6E,0x6E,0x91,0xFF,0x08,0x80,0x64,0x40,0x00,0x00,0x00,0x00,0x20,0x00,0x6E,0x6E,0x84,0xFF,0x08,0x80,0x44}, 60, 0},
    {0xAC, (uint8_t []){0x03,0x19,0x19,0x18,0x18,0x06,0x13,0x13,0x11,0x11,0x08,0x08,0x0A,0x0A,0x1C,0x1C,0x07,0x07,0x00,0x00,0x02,0x02,0x01,0x19,0x19,0x18,0x18,0x06,0x12,0x12,0x10,0x10,0x09,0x09,0x0B,0x0B,0x1C,0x1C,0x07,0x07,0x03,0x03,0x01,0x01}, 44, 0},
    {0xAD, (uint8_t []){0xF0,0x00,0x46,0x00,0x03,0x50,0x50,0xFF,0xFF,0xF0,0x40,0x06,0x01,0x07,0x42,0x42,0xFF,0xFF,0x01,0x00,0x00,0xFF,0xFF,0xFF,0xFF}, 25, 0},
    {0xAE, (uint8_t []){0xFE,0x3F,0x3F,0xFE,0x3F,0x3F,0x00}, 7, 0},
    {0xB2, (uint8_t []){0x15,0x19,0x05,0x23,0x49,0xAF,0x03,0x2E,0x5C,0xD2,0xFF,0x10,0x20,0xFD,0x20,0xC0,0x00}, 17, 0},
    {0xE8, (uint8_t []){0x20,0x6F,0x04,0x97,0x97,0x3E,0x04,0xDC,0xDC,0x3E,0x06,0xFA,0x26,0x3E}, 15, 0},
    {0x75, (uint8_t []){0x03,0x04}, 2, 0},
    {0xE7, (uint8_t []){0x3B,0x00,0x00,0x7C,0xA1,0x8C,0x20,0x1A,0xF0,0xB1,0x50,0x00,0x50,0xB1,0x50,0xB1,0x50,0xD8,0x00,0x55,0x00,0xB1,0x00,0x45,0xC9,0x6A,0xFF,0x5A,0xD8,0x18,0x88,0x15,0xB1,0x01,0x01,0x77}, 36, 0},
    {0xEA, (uint8_t []){0x13,0x00,0x04,0x00,0x00,0x00,0x00,0x2C}, 8, 0},
    {0xB0, (uint8_t []){0x22,0x43,0x11,0x61,0x25,0x43,0x43}, 7, 0},
    {0xb7, (uint8_t []){0x00,0x00,0x73,0x73}, 0x04, 0},
    {0xBF, (uint8_t []){0xA6,0XAA}, 2, 0},
    {0xA9, (uint8_t []){0x00,0x00,0x73,0xFF,0x00,0x00,0x03,0x00,0x00,0x03}, 10, 0},
    {0xC8, (uint8_t []){0x00,0x00,0x10,0x1F,0x36,0x00,0x5D,0x04,0x9D,0x05,0x10,0xF2,0x06,0x60,0x03,0x11,0xAD,0x00,0xEF,0x01,0x22,0x2E,0x0E,0x74,0x08,0x32,0xDC,0x09,0x33,0x0F,0xF3,0x77,0x0D,0xB0,0xDC,0x03,0xFF}, 37, 0},
    {0xC9, (uint8_t []){0x00,0x00,0x10,0x1F,0x36,0x00,0x5D,0x04,0x9D,0x05,0x10,0xF2,0x06,0x60,0x03,0x11,0xAD,0x00,0xEF,0x01,0x22,0x2E,0x0E,0x74,0x08,0x32,0xDC,0x09,0x33,0x0F,0xF3,0x77,0x0D,0xB0,0xDC,0x03,0xFF}, 37, 0},
    {0x36, (uint8_t []){0x03}, 1, 0},
    {0x11, (uint8_t []){0x00}, 1, 100},
    {0x29, (uint8_t []){0x00}, 1, 0},
    {0x35, (uint8_t []){0x00}, 1, 100},
    //============ Gamma END===========
};

static esp_err_t panel_st7123_del(esp_lcd_panel_t *panel)
{
    st7123_panel_t *st7123 = (st7123_panel_t *)panel->user_data;

    if (st7123->reset_gpio_num >= 0) {
        gpio_reset_pin(st7123->reset_gpio_num);
    }
    // Delete MIPI DPI panel
    st7123->del(panel);
    ESP_LOGD(TAG, "del st7123 panel @%p", st7123);
    free(st7123);

    return ESP_OK;
}

static esp_err_t panel_st7123_init(esp_lcd_panel_t *panel)
{
    st7123_panel_t *st7123 = (st7123_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st7123->io;
    const st7123_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;

    // switch (st7123->lane_num) {
    // case 0:
    // case 2:
    //     lane_command = ST7123_DSI_2_LANE;
    //     break;
    // case 3:
    // case 4:
    //     lane_command = ST7123_DSI_3_4_LANE;
    //     break;
    // default:
    //     ESP_LOGE(TAG, "Invalid lane number %d", st7123->lane_num);
    //     return ESP_ERR_INVALID_ARG;
    // }

    uint8_t ID[3];
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(io, 0x04, ID, 3), TAG, "read ID failed");
    ESP_LOGI(TAG, "LCD ID: %02X %02X %02X", ID[0], ID[1], ID[2]);

    // // For modifying MIPI-DSI lane settings
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ST7123_PAD_CONTROL, (uint8_t[]) {
    //     lane_command,
    // }, 1), TAG, "send command failed");

    // // back to CMD_Page 0
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ST7123_CMD_CNDBKxSEL, (uint8_t[]) {
    //     ST7123_CMD_BKxSEL_BYTE0, ST7123_CMD_BKxSEL_BYTE1, ST7123_CMD_BKxSEL_BYTE2_PAGE0
    // }, 3), TAG, "send command failed");
    // // exit sleep mode
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG,
    //                     "io tx param failed");
    // vTaskDelay(pdMS_TO_TICKS(120));

    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
    //     st7123->madctl_val,
    // }, 1), TAG, "send command failed");
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
    //     st7123->colmod_val,
    // }, 1), TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (st7123->init_cmds) {
        init_cmds = st7123->init_cmds;
        init_cmds_size = st7123->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(st7123_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        // if (is_command0_enable && init_cmds[i].data_bytes > 0) {
        //     switch (init_cmds[i].cmd) {
        //     case LCD_CMD_MADCTL:
        //         is_cmd_overwritten = true;
        //         st7123->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
        //         break;
        //     case LCD_CMD_COLMOD:
        //         is_cmd_overwritten = true;
        //         st7123->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
        //         break;
        //     default:
        //         is_cmd_overwritten = false;
        //         break;
        //     }

        //     if (is_cmd_overwritten) {
        //         is_cmd_overwritten = false;
        //         ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence",
        //                  init_cmds[i].cmd);
        //     }
        // }

        // Send command
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));

        // if ((init_cmds[i].cmd == ST7123_CMD_CNDBKxSEL) && (((uint8_t *)init_cmds[i].data)[2] == ST7123_CMD_BKxSEL_BYTE2_PAGE0)) {
        //     is_command0_enable = true;
        // } else if ((init_cmds[i].cmd == ST7123_CMD_CNDBKxSEL) && (((uint8_t *)init_cmds[i].data)[2] != ST7123_CMD_BKxSEL_BYTE2_PAGE0)) {
        //     is_command0_enable = false;
        // }
    }
    ESP_LOGD(TAG, "send init commands success");

    ESP_RETURN_ON_ERROR(st7123->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_st7123_reset(esp_lcd_panel_t *panel)
{
    st7123_panel_t *st7123 = (st7123_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st7123->io;

    // Perform hardware reset
    if (st7123->reset_gpio_num >= 0) {
        gpio_set_level(st7123->reset_gpio_num, st7123->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(st7123->reset_gpio_num, !st7123->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(50));
    } else if (io) { // Perform software reset
        // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static esp_err_t panel_st7123_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    // st7123_panel_t *st7123 = (st7123_panel_t *)panel->user_data;
    // esp_lcd_panel_io_handle_t io = st7123->io;
    // uint8_t command = 0;

    // ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    // if (invert_color_data) {
    //     command = LCD_CMD_INVON;
    // } else {
    //     command = LCD_CMD_INVOFF;
    // }
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");

    return ESP_OK;
}

static esp_err_t panel_st7123_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    // st7123_panel_t *st7123 = (st7123_panel_t *)panel->user_data;
    // esp_lcd_panel_io_handle_t io = st7123->io;
    // uint8_t madctl_val = st7123->madctl_val;

    // ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    // // Control mirror through LCD command
    // if (mirror_x) {
    //     madctl_val |= ST7123_CMD_GS_BIT;
    // } else {
    //     madctl_val &= ~ST7123_CMD_GS_BIT;
    // }
    // if (mirror_y) {
    //     madctl_val |= ST7123_CMD_SS_BIT;
    // } else {
    //     madctl_val &= ~ST7123_CMD_SS_BIT;
    // }

    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
    //     madctl_val
    // }, 1), TAG, "send command failed");
    // st7123->madctl_val = madctl_val;

    return ESP_OK;
}

static esp_err_t panel_st7123_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    // st7123_panel_t *st7123 = (st7123_panel_t *)panel->user_data;
    // esp_lcd_panel_io_handle_t io = st7123->io;
    // int command = 0;

    // if (on_off) {
    //     command = LCD_CMD_DISPON;
    // } else {
    //     command = LCD_CMD_DISPOFF;
    // }
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
 
static esp_err_t panel_st7123_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    // st7123_panel_t *st7123 = (st7123_panel_t *)panel->user_data;
    // esp_lcd_panel_io_handle_t io = st7123->io;
    // int command = 0;

    // if (sleep) {
    //     command = LCD_CMD_SLPIN;
    // } else {
    //     command = LCD_CMD_SLPOUT;
    // }
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    // vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}
#endif  // SOC_MIPI_DSI_SUPPORTED
