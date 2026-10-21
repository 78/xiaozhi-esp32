#ifndef XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_LCD_INIT_CMDS_H_
#define XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_LCD_INIT_CMDS_H_

#include <esp_lcd_jd9365.h>

// Vendor initialisation sequence for the JD9365DA-H3 800x1280 panel used on
// the reTerminal D1001, copied from `vendor_specific_init_default` in the
// Seeed BSP driver esp_lcd_jd9365_8/esp_lcd_jd9365_8.c (the JD9356_8_TEST_PATTERN
// block is deliberately left out).
//
// The registry driver espressif/esp_lcd_jd9365 replaces its own default table
// with this one when vendor_config.init_cmds is set; everything it sends around
// the table (read ID, page select, MADCTL, COLMOD, lane count, then the DPI
// panel init) is identical to the Seeed `_8` driver, so the resulting command
// stream on the wire matches the BSP.
static const jd9365_lcd_init_cmd_t lcd_init_cmds[] = {
    // {cmd, { data }, data_size, delay_ms}
    {0x11, (uint8_t[]){0x00}, 1, 500},
    {0xE0, (uint8_t[]){0x00}, 1, 0},
    {0xE1, (uint8_t[]){0x93}, 1, 0},
    {0xE2, (uint8_t[]){0x65}, 1, 0},
    {0xE3, (uint8_t[]){0xF8}, 1, 0},
    {0x80, (uint8_t[]){0x01}, 1, 0},
    {0xE0, (uint8_t[]){0x00}, 1, 0},
    {0x29, (uint8_t[]){0x00}, 1, 50},
};

#endif  // XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_LCD_INIT_CMDS_H_
