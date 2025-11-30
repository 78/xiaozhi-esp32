/*
 * @Description: hi8561_driver
 * @Author: LILYGO_L
 * @Date: 2025-06-13 11:02:44
 * @LastEditTime: 2025-10-09 10:36:27
 * @License: GPL 3.0
 */
#pragma once

#include <stdint.h>
#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_mipi_dsi.h"

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct
{
    int cmd;               /*<! The specific LCD command */
    const void *data;      /*<! Buffer that holds the command specific data */
    size_t data_bytes;     /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms; /*<! Delay in milliseconds after this command */
} hi8561_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct
{
    const hi8561_lcd_init_cmd_t *init_cmds; /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                             *   The array should be declared as `static const` and positioned outside the function.
                                             *   Please refer to `vendor_specific_init_default` in source file.
                                             */
    uint16_t init_cmds_size;                /*<! Number of commands in above array */
    struct
    {
        esp_lcd_dsi_bus_handle_t dsi_bus;             /*!< MIPI-DSI bus configuration */
        const esp_lcd_dpi_panel_config_t *dpi_config; /*!< MIPI-DPI panel configuration */
        uint8_t lane_num;                             /*!< Number of MIPI-DSI lanes, defaults to 2 if set to 0 */
    } mipi_config;
} hi8561_vendor_config_t;

/**
 * @brief Create LCD panel for model HI8561
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
esp_err_t esp_lcd_new_panel_hi8561(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel);

#endif
