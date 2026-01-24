#include "display_manager.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_st77916.h>
#include <esp_err.h>
#include <freertos/task.h>
#include <cstring>

static const char *TAG = "FogSeekDisplayManager";

// 沃乐康1.8英寸屏幕初始化命令(WLK 1.8 inch)
static const st77916_lcd_init_cmd_t lcd_init_cmds_wlk_1_8_inch[] = {
    {0xF0, (uint8_t[]){0x28}, 1, 0},
    {0xF2, (uint8_t[]){0x28}, 1, 0},
    {0x73, (uint8_t[]){0xF0}, 1, 0},
    {0x7C, (uint8_t[]){0xD1}, 1, 0},
    {0x83, (uint8_t[]){0xE0}, 1, 0},
    {0x84, (uint8_t[]){0x61}, 1, 0},
    {0xF2, (uint8_t[]){0x82}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x01}, 1, 0},
    {0xF1, (uint8_t[]){0x01}, 1, 0},
    {0xB0, (uint8_t[]){0x5E}, 1, 0},
    {0xB1, (uint8_t[]){0x55}, 1, 0},
    {0xB2, (uint8_t[]){0x24}, 1, 0},
    {0xB3, (uint8_t[]){0x01}, 1, 0},
    {0xB4, (uint8_t[]){0x87}, 1, 0},
    {0xB5, (uint8_t[]){0x44}, 1, 0},
    {0xB6, (uint8_t[]){0x8B}, 1, 0},
    {0xB7, (uint8_t[]){0x40}, 1, 0},
    {0xB8, (uint8_t[]){0x86}, 1, 0},
    {0xB9, (uint8_t[]){0x15}, 1, 0},
    {0xBA, (uint8_t[]){0x00}, 1, 0},
    {0xBB, (uint8_t[]){0x08}, 1, 0},
    {0xBC, (uint8_t[]){0x08}, 1, 0},
    {0xBD, (uint8_t[]){0x00}, 1, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x07}, 1, 0},
    {0xC0, (uint8_t[]){0x80}, 1, 0},
    {0xC1, (uint8_t[]){0x10}, 1, 0},
    {0xC2, (uint8_t[]){0x37}, 1, 0},
    {0xC3, (uint8_t[]){0x80}, 1, 0},
    {0xC4, (uint8_t[]){0x10}, 1, 0},
    {0xC5, (uint8_t[]){0x37}, 1, 0},
    {0xC6, (uint8_t[]){0xA9}, 1, 0},
    {0xC7, (uint8_t[]){0x41}, 1, 0},
    {0xC8, (uint8_t[]){0x01}, 1, 0},
    {0xC9, (uint8_t[]){0xA9}, 1, 0},
    {0xCA, (uint8_t[]){0x41}, 1, 0},
    {0xCB, (uint8_t[]){0x01}, 1, 0},
    {0xCC, (uint8_t[]){0x7F}, 1, 0},
    {0xCD, (uint8_t[]){0x7F}, 1, 0},
    {0xCE, (uint8_t[]){0xFF}, 1, 0},
    {0xD0, (uint8_t[]){0x91}, 1, 0},
    {0xD1, (uint8_t[]){0x68}, 1, 0},
    {0xD2, (uint8_t[]){0x68}, 1, 0},
    {0xF5, (uint8_t[]){0x00, 0xA5}, 2, 0},
    {0xDD, (uint8_t[]){0x40}, 1, 0},
    {0xDE, (uint8_t[]){0x40}, 1, 0},
    {0xF1, (uint8_t[]){0x10}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x02}, 1, 0},
    {0xE0, (uint8_t[]){0xF0, 0x10, 0x18, 0x0D, 0x0C, 0x38, 0x3E, 0x44, 0x51, 0x39, 0x15, 0x15, 0x30, 0x34}, 14, 0},
    {0xE1, (uint8_t[]){0xF0, 0x0F, 0x17, 0x0D, 0x0B, 0x07, 0x3E, 0x33, 0x51, 0x39, 0x15, 0x15, 0x30, 0x34}, 14, 0},
    {0xF0, (uint8_t[]){0x10}, 1, 0},
    {0xF3, (uint8_t[]){0x10}, 1, 0},
    {0xE0, (uint8_t[]){0x08}, 1, 0},
    {0xE1, (uint8_t[]){0x00}, 1, 0},
    {0xE2, (uint8_t[]){0x00}, 1, 0},
    {0xE3, (uint8_t[]){0x00}, 1, 0},
    {0xE4, (uint8_t[]){0xE0}, 1, 0},
    {0xE5, (uint8_t[]){0x06}, 1, 0},
    {0xE6, (uint8_t[]){0x21}, 1, 0},
    {0xE7, (uint8_t[]){0x03}, 1, 0},
    {0xE8, (uint8_t[]){0x05}, 1, 0},
    {0xE9, (uint8_t[]){0x02}, 1, 0},
    {0xEA, (uint8_t[]){0xE9}, 1, 0},
    {0xEB, (uint8_t[]){0x00}, 1, 0},
    {0xEC, (uint8_t[]){0x00}, 1, 0},
    {0xED, (uint8_t[]){0x14}, 1, 0},
    {0xEE, (uint8_t[]){0xFF}, 1, 0},
    {0xEF, (uint8_t[]){0x00}, 1, 0},
    {0xF8, (uint8_t[]){0xFF}, 1, 0},
    {0xF9, (uint8_t[]){0x00}, 1, 0},
    {0xFA, (uint8_t[]){0x00}, 1, 0},
    {0xFB, (uint8_t[]){0x30}, 1, 0},
    {0xFC, (uint8_t[]){0x00}, 1, 0},
    {0xFD, (uint8_t[]){0x00}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x00}, 1, 0},
    {0x60, (uint8_t[]){0x40}, 1, 0},
    {0x61, (uint8_t[]){0x05}, 1, 0},
    {0x62, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0x42}, 1, 0},
    {0x64, (uint8_t[]){0xDA}, 1, 0},
    {0x65, (uint8_t[]){0x00}, 1, 0},
    {0x66, (uint8_t[]){0x00}, 1, 0},
    {0x67, (uint8_t[]){0x00}, 1, 0},
    {0x68, (uint8_t[]){0x00}, 1, 0},
    {0x69, (uint8_t[]){0x00}, 1, 0},
    {0x6A, (uint8_t[]){0x00}, 1, 0},
    {0x6B, (uint8_t[]){0x00}, 1, 0},
    {0x70, (uint8_t[]){0x40}, 1, 0},
    {0x71, (uint8_t[]){0x04}, 1, 0},
    {0x72, (uint8_t[]){0x00}, 1, 0},
    {0x73, (uint8_t[]){0x42}, 1, 0},
    {0x74, (uint8_t[]){0xD9}, 1, 0},
    {0x75, (uint8_t[]){0x00}, 1, 0},
    {0x76, (uint8_t[]){0x00}, 1, 0},
    {0x77, (uint8_t[]){0x00}, 1, 0},
    {0x78, (uint8_t[]){0x00}, 1, 0},
    {0x79, (uint8_t[]){0x00}, 1, 0},
    {0x7A, (uint8_t[]){0x00}, 1, 0},
    {0x7B, (uint8_t[]){0x00}, 1, 0},
    {0x80, (uint8_t[]){0x48}, 1, 0},
    {0x81, (uint8_t[]){0x00}, 1, 0},
    {0x82, (uint8_t[]){0x07}, 1, 0},
    {0x83, (uint8_t[]){0x02}, 1, 0},
    {0x84, (uint8_t[]){0xD7}, 1, 0},
    {0x85, (uint8_t[]){0x04}, 1, 0},
    {0x86, (uint8_t[]){0x00}, 1, 0},
    {0x87, (uint8_t[]){0x00}, 1, 0},
    {0x88, (uint8_t[]){0x48}, 1, 0},
    {0x89, (uint8_t[]){0x00}, 1, 0},
    {0x8A, (uint8_t[]){0x09}, 1, 0},
    {0x8B, (uint8_t[]){0x02}, 1, 0},
    {0x8C, (uint8_t[]){0xD9}, 1, 0},
    {0x8D, (uint8_t[]){0x04}, 1, 0},
    {0x8E, (uint8_t[]){0x00}, 1, 0},
    {0x8F, (uint8_t[]){0x00}, 1, 0},
    {0x90, (uint8_t[]){0x48}, 1, 0},
    {0x91, (uint8_t[]){0x00}, 1, 0},
    {0x92, (uint8_t[]){0x0B}, 1, 0},
    {0x93, (uint8_t[]){0x02}, 1, 0},
    {0x94, (uint8_t[]){0xDB}, 1, 0},
    {0x95, (uint8_t[]){0x04}, 1, 0},
    {0x96, (uint8_t[]){0x00}, 1, 0},
    {0x97, (uint8_t[]){0x00}, 1, 0},
    {0x98, (uint8_t[]){0x48}, 1, 0},
    {0x99, (uint8_t[]){0x00}, 1, 0},
    {0x9A, (uint8_t[]){0x0D}, 1, 0},
    {0x9B, (uint8_t[]){0x02}, 1, 0},
    {0x9C, (uint8_t[]){0xDD}, 1, 0},
    {0x9D, (uint8_t[]){0x04}, 1, 0},
    {0x9E, (uint8_t[]){0x00}, 1, 0},
    {0x9F, (uint8_t[]){0x00}, 1, 0},
    {0xA0, (uint8_t[]){0x48}, 1, 0},
    {0xA1, (uint8_t[]){0x00}, 1, 0},
    {0xA2, (uint8_t[]){0x06}, 1, 0},
    {0xA3, (uint8_t[]){0x02}, 1, 0},
    {0xA4, (uint8_t[]){0xD6}, 1, 0},
    {0xA5, (uint8_t[]){0x04}, 1, 0},
    {0xA6, (uint8_t[]){0x00}, 1, 0},
    {0xA7, (uint8_t[]){0x00}, 1, 0},
    {0xA8, (uint8_t[]){0x48}, 1, 0},
    {0xA9, (uint8_t[]){0x00}, 1, 0},
    {0xAA, (uint8_t[]){0x08}, 1, 0},
    {0xAB, (uint8_t[]){0x02}, 1, 0},
    {0xAC, (uint8_t[]){0xD8}, 1, 0},
    {0xAD, (uint8_t[]){0x04}, 1, 0},
    {0xAE, (uint8_t[]){0x00}, 1, 0},
    {0xAF, (uint8_t[]){0x00}, 1, 0},
    {0xB0, (uint8_t[]){0x48}, 1, 0},
    {0xB1, (uint8_t[]){0x00}, 1, 0},
    {0xB2, (uint8_t[]){0x0A}, 1, 0},
    {0xB3, (uint8_t[]){0x02}, 1, 0},
    {0xB4, (uint8_t[]){0xDA}, 1, 0},
    {0xB5, (uint8_t[]){0x04}, 1, 0},
    {0xB6, (uint8_t[]){0x00}, 1, 0},
    {0xB7, (uint8_t[]){0x00}, 1, 0},
    {0xB8, (uint8_t[]){0x48}, 1, 0},
    {0xB9, (uint8_t[]){0x00}, 1, 0},
    {0xBA, (uint8_t[]){0x0C}, 1, 0},
    {0xBB, (uint8_t[]){0x02}, 1, 0},
    {0xBC, (uint8_t[]){0xDC}, 1, 0},
    {0xBD, (uint8_t[]){0x04}, 1, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x00}, 1, 0},
    {0xC0, (uint8_t[]){0x10}, 1, 0},
    {0xC1, (uint8_t[]){0x47}, 1, 0},
    {0xC2, (uint8_t[]){0x56}, 1, 0},
    {0xC3, (uint8_t[]){0x65}, 1, 0},
    {0xC4, (uint8_t[]){0x74}, 1, 0},
    {0xC5, (uint8_t[]){0x88}, 1, 0},
    {0xC6, (uint8_t[]){0x99}, 1, 0},
    {0xC7, (uint8_t[]){0x01}, 1, 0},
    {0xC8, (uint8_t[]){0xBB}, 1, 0},
    {0xC9, (uint8_t[]){0xAA}, 1, 0},
    {0xD0, (uint8_t[]){0x10}, 1, 0},
    {0xD1, (uint8_t[]){0x47}, 1, 0},
    {0xD2, (uint8_t[]){0x56}, 1, 0},
    {0xD3, (uint8_t[]){0x65}, 1, 0},
    {0xD4, (uint8_t[]){0x74}, 1, 0},
    {0xD5, (uint8_t[]){0x88}, 1, 0},
    {0xD6, (uint8_t[]){0x99}, 1, 0},
    {0xD7, (uint8_t[]){0x01}, 1, 0},
    {0xD8, (uint8_t[]){0xBB}, 1, 0},
    {0xD9, (uint8_t[]){0xAA}, 1, 0},
    {0xF3, (uint8_t[]){0x01}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x21, (uint8_t[]){0x00}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 1, 120},
    {0x29, (uint8_t[]){0x00}, 1, 0},
};

