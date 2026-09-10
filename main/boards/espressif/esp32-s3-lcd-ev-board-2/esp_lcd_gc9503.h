/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file
 * @brief ESP LCD: GC9503
 */

#pragma once

#include <stdint.h>

#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_rgb.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct {
    int cmd;                /*<! The specific LCD command */
    const void *data;       /*<! Buffer that holds the command specific data */
    size_t data_bytes;      /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms;  /*<! Delay in milliseconds after this command */
} gc9503_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct {
    const esp_lcd_rgb_panel_config_t *rgb_config;   /*!< RGB panel configuration */
    const gc9503_lcd_init_cmd_t *init_cmds;         /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                     *   The array should be declared as `static const` and positioned outside the function.
                                                     *   Please refer to `vendor_specific_init_default` in source file.
                                                     */
    uint16_t init_cmds_size;                        /*<! Number of commands in above array */
    struct {
        unsigned int mirror_by_cmd: 1;              /*<! The `mirror()` function will be implemented by LCD command if set to 1.
                                                     *   Otherwise, the function will be implemented by software.
                                                     */
        unsigned int auto_del_panel_io: 1;          /*<! Delete the panel IO instance automatically if set to 1. All `*_by_cmd` flags will be invalid.
                                                     *   If the panel IO pins are sharing other pins of the RGB interface to save GPIOs,
                                                     *   Please set it to 1 to release the panel IO and its pins (except CS signal).
                                                     */
    } flags;
} gc9503_vendor_config_t;

/**
 * @brief Create LCD panel for model GC9503
 *
 * @note  When `auto_del_panel_io` is set to 1, this function will first initialize the GC9503 with vendor specific initialization and then calls `esp_lcd_new_rgb_panel()` to create an RGB LCD panel. And the `esp_lcd_panel_init()` function will only initialize RGB.
 * @note  When `auto_del_panel_io` is set to 0, this function will only call `esp_lcd_new_rgb_panel()` to create an RGB LCD panel. And the `esp_lcd_panel_init()` function will initialize both the GC9503 and RGB.
 * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for initialization sequence code.
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config General panel device configuration (`vendor_config` and `rgb_config` are necessary)
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_ERR_INVALID_ARG   if parameter is invalid
 *      - ESP_OK                on success
 *      - Otherwise             on fail
 */
esp_err_t esp_lcd_new_panel_gc9503(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief 3-wire SPI panel IO configuration structure
 *
 * @param[in] line_cfg SPI line configuration
 * @param[in] scl_active_edge SCL signal active edge, 0: rising edge, 1: falling edge
 *
 */
#define GC9503_PANEL_IO_3WIRE_SPI_CONFIG(line_cfg, scl_active_edge) \
    {                                                               \
        .line_config = line_cfg,                                    \
        .expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX,             \
        .spi_mode = scl_active_edge ? 1 : 0,                        \
        .lcd_cmd_bytes = 1,                                         \
        .lcd_param_bytes = 1,                                       \
        .flags = {                                                  \
            .use_dc_bit = 1,                                        \
            .dc_zero_on_data = 0,                                   \
            .lsb_first = 0,                                         \
            .cs_high_active = 0,                                    \
            .del_keep_cs_inactive = 1,                              \
        },                                                          \
    }

/**
 * @brief RGB timing structure
 *
 * @note  refresh_rate = (pclk_hz * data_width) / (h_res + hsync_pulse_width + hsync_back_porch + hsync_front_porch)
 *                                              / (v_res + vsync_pulse_width + vsync_back_porch + vsync_front_porch)
 *                                              / bits_per_pixel
 *        .pclk_hz = 16 * 1000 * 1000,                \
        .h_res = 384,                               \
        .v_res = 960,                               \
        .hsync_pulse_width = 24,                    \
        .hsync_back_porch = 20,                     \
        .hsync_front_porch = 40,                    \
        .vsync_pulse_width = 30,                    \
        .vsync_back_porch = 18,                     \
        .vsync_front_porch = 20,                    \
        .flags.pclk_active_neg = 0,                 \

                .hsync_pulse_width = 24,                    \
        .hsync_back_porch = 20,                     \
        .hsync_front_porch = 40,                    \
        .vsync_pulse_width = 30,                    \
        .vsync_back_porch = 18,                     \
        .vsync_front_porch = 20,                    \

 */

 #define GC9503_800_480_PANEL_60HZ_RGB_TIMING()      \
 {                                               \
     .pclk_hz = 16 * 1000 * 1000,                \
     .h_res = 800,                               \
     .v_res = 480,                               \
     .hsync_pulse_width = 10,                    \
     .hsync_back_porch = 10,                     \
     .hsync_front_porch = 20,                    \
     .vsync_pulse_width = 10,                    \
     .vsync_back_porch = 10,                     \
     .vsync_front_porch = 10,                    \
     .flags = {                                  \
        .hsync_idle_low = 0,                    \
        .vsync_idle_low = 0,                    \
        .de_idle_high = 0,                      \
        .pclk_active_neg = 0,                   \
        .pclk_idle_high = 0,                    \
    },                                          \
 }

#ifdef __cplusplus
}
#endif
