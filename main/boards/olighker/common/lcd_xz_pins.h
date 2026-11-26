#pragma once

#define DISPLAY_USE_ST7789
#include "xz_pins.h"

// The LCD and microSD card use independent SPI hosts and pin groups.
#define LCD_SPI_HOST SPI3_HOST
#define LCD_SPI_SCK_GPIO GPIO_NUM_12
#define LCD_SPI_MOSI_GPIO GPIO_NUM_11
#define LCD_SPI_CS_GPIO GPIO_NUM_17
#define LCD_DC_GPIO GPIO_NUM_21
#define LCD_RESET_GPIO GPIO_NUM_38
#define LCD_BACKLIGHT_GPIO GPIO_NUM_47
#define LCD_SPI_CLOCK_HZ 18000000
#define LCD_SPI_MODE 3
#define LCD_BACKLIGHT_INVERT false