// 金逸晨1.5英寸屏幕初始化命令(JYC 1.5 inch)
static const st77916_lcd_init_cmd_t lcd_init_cmds_jyc_1_5_inch[] = {
    // Initial setup
    {0xF0, (uint8_t[]){0x28}, 1, 0},
    {0xF2, (uint8_t[]){0x28}, 1, 0},
    {0x73, (uint8_t[]){0xF0}, 1, 0},
    {0x7C, (uint8_t[]){0xD1}, 1, 0},
    {0x83, (uint8_t[]){0xE0}, 1, 0},
    {0x84, (uint8_t[]){0x61}, 1, 0},
    {0xF2, (uint8_t[]){0x82}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x01}, 1, 0},
    {0xF1, (uint8_t[]){0x01}, 1, 0},

    // Power settings
    {0xB0, (uint8_t[]){0x69}, 1, 0},
    {0xB1, (uint8_t[]){0x4A}, 1, 0},
    {0xB2, (uint8_t[]){0x2F}, 1, 0},
    {0xB3, (uint8_t[]){0x01}, 1, 0},
    {0xB4, (uint8_t[]){0x69}, 1, 0},
    {0xB5, (uint8_t[]){0x45}, 1, 0},
    {0xB6, (uint8_t[]){0xAB}, 1, 0},
    {0xB7, (uint8_t[]){0x41}, 1, 0},
    {0xB8, (uint8_t[]){0x86}, 1, 0},
    {0xB9, (uint8_t[]){0x15}, 1, 0},
    {0xBA, (uint8_t[]){0x00}, 1, 0},
    {0xBB, (uint8_t[]){0x08}, 1, 0},
    {0xBC, (uint8_t[]){0x08}, 1, 0},
    {0xBD, (uint8_t[]){0x00}, 1, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x07}, 1, 0},

    // More power settings
    {0xC0, (uint8_t[]){0x80}, 1, 0},
    {0xC1, (uint8_t[]){0x10}, 1, 0},
    {0xC2, (uint8_t[]){0x37}, 1, 0},
    {0xC3, (uint8_t[]){0x80}, 1, 0},
    {0xC4, (uint8_t[]){0x10}, 1, 0},
    {0xC5, (uint8_t[]){0x37}, 1, 0},
    {0xC6, (uint8_t[]){0xA9}, 1, 0}, // Fixed: removed extra field
    {0xC7, (uint8_t[]){0x41}, 1, 0},
    {0xC8, (uint8_t[]){0x01}, 1, 0},
    {0xC9, (uint8_t[]){0xA9}, 1, 0},
    {0xCA, (uint8_t[]){0x41}, 1, 0},
    {0xCB, (uint8_t[]){0x01}, 1, 0},
    {0xCC, (uint8_t[]){0x7F}, 1, 0},
    {0xCD, (uint8_t[]){0x7F}, 1, 0},
    {0xCE, (uint8_t[]){0xFF}, 1, 0},
    {0xD0, (uint8_t[]){0x91}, 1, 0},
    {0xD1, (uint8_t[]){0x68}, 1, 0},
    {0xD2, (uint8_t[]){0x68}, 1, 0},
    {0xF5, (uint8_t[]){0x00, 0xA5}, 2, 0},
    {0xF1, (uint8_t[]){0x10}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x02}, 1, 0},

    // Gamma settings
    {0xE0, (uint8_t[]){0xF0, 0x0A, 0x10, 0x09, 0x09, 0x36, 0x35, 0x33, 0x4A, 0x29, 0x15, 0x15, 0x2E, 0x34}, 14, 0},
    {0xE1, (uint8_t[]){0xF0, 0x0A, 0x0F, 0x08, 0x08, 0x05, 0x34, 0x33, 0x4A, 0x39, 0x15, 0x15, 0x2D, 0x33}, 14, 0},

    // More settings
    {0xF0, (uint8_t[]){0x10}, 1, 0},
    {0xF3, (uint8_t[]){0x10}, 1, 0},
    {0xE0, (uint8_t[]){0x07}, 1, 0},
    {0xE1, (uint8_t[]){0x00}, 1, 0},
    {0xE2, (uint8_t[]){0x00}, 1, 0},
    {0xE3, (uint8_t[]){0x00}, 1, 0},
    {0xE4, (uint8_t[]){0xE0}, 1, 0},
    {0xE5, (uint8_t[]){0x06}, 1, 0},
    {0xE6, (uint8_t[]){0x21}, 1, 0},
    {0xE7, (uint8_t[]){0x01}, 1, 0},
    {0xE8, (uint8_t[]){0x05}, 1, 0},
    {0xE9, (uint8_t[]){0x02}, 1, 0},
    {0xEA, (uint8_t[]){0xDA}, 1, 0},
    {0xEB, (uint8_t[]){0x00}, 1, 0},
    {0xEC, (uint8_t[]){0x00}, 1, 0},
    {0xED, (uint8_t[]){0x0F}, 1, 0},
    {0xEE, (uint8_t[]){0x00}, 1, 0},
    {0xEF, (uint8_t[]){0x00}, 1, 0},
    {0xF8, (uint8_t[]){0x00}, 1, 0},
    {0xF9, (uint8_t[]){0x00}, 1, 0},
    {0xFA, (uint8_t[]){0x00}, 1, 0},
    {0xFB, (uint8_t[]){0x00}, 1, 0},
    {0xFC, (uint8_t[]){0x00}, 1, 0},
    {0xFD, (uint8_t[]){0x00}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x00}, 1, 0},

    // Display settings
    {0x60, (uint8_t[]){0x42}, 1, 0},
    {0x61, (uint8_t[]){0xE0}, 1, 0},
    {0x62, (uint8_t[]){0x40}, 1, 0},
    {0x63, (uint8_t[]){0x40}, 1, 0},
    {0x64, (uint8_t[]){0xDA}, 1, 0},
    {0x65, (uint8_t[]){0x00}, 1, 0},
    {0x66, (uint8_t[]){0x00}, 1, 0},
    {0x67, (uint8_t[]){0x00}, 1, 0},
    {0x68, (uint8_t[]){0x00}, 1, 0},
    {0x69, (uint8_t[]){0x00}, 1, 0},
    {0x6A, (uint8_t[]){0x00}, 1, 0},
    {0x6B, (uint8_t[]){0x00}, 1, 0},
    {0x70, (uint8_t[]){0x42}, 1, 0},
    {0x71, (uint8_t[]){0xE4}, 1, 0},
    {0x72, (uint8_t[]){0x40}, 1, 0},
    {0x73, (uint8_t[]){0x40}, 1, 0},
    {0x74, (uint8_t[]){0xD9}, 1, 0},
    {0x75, (uint8_t[]){0x00}, 1, 0},
    {0x76, (uint8_t[]){0x00}, 1, 0},
    {0x77, (uint8_t[]){0x00}, 1, 0},
    {0x78, (uint8_t[]){0x00}, 1, 0},
    {0x79, (uint8_t[]){0x00}, 1, 0},
    {0x7A, (uint8_t[]){0x00}, 1, 0},
    {0x7B, (uint8_t[]){0x00}, 1, 0},

    // Final display settings
    {0x3A, (uint8_t[]){0x55}, 1, 0},   // Pixel format
    {0x21, (uint8_t[]){0x00}, 1, 0},   // Display inversion
    {0x11, (uint8_t[]){0x00}, 1, 120}, // Exit sleep mode, delay 120ms
    {0x29, (uint8_t[]){0x00}, 1, 0},   // Display on
};

