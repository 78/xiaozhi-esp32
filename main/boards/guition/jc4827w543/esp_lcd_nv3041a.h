/*
 * NV3041A panel driver for esp_lcd, QSPI only.
 *
 * The NV3041A has no driver in the component registry that speaks QSPI, so this
 * is a board-local one. The command framing follows the usual QSPI-LCD
 * convention, which the panel IO layer expects to receive pre-packed in a
 * 32-bit command word:
 *
 *   parameters : 0x02 << 24 | cmd << 8   command and address on one line
 *   pixels     : 0x32 << 24 | 0x3C << 8  command and address on one line,
 *                                        payload on four
 *
 * The init table and the 0x3C ("write memory continue") framing are taken from
 * Arduino_GFX's Arduino_NV3041A, which is what the vendor demo for this panel
 * ships with and what has been seen working on the board.
 */
#pragma once

#include <esp_err.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an NV3041A panel on an already-created QSPI panel IO.
 *
 * The panel IO must be configured with lcd_cmd_bits = 32, lcd_param_bits = 8,
 * dc_gpio_num = GPIO_NUM_NC and flags.quad_mode = true.
 *
 * Only panel_config->reset_gpio_num and ->bits_per_pixel (16) are used;
 * rgb_ele_order is fixed to RGB by the init sequence.
 */
esp_err_t esp_lcd_new_panel_nv3041a(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t* panel_config,
                                    esp_lcd_panel_handle_t* ret_panel);

#ifdef __cplusplus
}
#endif
