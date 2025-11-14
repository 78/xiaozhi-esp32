/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file
 * @brief ESP LCD: ST7123
 */

 #pragma once

 #include <stdint.h>
 #include "soc/soc_caps.h"
 
 #if SOC_MIPI_DSI_SUPPORTED
 #include "esp_lcd_panel_vendor.h"
 #include "esp_lcd_mipi_dsi.h"
 
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
 } st7123_lcd_init_cmd_t;
 
 /**
  * @brief LCD panel vendor configuration.
  *
  * @note  This structure needs to be passed to the `esp_lcd_panel_dev_config_t::vendor_config`.
  *
  */
 typedef struct {
     const st7123_lcd_init_cmd_t *init_cmds;       /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                      *   The array should be declared as `static const` and positioned outside the function.
                                                      *   Please refer to `vendor_specific_init_default` in source file.
                                                      */
     uint16_t init_cmds_size;                        /*<! Number of commands in above array */
     struct {
         esp_lcd_dsi_bus_handle_t dsi_bus;               /*!< MIPI-DSI bus configuration */
         const esp_lcd_dpi_panel_config_t *dpi_config;   /*!< MIPI-DPI panel configuration */
         uint8_t  lane_num;                              /*!< Number of MIPI-DSI lanes */
     } mipi_config;
 } st7123_vendor_config_t;
 
 /**
  * @brief Create LCD panel for model ST7123
  *
  * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for initialization sequence code.
  *
  * @param[in]  io LCD panel IO handle
  * @param[in]  panel_dev_config General panel device configuration
  * @param[out] ret_panel Returned LCD panel handle
  * @return
  *      - ESP_ERR_INVALID_ARG   if parameter is invalid
  *      - ESP_OK                on success
  *      - Otherwise             on fail
  */
 esp_err_t esp_lcd_new_panel_st7123(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                      esp_lcd_panel_handle_t *ret_panel);
 
 /**
  * @brief MIPI-DSI bus configuration structure
  *
  */
 #define ST7123_PANEL_BUS_DSI_2CH_CONFIG()              \
     {                                                    \
         .bus_id = 0,                                     \
         .num_data_lanes = 2,                             \
         .lane_bit_rate_mbps = 1000,                      \
     }
 
 /**
  * @brief MIPI-DBI panel IO configuration structure
  *
  */
 #define ST7123_PANEL_IO_DBI_CONFIG()  \
     {                                   \
         .virtual_channel = 0,           \
         .lcd_cmd_bits = 8,              \
         .lcd_param_bits = 8,            \
     }
 
 /**
  * @brief MIPI DPI configuration structure
  *
  * @note  refresh_rate = (dpi_clock_freq_mhz * 1000000) / (h_res + hsync_pulse_width + hsync_back_porch + hsync_front_porch)
  *                                                      / (v_res + vsync_pulse_width + vsync_back_porch + vsync_front_porch)
  *
  * @param[in] px_format Pixel format of the panel
  *
  */
 #define ST7123_800_1280_PANEL_60HZ_DPI_CONFIG(px_format) \
     {                                                      \
         .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,       \
         .dpi_clock_freq_mhz = 80,                          \
         .virtual_channel = 0,                              \
         .pixel_format = px_format,                         \
         .num_fbs = 1,                                      \
         .video_timing = {                                  \
             .h_size = 800,                                 \
             .v_size = 1280,                                \
             .hsync_back_porch = 140,                       \
             .hsync_pulse_width = 40,                       \
             .hsync_front_porch = 40,                       \
             .vsync_back_porch = 16,                        \
             .vsync_pulse_width = 4,                        \
             .vsync_front_porch = 16,                       \
         },                                                 \
         .flags.use_dma2d = true,                           \
     }
 #endif
 
 #ifdef __cplusplus
 }
 #endif
 