// 华夏彩1.8英寸屏幕初始化命令(HXC 1.8 inch)
static const st77916_lcd_init_cmd_t lcd_init_cmds_hxc_1_8_inch[] = {
    {0xF0, (uint8_t[]){0x28}, 1, 0},
    {0xF2, (uint8_t[]){0x28}, 1, 0},
    {0x73, (uint8_t[]){0xF0}, 1, 0},
    {0x7C, (uint8_t[]){0xD1}, 1, 0},
    {0x83, (uint8_t[]){0xE0}, 1, 0},
    {0x84, (uint8_t[]){0x61}, 1, 0},
    {0xF2, (uint8_t[]){0x82}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x01}, 1, 0},
    {0xF1, (uint8_t[]){0x01}, 1, 0},
    {0xB0, (uint8_t[]){0x56}, 1, 0},
    {0xB1, (uint8_t[]){0x4D}, 1, 0},
    {0xB2, (uint8_t[]){0x24}, 1, 0},
    {0xB4, (uint8_t[]){0x87}, 1, 0},
    {0xB5, (uint8_t[]){0x44}, 1, 0},
    {0xB6, (uint8_t[]){0x8B}, 1, 0},
    {0xB7, (uint8_t[]){0x40}, 1, 0},
    {0xB8, (uint8_t[]){0x86}, 1, 0},
    {0xBA, (uint8_t[]){0x00}, 1, 0},
    {0xBB, (uint8_t[]){0x08}, 1, 0},
    {0xBC, (uint8_t[]){0x08}, 1, 0},
    {0xBD, (uint8_t[]){0x00}, 1, 0},
    {0xC0, (uint8_t[]){0x80}, 1, 0},
    {0xC1, (uint8_t[]){0x10}, 1, 0},
    {0xC2, (uint8_t[]){0x37}, 1, 0},
    {0xC3, (uint8_t[]){0x80}, 1, 0},
    {0xC4, (uint8_t[]){0x10}, 1, 0},
    {0xC5, (uint8_t[]){0x37}, 1, 0},
    {0xC6, (uint8_t[]){0xA9}, 1, 0},
    {0xC7, (uint8_t[]){0x41}, 1, 0},
    {0xC8, (uint8_t[]){0x01}, 1, 0},
    {0xC9, (uint8_t[]){0xA9}, 1, 0},
    {0xCA, (uint8_t[]){0x41}, 1, 0},
    {0xCB, (uint8_t[]){0x01}, 1, 0},
    {0xD0, (uint8_t[]){0x91}, 1, 0},
    {0xD1, (uint8_t[]){0x68}, 1, 0},
    {0xD2, (uint8_t[]){0x68}, 1, 0},
    {0xF5, (uint8_t[]){0x00, 0xA5}, 2, 0},
    {0xDD, (uint8_t[]){0x4F}, 1, 0},
    {0xDE, (uint8_t[]){0x4F}, 1, 0},
    {0xF1, (uint8_t[]){0x10}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x02}, 1, 0},
    {0xE0, (uint8_t[]){0xF0, 0x0A, 0x10, 0x09, 0x09, 0x36, 0x35, 0x33, 0x4A, 0x29, 0x15, 0x15, 0x2D, 0x33}, 14, 0},
    {0xE1, (uint8_t[]){0xF0, 0x0A, 0x0F, 0x08, 0x08, 0x05, 0x34, 0x33, 0x4A, 0x39, 0x15, 0x15, 0x2D, 0x33}, 14, 0},
    {0xF0, (uint8_t[]){0x10}, 1, 0},
    {0xF3, (uint8_t[]){0x10}, 1, 0},
    {0xE0, (uint8_t[]){0x07}, 1, 0},
    {0xE1, (uint8_t[]){0x00}, 1, 0},
    {0xE2, (uint8_t[]){0x00}, 1, 0},
    {0xE3, (uint8_t[]){0x00}, 1, 0},
    {0xE4, (uint8_t[]){0xE0}, 1, 0},
    {0xE5, (uint8_t[]){0x06}, 1, 0},
    {0xE6, (uint8_t[]){0x21}, 1, 0},
    {0xE7, (uint8_t[]){0x01}, 1, 0},
    {0xE8, (uint8_t[]){0x05}, 1, 0},
    {0xE9, (uint8_t[]){0x02}, 1, 0},
    {0xEA, (uint8_t[]){0xDA}, 1, 0},
    {0xEB, (uint8_t[]){0x00}, 1, 0},
    {0xEC, (uint8_t[]){0x00}, 1, 0},
    {0xED, (uint8_t[]){0x0F}, 1, 0},
    {0xEE, (uint8_t[]){0x00}, 1, 0},
    {0xEF, (uint8_t[]){0x00}, 1, 0},
    {0xF8, (uint8_t[]){0x00}, 1, 0},
    {0xF9, (uint8_t[]){0x00}, 1, 0},
    {0xFA, (uint8_t[]){0x00}, 1, 0},
    {0xFB, (uint8_t[]){0x00}, 1, 0},
    {0xFC, (uint8_t[]){0x00}, 1, 0},
    {0xFD, (uint8_t[]){0x00}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x00}, 1, 0},
    {0x60, (uint8_t[]){0x40}, 1, 0},
    {0x61, (uint8_t[]){0x04}, 1, 0},
    {0x62, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0x42}, 1, 0},
    {0x64, (uint8_t[]){0xD9}, 1, 0},
    {0x65, (uint8_t[]){0x00}, 1, 0},
    {0x66, (uint8_t[]){0x00}, 1, 0},
    {0x67, (uint8_t[]){0x00}, 1, 0},
    {0x68, (uint8_t[]){0x00}, 1, 0},
    {0x69, (uint8_t[]){0x00}, 1, 0},
    {0x6A, (uint8_t[]){0x00}, 1, 0},
    {0x6B, (uint8_t[]){0x00}, 1, 0},
    {0x70, (uint8_t[]){0x40}, 1, 0},
    {0x71, (uint8_t[]){0x03}, 1, 0},
    {0x72, (uint8_t[]){0x00}, 1, 0},
    {0x73, (uint8_t[]){0x42}, 1, 0},
    {0x74, (uint8_t[]){0xD8}, 1, 0},
    {0x75, (uint8_t[]){0x00}, 1, 0},
    {0x76, (uint8_t[]){0x00}, 1, 0},
    {0x77, (uint8_t[]){0x00}, 1, 0},
    {0x78, (uint8_t[]){0x00}, 1, 0},
    {0x79, (uint8_t[]){0x00}, 1, 0},
    {0x7A, (uint8_t[]){0x00}, 1, 0},
    {0x7B, (uint8_t[]){0x00}, 1, 0},
    {0x80, (uint8_t[]){0x48}, 1, 0},
    {0x81, (uint8_t[]){0x00}, 1, 0},
    {0x82, (uint8_t[]){0x06}, 1, 0},
    {0x83, (uint8_t[]){0x02}, 1, 0},
    {0x84, (uint8_t[]){0xD6}, 1, 0},
    {0x85, (uint8_t[]){0x04}, 1, 0},
    {0x86, (uint8_t[]){0x00}, 1, 0},
    {0x87, (uint8_t[]){0x00}, 1, 0},
    {0x88, (uint8_t[]){0x48}, 1, 0},
    {0x89, (uint8_t[]){0x00}, 1, 0},
    {0x8A, (uint8_t[]){0x08}, 1, 0},
    {0x8B, (uint8_t[]){0x02}, 1, 0},
    {0x8C, (uint8_t[]){0xD8}, 1, 0},
    {0x8D, (uint8_t[]){0x04}, 1, 0},
    {0x8E, (uint8_t[]){0x00}, 1, 0},
    {0x8F, (uint8_t[]){0x00}, 1, 0},
    {0x90, (uint8_t[]){0x48}, 1, 0},
    {0x91, (uint8_t[]){0x00}, 1, 0},
    {0x92, (uint8_t[]){0x0A}, 1, 0},
    {0x93, (uint8_t[]){0x02}, 1, 0},
    {0x94, (uint8_t[]){0xDA}, 1, 0},
    {0x95, (uint8_t[]){0x04}, 1, 0},
    {0x96, (uint8_t[]){0x00}, 1, 0},
    {0x97, (uint8_t[]){0x00}, 1, 0},
    {0x98, (uint8_t[]){0x48}, 1, 0},
    {0x99, (uint8_t[]){0x00}, 1, 0},
    {0x9A, (uint8_t[]){0x0C}, 1, 0},
    {0x9B, (uint8_t[]){0x02}, 1, 0},
    {0x9C, (uint8_t[]){0xDC}, 1, 0},
    {0x9D, (uint8_t[]){0x04}, 1, 0},
    {0x9E, (uint8_t[]){0x00}, 1, 0},
    {0x9F, (uint8_t[]){0x00}, 1, 0},
    {0xA0, (uint8_t[]){0x48}, 1, 0},
    {0xA1, (uint8_t[]){0x00}, 1, 0},
    {0xA2, (uint8_t[]){0x05}, 1, 0},
    {0xA3, (uint8_t[]){0x02}, 1, 0},
    {0xA4, (uint8_t[]){0xD5}, 1, 0},
    {0xA5, (uint8_t[]){0x04}, 1, 0},
    {0xA6, (uint8_t[]){0x00}, 1, 0},
    {0xA7, (uint8_t[]){0x00}, 1, 0},
    {0xA8, (uint8_t[]){0x48}, 1, 0},
    {0xA9, (uint8_t[]){0x00}, 1, 0},
    {0xAA, (uint8_t[]){0x07}, 1, 0},
    {0xAB, (uint8_t[]){0x02}, 1, 0},
    {0xAC, (uint8_t[]){0xD7}, 1, 0},
    {0xAD, (uint8_t[]){0x04}, 1, 0},
    {0xAE, (uint8_t[]){0x00}, 1, 0},
    {0xAF, (uint8_t[]){0x00}, 1, 0},
    {0xB0, (uint8_t[]){0x48}, 1, 0},
    {0xB1, (uint8_t[]){0x00}, 1, 0},
    {0xB2, (uint8_t[]){0x09}, 1, 0},
    {0xB3, (uint8_t[]){0x02}, 1, 0},
    {0xB4, (uint8_t[]){0xD9}, 1, 0},
    {0xB5, (uint8_t[]){0x04}, 1, 0},
    {0xB6, (uint8_t[]){0x00}, 1, 0},
    {0xB7, (uint8_t[]){0x00}, 1, 0},
    {0xB8, (uint8_t[]){0x48}, 1, 0},
    {0xB9, (uint8_t[]){0x00}, 1, 0},
    {0xBA, (uint8_t[]){0x0B}, 1, 0},
    {0xBB, (uint8_t[]){0x02}, 1, 0},
    {0xBC, (uint8_t[]){0xDB}, 1, 0},
    {0xBD, (uint8_t[]){0x04}, 1, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x00}, 1, 0},
    {0xC0, (uint8_t[]){0x10}, 1, 0},
    {0xC1, (uint8_t[]){0x47}, 1, 0},
    {0xC2, (uint8_t[]){0x56}, 1, 0},
    {0xC3, (uint8_t[]){0x65}, 1, 0},
    {0xC4, (uint8_t[]){0x74}, 1, 0},
    {0xC5, (uint8_t[]){0x88}, 1, 0},
    {0xC6, (uint8_t[]){0x99}, 1, 0},
    {0xC7, (uint8_t[]){0x01}, 1, 0},
    {0xC8, (uint8_t[]){0xBB}, 1, 0},
    {0xC9, (uint8_t[]){0xAA}, 1, 0},
    {0xD0, (uint8_t[]){0x10}, 1, 0},
    {0xD1, (uint8_t[]){0x47}, 1, 0},
    {0xD2, (uint8_t[]){0x56}, 1, 0},
    {0xD3, (uint8_t[]){0x65}, 1, 0},
    {0xD4, (uint8_t[]){0x74}, 1, 0},
    {0xD5, (uint8_t[]){0x88}, 1, 0},
    {0xD6, (uint8_t[]){0x99}, 1, 0},
    {0xD7, (uint8_t[]){0x01}, 1, 0},
    {0xD8, (uint8_t[]){0xBB}, 1, 0},
    {0xD9, (uint8_t[]){0xAA}, 1, 0},
    {0xF3, (uint8_t[]){0x01}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0x21, (uint8_t[]){0x00}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 1, 120},
    {0x29, (uint8_t[]){0x00}, 1, 0},
};

