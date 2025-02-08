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

#include "esp_lcd_gc9d01n.h"

static const char *TAG = "gc9d01n";

static esp_err_t panel_gc9d01n_del(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9d01n_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9d01n_init(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9d01n_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_gc9d01n_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_gc9d01n_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_gc9d01n_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_gc9d01n_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_gc9d01n_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct{
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save current value of LCD_CMD_COLMOD register
    const gc9d01n_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} gc9d01n_panel_t;

esp_err_t esp_lcd_new_panel_gc9d01n(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel){
    esp_err_t ret = ESP_OK;
    gc9d01n_panel_t *gc9d01n = NULL;
    gpio_config_t io_conf = {0};

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    gc9d01n = (gc9d01n_panel_t *)calloc(1, sizeof(gc9d01n_panel_t));
    ESP_GOTO_ON_FALSE(gc9d01n, ESP_ERR_NO_MEM, err, TAG, "no mem for gc9d01n panel");

    if (panel_dev_config->reset_gpio_num >= 0){
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num;
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    switch (panel_dev_config->color_space){
    case ESP_LCD_COLOR_SPACE_RGB:
        gc9d01n->madctl_val = 0;
        break;
    case ESP_LCD_COLOR_SPACE_BGR:
        gc9d01n->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }
#else
    switch (panel_dev_config->rgb_endian){
    case LCD_RGB_ENDIAN_RGB:
        gc9d01n->madctl_val = 0;
        break;
    case LCD_RGB_ENDIAN_BGR:
        gc9d01n->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb endian");
        break;
    }
#endif

    switch (panel_dev_config->bits_per_pixel){
    case 16: // RGB565
        gc9d01n->colmod_val = 0x55;
        gc9d01n->fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        gc9d01n->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        gc9d01n->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    gc9d01n->io = io;
    gc9d01n->reset_gpio_num = panel_dev_config->reset_gpio_num;
    gc9d01n->reset_level = panel_dev_config->flags.reset_active_high;
    if (panel_dev_config->vendor_config){
        gc9d01n->init_cmds = ((gc9d01n_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds;
        gc9d01n->init_cmds_size = ((gc9d01n_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds_size;
    }
    gc9d01n->base.del = panel_gc9d01n_del;
    gc9d01n->base.reset = panel_gc9d01n_reset;
    gc9d01n->base.init = panel_gc9d01n_init;
    gc9d01n->base.draw_bitmap = panel_gc9d01n_draw_bitmap;
    gc9d01n->base.invert_color = panel_gc9d01n_invert_color;
    gc9d01n->base.set_gap = panel_gc9d01n_set_gap;
    gc9d01n->base.mirror = panel_gc9d01n_mirror;
    gc9d01n->base.swap_xy = panel_gc9d01n_swap_xy;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    gc9d01n->base.disp_off = panel_gc9d01n_disp_on_off;
#else
    gc9d01n->base.disp_on_off = panel_gc9d01n_disp_on_off;
#endif
    *ret_panel = &(gc9d01n->base);
    ESP_LOGD(TAG, "new gc9d01n panel @%p", gc9d01n);

    // ESP_LOGI(TAG, "LCD panel create success, version: %d.%d.%d", ESP_LCD_GC9D01N_VER_MAJOR, ESP_LCD_GC9D01N_VER_MINOR,
    //          ESP_LCD_GC9D01N_VER_PATCH);

    return ESP_OK;

err:
    if (gc9d01n){
        if (panel_dev_config->reset_gpio_num >= 0){
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(gc9d01n);
    }
    return ret;
}

static esp_err_t panel_gc9d01n_del(esp_lcd_panel_t *panel){
    gc9d01n_panel_t *gc9d01n = __containerof(panel, gc9d01n_panel_t, base);

    if (gc9d01n->reset_gpio_num >= 0){
        gpio_reset_pin(gc9d01n->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del gc9d01n panel @%p", gc9d01n);
    free(gc9d01n);
    return ESP_OK;
}

static esp_err_t panel_gc9d01n_reset(esp_lcd_panel_t *panel){
    gc9d01n_panel_t *gc9d01n = __containerof(panel, gc9d01n_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9d01n->io;

    // perform hardware reset
    if (gc9d01n->reset_gpio_num >= 0){
        gpio_set_level(gc9d01n->reset_gpio_num, gc9d01n->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(gc9d01n->reset_gpio_num, !gc9d01n->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    else{ // perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5ms before sending new command
    }

    return ESP_OK;
}

static const gc9d01n_lcd_init_cmd_t vendor_specific_init_default[] = {
    //  {cmd, { data }, data_size, delay_ms}
    // Enable Inter Register
    {0xFE, (uint8_t[]){0x00}, 0, 0},
    {0xEF, (uint8_t[]){0x00}, 0, 0},
    {0x80, (uint8_t[]){0xFF}, 1, 0},
    {0x81, (uint8_t[]){0xFF}, 1, 0},
    {0x82, (uint8_t[]){0xFF}, 1, 0},
    {0x84, (uint8_t[]){0xFF}, 1, 0},
    {0x85, (uint8_t[]){0xFF}, 1, 0},
    {0x86, (uint8_t[]){0xFF}, 1, 0},
    {0x87, (uint8_t[]){0xFF}, 1, 0},
    {0x88, (uint8_t[]){0xFF}, 1, 0},
    {0x89, (uint8_t[]){0xFF}, 1, 0},
    {0x8A, (uint8_t[]){0xFF}, 1, 0},
    {0x8B, (uint8_t[]){0xFF}, 1, 0},
    {0x8C, (uint8_t[]){0xFF}, 1, 0},
    {0x8D, (uint8_t[]){0xFF}, 1, 0},
    {0x8E, (uint8_t[]){0xFF}, 1, 0},
    {0x8F, (uint8_t[]){0xFF}, 1, 0},
    {0x3A, (uint8_t[]){0x05}, 1, 0},
    {0xEC, (uint8_t[]){0x01}, 1, 0},
    {0x74, (uint8_t[]){0x02, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0x98, (uint8_t[]){0x3E}, 1, 0},
    {0x99, (uint8_t[]){0x3E}, 1, 0},
    {0xB5, (uint8_t[]){0x0D, 0x0D}, 2, 0},
    {0x60, (uint8_t[]){0x38, 0x0F, 0x79, 0x67}, 4, 0},
    {0x61, (uint8_t[]){0x38, 0x11, 0x79, 0x67}, 4, 0},
    {0x64, (uint8_t[]){0x38, 0x17, 0x71, 0x5F, 0x79, 0x67}, 6, 0},
    {0x65, (uint8_t[]){0x38, 0x13, 0x71, 0x5B, 0x79, 0x67}, 6, 0},
    {0x6A, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x6C, (uint8_t[]){0x22, 0x02, 0x22, 0x02, 0x22, 0x22, 0x50}, 7, 0},
    {0x6E, (uint8_t[]){0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0x0F, 0x0F, 0x0D, 0x0D, 0x0B, 0x0B, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x0A, 0x0C, 0x0C, 0x0E, 0x0E, 0x10, 0x10, 0x00, 0x00, 0x02, 0x02, 0x04, 0x04}, 32, 0},
    {0xBF, (uint8_t[]){0x01}, 1, 0},
    {0xF9, (uint8_t[]){0x40}, 1, 0},
    {0x9B, (uint8_t[]){0x3B, 0x93, 0x33, 0x7F, 0x00}, 5, 0},
    {0x7E, (uint8_t[]){0x30}, 1, 0},
    {0x70, (uint8_t[]){0x0D, 0x02, 0x08, 0x0D, 0x02, 0x08}, 6, 0},
    {0x71, (uint8_t[]){0x0D, 0x02, 0x08}, 3, 0},
    {0x91, (uint8_t[]){0x0E, 0x09}, 2, 0},
    {0xC3, (uint8_t[]){0x19, 0xC4, 0x19, 0xC9, 0x3C}, 5, 0},
    {0xF0, (uint8_t[]){0x53, 0x15, 0x0A, 0x04, 0x00, 0x3E}, 6, 0},
    {0xF1, (uint8_t[]){0x56, 0xA8, 0x7F, 0x33, 0x34, 0x5F}, 6, 0},
    {0xF2, (uint8_t[]){0x53, 0x15, 0x0A, 0x04, 0x00, 0x3A}, 6, 0},
    {0xF3, (uint8_t[]){0x52, 0xA4, 0x7F, 0x33, 0x34, 0xDF}, 6, 0},

    // {0x20, (uint8_t[]){0x00}, 0, 0},
    {0x36, (uint8_t[]){0x00}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 0, 200},
    {0x29, (uint8_t[]){0x00}, 0, 0},
    {0x2C, (uint8_t[]){0x00}, 0, 20},
};

static esp_err_t panel_gc9d01n_init(esp_lcd_panel_t *panel){
    gc9d01n_panel_t *gc9d01n = __containerof(panel, gc9d01n_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9d01n->io;

    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "send command failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]){gc9d01n->madctl_val,},1),TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]){gc9d01n->colmod_val,},1),TAG, "send command failed");

    const gc9d01n_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    if (gc9d01n->init_cmds){
        init_cmds = gc9d01n->init_cmds;
        init_cmds_size = gc9d01n->init_cmds_size;
    }else{
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(gc9d01n_lcd_init_cmd_t);
    }

    bool is_cmd_overwritten = false;
    for (int i = 0; i < init_cmds_size; i++){
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd){
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            gc9d01n->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            gc9d01n->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        default:
            is_cmd_overwritten = false;
            break;
        }

        if (is_cmd_overwritten){
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        }

        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_gc9d01n_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data){
    gc9d01n_panel_t *gc9d01n = __containerof(panel, gc9d01n_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = gc9d01n->io;

    x_start += gc9d01n->x_gap;
    x_end += gc9d01n->x_gap;
    y_start += gc9d01n->y_gap;
    y_end += gc9d01n->y_gap;

    // define an area of frame memory where MCU can access
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]){(x_start >> 8) & 0xFF,x_start & 0xFF,((x_end - 1) >> 8) & 0xFF,(x_end - 1) & 0xFF,},4),TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]){(y_start >> 8) & 0xFF,y_start & 0xFF,((y_end - 1) >> 8) & 0xFF,(y_end - 1) & 0xFF,},4),TAG, "send command failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * gc9d01n->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), TAG, "send color failed");

    return ESP_OK;
}

static esp_err_t panel_gc9d01n_invert_color(esp_lcd_panel_t *panel, bool invert_color_data){
    gc9d01n_panel_t *gc9d01n = __containerof(panel, gc9d01n_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9d01n->io;
    int command = 0;
    if (invert_color_data){
        command = LCD_CMD_INVON;
    }else{
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_gc9d01n_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y){
    gc9d01n_panel_t *gc9d01n = __containerof(panel, gc9d01n_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9d01n->io;
    if (mirror_x){
        gc9d01n->madctl_val |= LCD_CMD_MX_BIT;
    }else{
        gc9d01n->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y){
        gc9d01n->madctl_val |= LCD_CMD_MY_BIT;
    }else{
        gc9d01n->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]){gc9d01n->madctl_val}, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_gc9d01n_swap_xy(esp_lcd_panel_t *panel, bool swap_axes){
    gc9d01n_panel_t *gc9d01n = __containerof(panel, gc9d01n_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9d01n->io;
    if (swap_axes){
        gc9d01n->madctl_val |= LCD_CMD_MV_BIT;
    }else{
        gc9d01n->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]){gc9d01n->madctl_val}, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_gc9d01n_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap){
    gc9d01n_panel_t *gc9d01n = __containerof(panel, gc9d01n_panel_t, base);
    gc9d01n->x_gap = x_gap;
    gc9d01n->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_gc9d01n_disp_on_off(esp_lcd_panel_t *panel, bool on_off){
    gc9d01n_panel_t *gc9d01n = __containerof(panel, gc9d01n_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9d01n->io;
    int command = 0;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    on_off = !on_off;
#endif

    if (on_off){
        command = LCD_CMD_DISPON;
    }else{
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
