#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ES3C28P (2.8" ESP32-S3 Display) board template.
// Please verify pin mappings against the datasheet and wiring for your module:
// https://www.lcdwiki.com/2.8inch_ESP32-S3_Display#ESP32_Pin_Parameters

// --- I2C bus for Codec and Touch Controller ---
#define BOARD_I2C_SDA_PIN GPIO_NUM_16
#define BOARD_I2C_SCL_PIN GPIO_NUM_15

// --- Audio / Codec (set according to the 2.8" ESP32-S3 module datasheet) ---
#define AUDIO_INPUT_SAMPLE_RATE     16000
#define AUDIO_OUTPUT_SAMPLE_RATE    16000

// I2S pin mapping (update if your wiring differs)
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_4 // Audio I2S master clock (MCLK)
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5 // Audio I2S bit clock (BCLK)
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_7 // Audio I2S LR clock / WS (left/right select)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_6 // Audio I2S data out (DOUT)
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_8 // Audio I2S data in (DIN)

// ES8311 I2C control pins (ES8311 codec over I2C)
#define AUDIO_CODEC_I2C_SDA_PIN  BOARD_I2C_SDA_PIN // I2C bus data signal
#define AUDIO_CODEC_I2C_SCL_PIN  BOARD_I2C_SCL_PIN // I2C bus clock signal
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_1 // Audio output enable (PA enable, low = enable)
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// --- Buttons / LEDs ---
#define BOOT_BUTTON_PIN        GPIO_NUM_0
#define BUILTIN_LED_GPIO       GPIO_NUM_42

// --- Touch Screen (Capacitive) ---
#define TOUCH_I2C_SDA_PIN      GPIO_NUM_16 // I2C bus data signal - shared with AUDIO_CODEC_I2C_SDA_PIN
#define TOUCH_I2C_SCL_PIN      GPIO_NUM_15 // I2C bus clock signal - shared with AUDIO_CODEC_I2C_SCL_PIN
#define TOUCH_RST_PIN          GPIO_NUM_18 // Touch controller reset pin (active low)
#define TOUCH_INT_PIN          GPIO_NUM_17 // Touch controller interrupt pin (active low)

// --- Display SPI pins ---
#define DISPLAY_SPI_LCD_HOST    SPI2_HOST
#define DISPLAY_SPI_CLOCK_HZ    (40 * 1000 * 1000)

#define DISPLAY_SPI_PIN_SCLK    GPIO_NUM_12
#define DISPLAY_SPI_PIN_MOSI    GPIO_NUM_11
#define DISPLAY_SPI_PIN_MISO    GPIO_NUM_13
#define DISPLAY_SPI_PIN_LCD_DC  GPIO_NUM_46
// Reset is shared with main control; don't drive from GPIO unless your board exposes it
#define DISPLAY_SPI_PIN_LCD_RST GPIO_NUM_NC
#define DISPLAY_SPI_PIN_LCD_CS  GPIO_NUM_10

#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_45
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// --- Display geometry / orientation ---
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_SPI_MODE 0

//battery
#define BUILTIN_BATTERY_GPIO GPIO_NUM_9

#endif // _BOARD_CONFIG_H_