// 配置表
static const lcd_config_item_t wlk_1_8_config = {
    .comm_type = COMM_QSPI,
    .driver_type = DRIVER_ST77916,
    .init_cmds = lcd_init_cmds_wlk_1_8_inch,
    .init_cmds_size = sizeof(lcd_init_cmds_wlk_1_8_inch) / sizeof(st77916_lcd_init_cmd_t)};

static const lcd_config_item_t jyc_1_5_config = {
    .comm_type = COMM_QSPI,
    .driver_type = DRIVER_ST77916,
    .init_cmds = lcd_init_cmds_jyc_1_5_inch,
    .init_cmds_size = sizeof(lcd_init_cmds_jyc_1_5_inch) / sizeof(st77916_lcd_init_cmd_t)};

static const lcd_config_item_t hxc_1_8_config = {
    .comm_type = COMM_QSPI,
    .driver_type = DRIVER_ST77916,
    .init_cmds = lcd_init_cmds_hxc_1_8_inch,
    .init_cmds_size = sizeof(lcd_init_cmds_hxc_1_8_inch) / sizeof(st77916_lcd_init_cmd_t)};

static const lcd_config_item_t hxc_1_15_config = {
    .comm_type = COMM_SPI,
    .driver_type = DRIVER_ST7789,
    .init_cmds = NULL, // 使用标准库默认初始化
    .init_cmds_size = 0};

