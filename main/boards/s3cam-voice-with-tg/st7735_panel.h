/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Minimal esp_lcd panel driver for the Sitronix ST7735S TFT controller
 * (SPI, 128x160 RGB565), modeled on esp_lcd_panel_st7789.c from ESP-IDF.
 */
#pragma once

#include "esp_lcd_types.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an ST7735S panel handle
 *
 * @param[in] io LCD panel IO handle
 * @param[in] panel_dev_config General panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_ERR_NO_MEM if out of memory
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_new_panel_st7735(const esp_lcd_panel_io_handle_t io,
                                   const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif