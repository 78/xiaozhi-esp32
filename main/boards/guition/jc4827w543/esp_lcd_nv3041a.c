#include "esp_lcd_nv3041a.h"

#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_lcd_panel_interface.h"

#define TAG "nv3041a"

// QSPI command framing. See the header for why the pixel opcode is 0x3C.
#define NV3041A_CMD(cmd) ((0x02u << 24) | ((uint32_t)(cmd) << 8))
#define NV3041A_PIXELS ((0x32u << 24) | (0x3Cu << 8))

#define NV3041A_SLPOUT 0x11
#define NV3041A_INVOFF 0x20
#define NV3041A_INVON 0x21
#define NV3041A_DISPOFF 0x28
#define NV3041A_DISPON 0x29
#define NV3041A_CASET 0x2A
#define NV3041A_RASET 0x2B
#define NV3041A_RAMWR 0x2C
#define NV3041A_MADCTL 0x36

#define NV3041A_MADCTL_MY 0x80
#define NV3041A_MADCTL_MX 0x40
#define NV3041A_MADCTL_MV 0x20

// Rotation 0 on this panel. Mirroring is expressed as a delta from here.
#define NV3041A_MADCTL_BASE (NV3041A_MADCTL_MX | NV3041A_MADCTL_MY)

typedef struct {
    uint8_t cmd;
    uint8_t data;
    uint16_t delay_ms;
} nv3041a_init_cmd_t;

// Vendor init sequence, byte for byte from Arduino_GFX's Arduino_NV3041A.
// 0xFF 0xA5 unlocks the register set and 0xFF 0x00 locks it again; 0x3A 0x01
// and 0x41 0x03 together select RGB565 over a 16-bit bus.
static const nv3041a_init_cmd_t nv3041a_init_cmds[] = {
    {0xff, 0xa5, 0},
    {0x36, NV3041A_MADCTL_BASE, 0},
    {0x3a, 0x01, 0},  // 0x01 = 565, 0x00 = 666
    {0x41, 0x03, 0},  // 0x01 = 8bit, 0x03 = 16bit
    {0x44, 0x15, 0},  // VBP
    {0x45, 0x15, 0},  // VFP
    {0x7d, 0x03, 0},  // vdds_trim
    {0xc1, 0xab, 0},  // avdd/avcl clamp
    {0xc2, 0x17, 0},
    {0xc3, 0x10, 0},
    {0xc6, 0x3a, 0},
    {0xc7, 0x25, 0},
    {0xc8, 0x11, 0},
    {0x7a, 0x49, 0},  // user_vgsp
    {0x6f, 0x2f, 0},  // user_gvdd
    {0x78, 0x4b, 0},  // user_gvcl
    {0xc9, 0x00, 0},
    {0x67, 0x33, 0},
    // gate
    {0x51, 0x4b, 0},
    {0x52, 0x7c, 0},
    {0x53, 0x1c, 0},
    {0x54, 0x77, 0},
    // source
    {0x46, 0x0a, 0},
    {0x47, 0x2a, 0},
    {0x48, 0x0a, 0},
    {0x49, 0x1a, 0},
    {0x56, 0x43, 0},
    {0x57, 0x42, 0},
    {0x58, 0x3c, 0},
    {0x59, 0x64, 0},
    {0x5a, 0x41, 0},
    {0x5b, 0x3c, 0},
    {0x5c, 0x02, 0},
    {0x5d, 0x3c, 0},
    {0x5e, 0x1f, 0},
    {0x60, 0x80, 0},
    {0x61, 0x3f, 0},
    {0x62, 0x21, 0},
    {0x63, 0x07, 0},
    {0x64, 0xe0, 0},
    {0x65, 0x01, 0},
    {0xca, 0x20, 0},
    {0xcb, 0x52, 0},
    {0xcc, 0x10, 0},
    {0xcd, 0x42, 0},
    {0xd0, 0x20, 0},
    {0xd1, 0x52, 0},
    {0xd2, 0x10, 0},
    {0xd3, 0x42, 0},
    {0xd4, 0x0a, 0},
    {0xd5, 0x32, 0},
    // gamma
    {0x80, 0x04, 0},
    {0xa0, 0x00, 0},
    {0x81, 0x07, 0},
    {0xa1, 0x05, 0},
    {0x82, 0x06, 0},
    {0xa2, 0x04, 0},
    {0x86, 0x2c, 0},
    {0xa6, 0x2a, 0},
    {0x87, 0x46, 0},
    {0xa7, 0x44, 0},
    {0x83, 0x39, 0},
    {0xa3, 0x39, 0},
    {0x84, 0x3a, 0},
    {0xa4, 0x3a, 0},
    {0x85, 0x3f, 0},
    {0xa5, 0x3f, 0},
    {0x88, 0x08, 0},
    {0xa8, 0x08, 0},
    {0x89, 0x0f, 0},
    {0xa9, 0x0f, 0},
    {0x8a, 0x17, 0},
    {0xaa, 0x17, 0},
    {0x8b, 0x10, 0},
    {0xab, 0x10, 0},
    {0x8c, 0x16, 0},
    {0xac, 0x16, 0},
    {0x8d, 0x14, 0},
    {0xad, 0x14, 0},
    {0x8e, 0x11, 0},
    {0xae, 0x11, 0},
    {0x8f, 0x14, 0},
    {0xaf, 0x14, 0},
    {0x90, 0x06, 0},
    {0xb0, 0x06, 0},
    {0x91, 0x0f, 0},
    {0xb1, 0x0f, 0},
    {0x92, 0x16, 0},
    {0xb2, 0x16, 0},
    {0xff, 0x00, 0},
    {NV3041A_SLPOUT, 0x00, 120},
    {NV3041A_DISPON, 0x00, 100},
};

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t madctl;
} nv3041a_panel_t;