// 通信接口和驱动接口的实现类定义
class SpiCommunicationInterface : public ICommunicationInterface
{
public:
    bool Initialize(esp_lcd_panel_io_handle_t *panel_io, const lcd_pin_config_t *pin_config) override
    {
        // 配置SPI接口
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = (gpio_num_t)pin_config->spi_mosi_gpio;
        buscfg.miso_io_num = (gpio_num_t)(-1); // ST7789不需要MISO
        buscfg.sclk_io_num = (gpio_num_t)pin_config->spi_sclk_gpio;
        buscfg.data0_io_num = (gpio_num_t)(-1);
        buscfg.data1_io_num = (gpio_num_t)(-1);
        buscfg.data2_io_num = (gpio_num_t)(-1);
        buscfg.data3_io_num = (gpio_num_t)(-1);
        buscfg.data4_io_num = (gpio_num_t)(-1);
        buscfg.data5_io_num = (gpio_num_t)(-1);
        buscfg.data6_io_num = (gpio_num_t)(-1);
        buscfg.data7_io_num = (gpio_num_t)(-1);
        buscfg.max_transfer_sz = 0;
        buscfg.flags = 0; // 设置为0，不再使用结构体形式访问
        buscfg.intr_flags = 0;

        esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "SPI init failed");
            return false;
        }

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = (gpio_num_t)pin_config->spi_cs_gpio;
        io_config.dc_gpio_num = (gpio_num_t)pin_config->st7789_dc_gpio;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000; // 40MHz
        io_config.trans_queue_depth = 10;
        io_config.on_color_trans_done = nullptr;
        io_config.user_ctx = nullptr;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        io_config.flags.dc_low_on_data = 0;
        io_config.flags.octal_mode = 0;
        io_config.flags.lsb_first = 0;
        io_config.flags.cs_high_active = 0;

        ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, panel_io);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Panel IO init failed");
            return false;
        }

        ESP_LOGI(TAG, "SPI communication interface initialized successfully");
        return true;
    }
};

