/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "hi8561_driver.h"

#define HI8561_PAD_CONTROL (0xB2)
#define HI8561_DSI_2_LANE (0x10)
#define HI8561_DSI_4_LANE (0x00)

#define HI8561_CMD_SHLR_BIT (1ULL << 0)
#define HI8561_CMD_UPDN_BIT (1ULL << 1)
#define HI8561_MDCTL_VALUE_DEFAULT (0x01)

static const hi8561_lcd_init_cmd_t vendor_specific_init_default[] = {
    //  {cmd, { data }, data_size, delay_ms}
    /**** CMD_Page 3 ****/
    {0xDF, (uint8_t[]){0x90, 0x69, 0xF9}, 3, 0},
    {0xDE, (uint8_t[]){0x00}, 1, 0},
    {0xBB, (uint8_t[]){0x0F, 0x10, 0x43, 0x50, 0x32, 0x44, 0x44}, 7, 0},
    {0xBF, (uint8_t[]){0x46, 0x32}, 2, 0},
    {0xC0, (uint8_t[]){0x01, 0xAD, 0x01, 0xAD}, 4, 0},
    {0xBD, (uint8_t[]){0x00, 0xB4}, 2, 0},
    {0xC6, (uint8_t[]){0x00, 0x7D, 0x00, 0xC8, 0x00, 0x17, 0x1A, 0x82, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01}, 23, 0},
    {0xC8, (uint8_t[]){0x23, 0x48, 0x87}, 3, 0},
    // {0xCC, (uint8_t[]){0x33}, 1, 0},//4lane
    {0xCC, (uint8_t[]){0x31}, 1, 0}, // 2lane
    // {0xCC, (uint8_t[]){0x30}, 1, 0}, // 1lane
    {0xBC, (uint8_t[]){0x2E, 0x80, 0x84}, 3, 0},
    {0xC3, (uint8_t[]){0x3B, 0x01, 0x02, 0x05, 0x0C, 0x0C, 0x75, 0x0A, 0x79, 0x0A, 0x79, 0x02, 0x6E, 0x02, 0x6E, 0x02, 0x6E, 0x0A, 0x0D, 0x0A, 0x0F, 0x0A, 0x0F, 0x0A, 0x0F}, 25, 0},
    {0xC4, (uint8_t[]){0x01, 0x02, 0x05, 0x0C, 0x0C, 0x75, 0x0A, 0x79, 0x0A, 0x79, 0x02, 0x6E, 0x02, 0x6E, 0x02, 0x6E, 0x0A, 0x0D, 0x0A, 0x0F, 0x0A, 0x0F, 0x0A, 0x0F}, 24, 0},
    {0xC5, (uint8_t[]){0x03, 0x05, 0x0C, 0x0C, 0x75, 0x0A, 0x79, 0x0A, 0x79, 0x02, 0x6E, 0x02, 0x6E, 0x02, 0x6E, 0x0A, 0x0D, 0x0A, 0x0F, 0x0A, 0x0F, 0x0A, 0x0F}, 23, 0},
    {0xD7, (uint8_t[]){0x00, 0x0A, 0x63, 0x0A, 0x63, 0x0A, 0x63, 0x0A, 0x63, 0x0A, 0x63, 0x0A, 0x63, 0x0A, 0x63, 0x0A, 0x63}, 17, 0},
    {0xCB, (uint8_t[]){0x7F, 0x78, 0x71, 0x64, 0x5A, 0x58, 0x4B, 0x51, 0x3A, 0x53, 0x51, 0x4F, 0x6A, 0x54, 0x57, 0x46, 0x3F, 0x2F, 0x1B, 0x0F, 0x08, 0x7F, 0x78, 0x71, 0x64, 0x5A, 0x58, 0x4B, 0x51, 0x3A, 0x53, 0x51, 0x4F, 0x6A, 0x54, 0x57, 0x46, 0x3F, 0x2F, 0x1B, 0x0F, 0x08, 0x00}, 43, 0},
    {0xCE, (uint8_t[]){0x00, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C}, 23, 0},
    {0xCF, (uint8_t[]){0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 45, 0},
    {0xD0, (uint8_t[]){0x00, 0x1F, 0x1F, 0x11, 0x1E, 0x1F, 0x0F, 0x0F, 0x0D, 0x0D, 0x0B, 0x0B, 0x09, 0x09, 0x07, 0x07, 0x05, 0x05, 0x01, 0x1F, 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 29, 0},
    {0xD1, (uint8_t[]){0x00, 0x1F, 0x1F, 0x10, 0x1E, 0x1F, 0x0E, 0x0E, 0x0C, 0x0C, 0x0A, 0x0A, 0x08, 0x08, 0x06, 0x06, 0x04, 0x04, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 29, 0},
    {0xD2, (uint8_t[]){0x00, 0x5F, 0x1F, 0x10, 0x1F, 0x1E, 0x08, 0x08, 0x4A, 0x0A, 0x0C, 0x0C, 0x0E, 0x0E, 0x04, 0x04, 0x06, 0x06, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 29, 0},
    {0xD3, (uint8_t[]){0x00, 0x1F, 0x1F, 0x11, 0x1F, 0x1E, 0x09, 0x09, 0x0B, 0x0B, 0x0D, 0x0D, 0x0F, 0x0F, 0x05, 0x05, 0x07, 0x07, 0x01, 0x1F, 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 29, 0},
    {0xD4, (uint8_t[]){0x00, 0x20, 0x0B, 0x00, 0x0D, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x00, 0x81, 0x04, 0xAE, 0x04, 0xB0, 0x04, 0xB2, 0x04, 0xB4, 0x04, 0xB6, 0x04, 0xB8, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x06, 0x44, 0x06, 0x46, 0x03, 0x03, 0x00, 0x00, 0x07, 0x00, 0x06, 0x04, 0xA7, 0x04, 0xA8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x01, 0x00, 0x00, 0x20, 0x00}, 87, 0},
    {0xD5, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x07, 0x32, 0x5A, 0x00, 0x00, 0x3C, 0x00, 0x1E, 0x00, 0x1E, 0xB3, 0x00, 0x0F, 0x06, 0x0C, 0x00, 0x71, 0x20, 0x04, 0x10, 0x04, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x1F, 0xFF, 0x00, 0x00, 0x00, 0x1F, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 61, 0},
    {0xCD, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0xDE, (uint8_t[]){0x01}, 1, 0},
    {0xB9, (uint8_t[]){0x00, 0xFF, 0xFF, 0x04}, 4, 0},
    {0xC7, (uint8_t[]){0x1F, 0x14, 0x0E}, 3, 0},

    {0xDE, (uint8_t[]){0x02}, 1, 0},
    {0xE5, (uint8_t[]){0x00, 0x60, 0x60, 0x02, 0x18, 0x60, 0x18, 0x60, 0x09, 0x04, 0x00, 0xC5, 0x01, 0x2C, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x04}, 24, 0},
    {0xE6, (uint8_t[]){0x10, 0x10, 0x82}, 3, 0},
    {0xC4, (uint8_t[]){0x00, 0x11, 0x07, 0x00, 0x11, 0x01, 0x08}, 7, 0},
    {0xC3, (uint8_t[]){0x20, 0xFF}, 2, 0},
    {0xBD, (uint8_t[]){0x1B}, 1, 0},
    {0xC6, (uint8_t[]){0x4A, 0x00}, 2, 0},
    {0xCD, (uint8_t[]){0x14, 0x64, 0x11, 0x40}, 4, 0},
    {0xC1, (uint8_t[]){0x00, 0x40, 0x00, 0x02, 0x02, 0x02, 0x02, 0x7F, 0x00, 0x00}, 10, 0},
    {0xB3, (uint8_t[]){0x00, 0xA8}, 2, 0},
    {0xBB, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x40, 0x43, 0x04}, 11, 0},
    {0xC2, (uint8_t[]){0x02, 0x42, 0x50, 0x00, 0x02, 0xE4, 0x61, 0x73, 0xF9, 0x08}, 10, 0},
    {0xEC, (uint8_t[]){0x07, 0x07, 0x40, 0x00, 0x22, 0x02, 0x00, 0xFF, 0x08, 0x7C, 0x00, 0x00, 0x00, 0x00}, 14, 0},

    {0xDE, (uint8_t[]){0x03}, 1, 0},
    {0xD1, (uint8_t[]){0x00, 0x00, 0x21, 0xFF, 0x00}, 5, 0},

    {0xDE, (uint8_t[]){0x00}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 0, 30},

    {0x11, (uint8_t[]){0x00}, 0, 120},

    {0x29, (uint8_t[]){0x00}, 0, 50},

    //============ Gamma END===========
};

typedef struct
{
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    const hi8561_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    uint8_t lane_num;
    struct
    {
        unsigned int reset_level : 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} hi8561_panel_t;

static const char *TAG = "hi8561";

static esp_err_t panel_hi8561_send_init_cmds(hi8561_panel_t *hi8561);

static esp_err_t panel_hi8561_del(esp_lcd_panel_t *panel);
static esp_err_t panel_hi8561_init(esp_lcd_panel_t *panel);
static esp_err_t panel_hi8561_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_hi8561_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_hi8561_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_hi8561_sleep(esp_lcd_panel_t *panel, bool sleep);
static esp_err_t panel_hi8561_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_hi8561(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel)
{
    // ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_hi8561_VER_MAJOR, ESP_LCD_hi8561_VER_MINOR,
    //          ESP_LCD_hi8561_VER_PATCH);
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    hi8561_vendor_config_t *vendor_config = (hi8561_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus, ESP_ERR_INVALID_ARG, TAG,
                        "invalid vendor config");

    esp_err_t ret = ESP_OK;
    hi8561_panel_t *hi8561 = (hi8561_panel_t *)calloc(1, sizeof(hi8561_panel_t));
    ESP_RETURN_ON_FALSE(hi8561, ESP_ERR_NO_MEM, TAG, "no mem for hi8561 panel");

    if (panel_dev_config->reset_gpio_num >= 0)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    hi8561->io = io;
    hi8561->init_cmds = vendor_config->init_cmds;
    hi8561->init_cmds_size = vendor_config->init_cmds_size;
    hi8561->lane_num = vendor_config->mipi_config.lane_num;
    hi8561->reset_gpio_num = panel_dev_config->reset_gpio_num;
    hi8561->flags.reset_level = panel_dev_config->flags.reset_active_high;
    hi8561->madctl_val = HI8561_MDCTL_VALUE_DEFAULT;

    // Create MIPI DPI panel
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, ret_panel), err, TAG,
                      "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", *ret_panel);

    // Save the original functions of MIPI DPI panel
    hi8561->del = (*ret_panel)->del;
    hi8561->init = (*ret_panel)->init;
    // Overwrite the functions of MIPI DPI panel
    (*ret_panel)->del = panel_hi8561_del;
    (*ret_panel)->init = panel_hi8561_init;
    (*ret_panel)->reset = panel_hi8561_reset;
    (*ret_panel)->mirror = panel_hi8561_mirror;
    (*ret_panel)->invert_color = panel_hi8561_invert_color;
    (*ret_panel)->disp_sleep = panel_hi8561_sleep;
    (*ret_panel)->disp_on_off = panel_hi8561_on_off;
    (*ret_panel)->user_data = hi8561;

    ESP_LOGD(TAG, "new hi8561 panel @%p", hi8561);

    return ESP_OK;

err:
    if (hi8561)
    {
        if (panel_dev_config->reset_gpio_num >= 0)
        {
            gpio_reset_pin(static_cast<gpio_num_t>(panel_dev_config->reset_gpio_num));
        }
        free(hi8561);
    }
    return ret;
}

static esp_err_t panel_hi8561_send_init_cmds(hi8561_panel_t *hi8561)
{
    esp_lcd_panel_io_handle_t io = hi8561->io;
    const hi8561_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    // uint8_t lane_command = HI8561_DSI_2_LANE;
    // bool is_cmd_overwritten = false;

    // switch (hi8561->lane_num)
    // {
    // case 0:
    // case 2:
    //     lane_command = HI8561_DSI_2_LANE;
    //     break;
    // case 4:
    //     lane_command = HI8561_DSI_4_LANE;
    //     break;
    // default:
    //     ESP_LOGE(TAG, "Invalid lane number %d", hi8561->lane_num);
    //     return ESP_ERR_INVALID_ARG;
    // }
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, HI8561_PAD_CONTROL, (uint8_t[]){
    //                                                                           lane_command,
    //                                                                       },
    //                                               1),
    //                     TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    // if (hi8561->init_cmds)
    // {
    //     init_cmds = hi8561->init_cmds;
    //     init_cmds_size = hi8561->init_cmds_size;
    // }
    // else
    // {
    //     init_cmds = vendor_specific_init_default;
    //     init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(hi8561_lcd_init_cmd_t);
    // }

    init_cmds = vendor_specific_init_default;
    init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(hi8561_lcd_init_cmd_t);

    for (int i = 0; i < init_cmds_size; i++)
    {
        //     // Check if the command has been used or conflicts with the internal
        //     if (init_cmds[i].data_bytes > 0)
        //     {
        //         switch (init_cmds[i].cmd)
        //         {
        //         case LCD_CMD_MADCTL:
        //             is_cmd_overwritten = true;
        //             hi8561->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
        //             break;
        //         default:
        //             is_cmd_overwritten = false;
        //             break;
        //         }

        //         if (is_cmd_overwritten)
        //         {
        //             is_cmd_overwritten = false;
        //             ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence",
        //                      init_cmds[i].cmd);
        //         }
        //     }

        esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes);
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
        // printf("Ciallo\n");
    }

    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_hi8561_del(esp_lcd_panel_t *panel)
{
    hi8561_panel_t *hi8561 = (hi8561_panel_t *)panel->user_data;

    if (hi8561->reset_gpio_num >= 0)
    {
        gpio_reset_pin(static_cast<gpio_num_t>(hi8561->reset_gpio_num));
    }
    // Delete MIPI DPI panel
    hi8561->del(panel);
    ESP_LOGD(TAG, "del hi8561 panel @%p", hi8561);
    free(hi8561);

    return ESP_OK;
}

static esp_err_t panel_hi8561_init(esp_lcd_panel_t *panel)
{
    hi8561_panel_t *hi8561 = (hi8561_panel_t *)panel->user_data;

    ESP_RETURN_ON_ERROR(panel_hi8561_send_init_cmds(hi8561), TAG, "send init commands failed");
    ESP_RETURN_ON_ERROR(hi8561->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_hi8561_reset(esp_lcd_panel_t *panel)
{
    hi8561_panel_t *hi8561 = (hi8561_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = hi8561->io;

    // Perform hardware reset
    if (hi8561->reset_gpio_num >= 0)
    {
        gpio_set_level(static_cast<gpio_num_t>(hi8561->reset_gpio_num), hi8561->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(static_cast<gpio_num_t>(hi8561->reset_gpio_num), !hi8561->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    else if (io)
    { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static esp_err_t panel_hi8561_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    hi8561_panel_t *hi8561 = (hi8561_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = hi8561->io;

    if (sleep == true)
    {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x10, 0x00, 0), TAG, "esp_lcd_panel_io_tx_param fail");
        ESP_LOGI(TAG, "panel_hi8561 sleep on");
    }
    else
    {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x11, 0x00, 0), TAG, "esp_lcd_panel_io_tx_param fail");
        ESP_LOGI(TAG, "panel_hi8561 sleep off");
    }

    vTaskDelay(pdMS_TO_TICKS(120));

    return ESP_OK;
}

static esp_err_t panel_hi8561_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    hi8561_panel_t *hi8561 = (hi8561_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = hi8561->io;

    if (on_off == true)
    {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x29, 0x00, 0), TAG, "esp_lcd_panel_io_tx_param fail");
        ESP_LOGI(TAG, "panel_hi8561 display on");
    }
    else
    {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x28, 0x00, 0), TAG, "esp_lcd_panel_io_tx_param fail");
        ESP_LOGI(TAG, "panel_hi8561 display off");
    }

    vTaskDelay(pdMS_TO_TICKS(120));

    return ESP_OK;
}

static esp_err_t panel_hi8561_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    hi8561_panel_t *hi8561 = (hi8561_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = hi8561->io;
    uint8_t madctl_val = hi8561->madctl_val;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    // Control mirror through LCD command
    if (mirror_x)
    {
        madctl_val |= HI8561_CMD_SHLR_BIT;
    }
    else
    {
        madctl_val &= ~HI8561_CMD_SHLR_BIT;
    }
    if (mirror_y)
    {
        madctl_val |= HI8561_CMD_UPDN_BIT;
    }
    else
    {
        madctl_val &= ~HI8561_CMD_UPDN_BIT;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]){madctl_val}, 1), TAG, "send command failed");
    hi8561->madctl_val = madctl_val;

    return ESP_OK;
}

static esp_err_t panel_hi8561_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    hi8561_panel_t *hi8561 = (hi8561_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = hi8561->io;
    uint8_t command = 0;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    if (invert_color_data)
    {
        command = LCD_CMD_INVON;
    }
    else
    {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");

    return ESP_OK;
}

#endif