static esp_err_t nv3041a_tx_cmd(nv3041a_panel_t* panel, uint8_t cmd, uint8_t data) {
    return esp_lcd_panel_io_tx_param(panel->io, NV3041A_CMD(cmd), &data, 1);
}

static esp_err_t panel_nv3041a_del(esp_lcd_panel_t* panel) {
    nv3041a_panel_t* nv = __containerof(panel, nv3041a_panel_t, base);
    if (nv->reset_gpio_num >= 0) {
        gpio_reset_pin((gpio_num_t)nv->reset_gpio_num);
    }
    free(nv);
    return ESP_OK;
}

static esp_err_t panel_nv3041a_reset(esp_lcd_panel_t* panel) {
    nv3041a_panel_t* nv = __containerof(panel, nv3041a_panel_t, base);

    // On the JC4827W543 the panel's reset is not wired to the ESP32, so this
    // is usually a no-op and the init sequence does the unlocking. Keep the
    // GPIO path for boards that do wire it.
    if (nv->reset_gpio_num >= 0) {
        gpio_set_level((gpio_num_t)nv->reset_gpio_num, nv->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)nv->reset_gpio_num, !nv->reset_level);
    }
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t panel_nv3041a_init(esp_lcd_panel_t* panel) {
    nv3041a_panel_t* nv = __containerof(panel, nv3041a_panel_t, base);

    for (size_t i = 0; i < sizeof(nv3041a_init_cmds) / sizeof(nv3041a_init_cmds[0]); i++) {
        const nv3041a_init_cmd_t* cmd = &nv3041a_init_cmds[i];
        ESP_RETURN_ON_ERROR(nv3041a_tx_cmd(nv, cmd->cmd, cmd->data), TAG,
                            "init command 0x%02x failed", cmd->cmd);
        if (cmd->delay_ms) {
            vTaskDelay(pdMS_TO_TICKS(cmd->delay_ms));
        }
    }

    // The table wrote the base MADCTL; re-send whatever mirror/swap state was
    // requested before init.
    return nv3041a_tx_cmd(nv, NV3041A_MADCTL, nv->madctl);
}

static esp_err_t panel_nv3041a_draw_bitmap(esp_lcd_panel_t* panel, int x_start, int y_start,
                                           int x_end, int y_end, const void* color_data) {
    nv3041a_panel_t* nv = __containerof(panel, nv3041a_panel_t, base);
    ESP_RETURN_ON_FALSE(x_start < x_end && y_start < y_end, ESP_ERR_INVALID_ARG, TAG,
                        "start position must be smaller than end position");

    x_start += nv->x_gap;
    x_end += nv->x_gap;
    y_start += nv->y_gap;
    y_end += nv->y_gap;

    const uint8_t caset[4] = {
        (uint8_t)(x_start >> 8),
        (uint8_t)x_start,
        (uint8_t)((x_end - 1) >> 8),
        (uint8_t)(x_end - 1),
    };
    const uint8_t raset[4] = {
        (uint8_t)(y_start >> 8),
        (uint8_t)y_start,
        (uint8_t)((y_end - 1) >> 8),
        (uint8_t)(y_end - 1),
    };

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(nv->io, NV3041A_CMD(NV3041A_CASET), caset, 4),
                        TAG, "CASET failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(nv->io, NV3041A_CMD(NV3041A_RASET), raset, 4),
                        TAG, "RASET failed");
    // RAMWR goes out as a bare command; the pixels then ride the 0x3C
    // write-memory-continue opcode. Splitting it this way is what the vendor
    // driver does and what this panel answers to.
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(nv->io, NV3041A_CMD(NV3041A_RAMWR), NULL, 0), TAG,
                        "RAMWR failed");

    size_t len = (size_t)(x_end - x_start) * (size_t)(y_end - y_start) * 2;
    return esp_lcd_panel_io_tx_color(nv->io, NV3041A_PIXELS, color_data, len);
}