// QSPI通信接口实现
class QspiCommunicationInterface : public ICommunicationInterface
{
public:
    bool Initialize(esp_lcd_panel_io_handle_t *panel_io, const lcd_pin_config_t *pin_config) override
    {
        // 配置IM0和IM2引脚用于QSPI模式选择（仅沃乐康屏幕需要）
        if (pin_config->qspi_d0_gpio >= 0 && pin_config->qspi_d1_gpio >= 0)
        {
            gpio_config_t io_conf = {};
            io_conf.pin_bit_mask = (1ULL << pin_config->qspi_d0_gpio) | (1ULL << pin_config->qspi_d1_gpio);
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.intr_type = GPIO_INTR_DISABLE;

            esp_err_t ret = gpio_config(&io_conf);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "GPIO config failed");
                return false;
            }

            // 设置IM0=1, IM2=0选择QSPI模式
            gpio_set_level((gpio_num_t)pin_config->qspi_d0_gpio, 1);
            gpio_set_level((gpio_num_t)pin_config->qspi_d1_gpio, 0);
        }

        // 使用标准ESP-IDF配置初始化SPI总线
        spi_bus_config_t bus_config = {};
        bus_config.data0_io_num = (gpio_num_t)pin_config->qspi_d0_gpio;
        bus_config.data1_io_num = (gpio_num_t)pin_config->qspi_d1_gpio;
        bus_config.sclk_io_num = (gpio_num_t)pin_config->qspi_sclk_gpio;
        bus_config.data2_io_num = (gpio_num_t)pin_config->qspi_d2_gpio;
        bus_config.data3_io_num = (gpio_num_t)pin_config->qspi_d3_gpio;
        bus_config.data4_io_num = (gpio_num_t)(-1);
        bus_config.data5_io_num = (gpio_num_t)(-1);
        bus_config.data6_io_num = (gpio_num_t)(-1);
        bus_config.data7_io_num = (gpio_num_t)(-1);
        bus_config.max_transfer_sz = 4096;
        bus_config.flags = 0; // 设置为0，不再使用结构体形式访问
        bus_config.intr_flags = 0;

        esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "SPI init failed");
            return false;
        }

        // 使用ESP-IDF标准宏配置QSPI接口IO
        esp_lcd_panel_io_spi_config_t io_config = ST77916_PANEL_IO_QSPI_CONFIG(
            (gpio_num_t)pin_config->qspi_cs_gpio,
            NULL,
            NULL);

        ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, panel_io);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Panel IO init failed");
            return false;
        }

        ESP_LOGI(TAG, "QSPI communication interface initialized successfully");
        return true;
    }
};

// ST7789驱动实现
class St7789DisplayDriver : public IDisplayDriver
{
public:
    bool Initialize(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t *panel, const lcd_pin_config_t *pin_config, const lcd_config_item_t &config) override
    {
        // 创建ST7789面板
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = (gpio_num_t)pin_config->st7789_reset_gpio;
        panel_config.color_space = ESP_LCD_COLOR_SPACE_BGR;
        panel_config.bits_per_pixel = 16;
        panel_config.flags.reset_active_high = 0;
        panel_config.vendor_config = NULL;

        esp_err_t ret = esp_lcd_new_panel_st7789(panel_io, &panel_config, panel);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Panel creation failed");
            return false;
        }

        // 如果提供了自定义初始化命令，则发送它们
        if (config.init_cmds != NULL && config.init_cmds_size > 0)
        {
            // ST7789使用标准初始化，这里跳过自定义命令
        }

        // 初始化面板
        ret = esp_lcd_panel_init(*panel);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Panel init failed");
            return false;
        }

        // 设置旋转方向（如果需要）
        ret = esp_lcd_panel_swap_xy(*panel, false);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Swap XY failed");
            return false;
        }
        ret = esp_lcd_panel_mirror(*panel, true, false);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Mirror failed");
            return false;
        }

        ESP_LOGI(TAG, "ST7789 driver initialized successfully");
        return true;
    }
};

// ST77916驱动实现
class St77916DisplayDriver : public IDisplayDriver
{
private:
    lcd_type_t lcd_type_;

public:
    St77916DisplayDriver(lcd_type_t lcd_type) : lcd_type_(lcd_type) {}

    bool Initialize(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t *panel, const lcd_pin_config_t *pin_config, const lcd_config_item_t &config) override
    {
        st77916_vendor_config_t vendor_config = {};
        vendor_config.init_cmds = static_cast<const st77916_lcd_init_cmd_t *>(config.init_cmds);
        vendor_config.init_cmds_size = config.init_cmds_size;
        vendor_config.flags.use_qspi_interface = 1;

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = (gpio_num_t)pin_config->st77916_reset_gpio;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;

        // 创建ST77916面板
        esp_err_t ret = esp_lcd_new_panel_st77916(panel_io, &panel_config, panel);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Panel creation failed");
            return false;
        }

        // 重置并初始化面板
        ret = esp_lcd_panel_reset(*panel);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Panel reset failed");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 增加延时
        ret = esp_lcd_panel_init(*panel);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Panel init failed");
            return false;
        }

        // 开启显示
        ret = esp_lcd_panel_disp_on_off(*panel, true);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Enable display failed");
            return false;
        }

        ESP_LOGI(TAG, "ST77916 driver initialized successfully");
        return true;
    }
};