static esp_err_t panel_nv3041a_mirror(esp_lcd_panel_t* panel, bool mirror_x, bool mirror_y) {
    nv3041a_panel_t* nv = __containerof(panel, nv3041a_panel_t, base);

    uint8_t madctl = nv->madctl;
    // Base already has MX and MY set, so a requested mirror clears the bit.
    if (mirror_x) {
        madctl &= ~NV3041A_MADCTL_MX;
    } else {
        madctl |= NV3041A_MADCTL_MX;
    }
    if (mirror_y) {
        madctl &= ~NV3041A_MADCTL_MY;
    } else {
        madctl |= NV3041A_MADCTL_MY;
    }
    nv->madctl = madctl;
    return nv3041a_tx_cmd(nv, NV3041A_MADCTL, madctl);
}

static esp_err_t panel_nv3041a_swap_xy(esp_lcd_panel_t* panel, bool swap_axes) {
    nv3041a_panel_t* nv = __containerof(panel, nv3041a_panel_t, base);

    if (swap_axes) {
        // The panel accepts MV, but only 0 and 180 degrees are reliable on this
        // glass - 90/270 tear and shift. Refuse rather than paint garbage.
        ESP_LOGE(TAG, "swap_xy is not supported on NV3041A (only 0/180 rotation)");
        return ESP_ERR_NOT_SUPPORTED;
    }
    nv->madctl &= ~NV3041A_MADCTL_MV;
    return nv3041a_tx_cmd(nv, NV3041A_MADCTL, nv->madctl);
}

static esp_err_t panel_nv3041a_set_gap(esp_lcd_panel_t* panel, int x_gap, int y_gap) {
    nv3041a_panel_t* nv = __containerof(panel, nv3041a_panel_t, base);
    nv->x_gap = x_gap;
    nv->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_nv3041a_invert_color(esp_lcd_panel_t* panel, bool invert) {
    nv3041a_panel_t* nv = __containerof(panel, nv3041a_panel_t, base);
    uint8_t cmd = invert ? NV3041A_INVON : NV3041A_INVOFF;
    return esp_lcd_panel_io_tx_param(nv->io, NV3041A_CMD(cmd), NULL, 0);
}

static esp_err_t panel_nv3041a_disp_on_off(esp_lcd_panel_t* panel, bool on) {
    nv3041a_panel_t* nv = __containerof(panel, nv3041a_panel_t, base);
    uint8_t cmd = on ? NV3041A_DISPON : NV3041A_DISPOFF;
    return esp_lcd_panel_io_tx_param(nv->io, NV3041A_CMD(cmd), NULL, 0);
}

esp_err_t esp_lcd_new_panel_nv3041a(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t* panel_config,
                                    esp_lcd_panel_handle_t* ret_panel) {
    ESP_RETURN_ON_FALSE(io && panel_config && ret_panel, ESP_ERR_INVALID_ARG, TAG,
                        "invalid argument");
    ESP_RETURN_ON_FALSE(panel_config->bits_per_pixel == 16, ESP_ERR_NOT_SUPPORTED, TAG,
                        "only RGB565 is supported");

    nv3041a_panel_t* nv = calloc(1, sizeof(nv3041a_panel_t));
    ESP_RETURN_ON_FALSE(nv, ESP_ERR_NO_MEM, TAG, "no mem for panel");

    esp_err_t ret = ESP_OK;
    if (panel_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << panel_config->reset_gpio_num,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "reset gpio config failed");
    }

    nv->io = io;
    nv->reset_gpio_num = panel_config->reset_gpio_num;
    nv->reset_level = panel_config->flags.reset_active_high;
    nv->madctl = NV3041A_MADCTL_BASE;

    nv->base.del = panel_nv3041a_del;
    nv->base.reset = panel_nv3041a_reset;
    nv->base.init = panel_nv3041a_init;
    nv->base.draw_bitmap = panel_nv3041a_draw_bitmap;
    nv->base.mirror = panel_nv3041a_mirror;
    nv->base.swap_xy = panel_nv3041a_swap_xy;
    nv->base.set_gap = panel_nv3041a_set_gap;
    nv->base.invert_color = panel_nv3041a_invert_color;
    nv->base.disp_on_off = panel_nv3041a_disp_on_off;

    *ret_panel = &nv->base;
    return ESP_OK;

err:
    free(nv);
    return ret;
}