// 构造函数
FogSeekDisplayManager::FogSeekDisplayManager() : panel_io_(nullptr),
                                                 panel_(nullptr),
                                                 backlight_(nullptr),
                                                 display_(nullptr)
{
}

// 析构函数 - 清理资源
FogSeekDisplayManager::~FogSeekDisplayManager()
{
    // 清理显示对象
    if (display_)
    {
        delete display_;
        display_ = nullptr;
    }

    // 清理面板资源
    if (panel_)
    {
        esp_lcd_panel_del(panel_);
        panel_ = nullptr;
    }

    if (panel_io_)
    {
        esp_lcd_panel_io_del(panel_io_);
        panel_io_ = nullptr;
    }

    // 背光资源由 unique_ptr 自动管理
}

const lcd_config_item_t *FogSeekDisplayManager::GetLcdConfig(lcd_type_t lcd_type)
{
    switch (lcd_type)
    {
    case DISPLAY_TYPE_WLK_1_8_INCH:
        return &wlk_1_8_config;
    case DISPLAY_TYPE_JYC_1_5_INCH:
        return &jyc_1_5_config;
    case DISPLAY_TYPE_HXC_1_8_INCH:
        return &hxc_1_8_config;
    case DISPLAY_TYPE_HXC_1_15_INCH:
        return &hxc_1_15_config;
    default:
        ESP_LOGE(TAG, "Unsupported LCD type: %d", lcd_type);
        return nullptr;
    }
}

std::unique_ptr<ICommunicationInterface> FogSeekDisplayManager::CreateCommunicationInterface(comm_type_t comm_type)
{
    switch (comm_type)
    {
    case COMM_SPI:
        return std::make_unique<SpiCommunicationInterface>();
    case COMM_QSPI:
        return std::make_unique<QspiCommunicationInterface>();
    default:
        return nullptr;
    }
}

std::unique_ptr<IDisplayDriver> FogSeekDisplayManager::CreateDisplayDriver(driver_type_t driver_type, lcd_type_t lcd_type)
{
    switch (driver_type)
    {
    case DRIVER_ST7789:
        return std::make_unique<St7789DisplayDriver>();
    case DRIVER_ST77916:
        return std::make_unique<St77916DisplayDriver>(lcd_type);
    default:
        return nullptr;
    }
}

void FogSeekDisplayManager::Initialize(lcd_type_t lcd_type, const lcd_pin_config_t *pin_config)
{
    // 获取屏幕配置
    const lcd_config_item_t *config = GetLcdConfig(lcd_type);
    if (!config)
    {
        ESP_LOGE(TAG, "Invalid LCD type: %d", lcd_type);
        return;
    }

    // 创建通信接口
    auto comm_interface = CreateCommunicationInterface(config->comm_type);
    if (!comm_interface)
    {
        ESP_LOGE(TAG, "Failed to create communication interface for LCD type: %d", lcd_type);
        return;
    }

    // 创建驱动接口
    auto driver = CreateDisplayDriver(config->driver_type, lcd_type);
    if (!driver)
    {
        ESP_LOGE(TAG, "Failed to create display driver for LCD type: %d", lcd_type);
        return;
    }

    // 初始化通信接口
    if (!comm_interface->Initialize(&panel_io_, pin_config))
    {
        ESP_LOGE(TAG, "Failed to initialize communication interface");
        return;
    }

    // 初始化驱动
    if (!driver->Initialize(panel_io_, &panel_, pin_config, *config))
    {
        ESP_LOGE(TAG, "Failed to initialize display driver");
        return;
    }

    // 通用初始化流程
    if (!InitializeCommonComponents(pin_config))
    {
        ESP_LOGE(TAG, "Failed to initialize common components");
        return;
    }
}

bool FogSeekDisplayManager::InitializeCommonComponents(const lcd_pin_config_t *pin_config)
{
    // 9. 初始化背光
    backlight_ = std::make_unique<PwmBacklight>((gpio_num_t)pin_config->st7789_bl_gpio, true);
    if (backlight_)
    {
        backlight_->SetBrightness(0);
    }

    // 10. 创建SPI LCD显示对象
    display_ = new (std::nothrow) SpiLcdDisplay(
        panel_io_,
        panel_,
        pin_config->width,
        pin_config->height,
        pin_config->offset_x,
        pin_config->offset_y,
        pin_config->mirror_x,
        pin_config->mirror_y,
        pin_config->swap_xy);

    if (!display_)
    {
        ESP_LOGE(TAG, "Failed to create display object");
        return false;
    }

    // 11. 延时以确保显示和LVGL完全初始化
    vTaskDelay(pdMS_TO_TICKS(200));
    return true;
}

// 设置显示亮度
void FogSeekDisplayManager::SetBrightness(int percent)
{
    if (backlight_)
    {
        backlight_->SetBrightness(percent);
    }
}

// 恢复显示亮度
void FogSeekDisplayManager::RestoreBrightness()
{
    if (backlight_)
    {
        backlight_->RestoreBrightness();
    }
}

// 设置状态文本
void FogSeekDisplayManager::SetStatus(const char *status)
{
    if (display_)
    {
        display_->SetStatus(status);
    }
}

// 设置聊天消息
void FogSeekDisplayManager::SetChatMessage(const char *sender, const char *message)
{
    if (display_)
    {
        display_->SetChatMessage(sender, message);
    }
}

// 处理设备状态变更
void FogSeekDisplayManager::HandleDeviceState(DeviceState current_state)
{
    if (!display_)
        return;

    switch (current_state)
    {
    case kDeviceStateIdle:
        display_->SetStatus("空闲");
        display_->SetChatMessage("system", "等待唤醒...");
        break;

    case kDeviceStateListening:
        display_->SetStatus("监听中");
        display_->SetChatMessage("system", "正在聆听...");
        break;

    case kDeviceStateSpeaking:
        display_->SetStatus("回答中");
        display_->SetChatMessage("system", "正在回答...");
        break;

    default:
        ESP_LOGW(TAG, "Unknown device state: %d", static_cast<int>(current_state));
        break;
    }
}