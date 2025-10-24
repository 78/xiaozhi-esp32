#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>

#define AUDIO_INPUT_REFERENCE    true
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
// This matches DogePet's working configuration
#define AUDIO_I2S_METHOD_DUPLEX

#ifdef AUDIO_I2S_METHOD_DUPLEX

// I2S pins for MAX98357 (speaker) and INMP441 (microphone)
// Adjust these to match your wiring on the ESP32-C3 SuperMini
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_8
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_5   // To MAX98357 DIN
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_7   // From INMP441 DOUT

// Built-in LED and boot button (adjust if your board differs)
#define BUILTIN_LED_GPIO   GPIO_NUM_2
#define BOOT_BUTTON_GPIO   GPIO_NUM_0

// TFT LCD 1.69" ST7789 240x280 over SPI
// SPI pins for the LCD display
#define DISPLAY_SPI_MODE        3
#define DISPLAY_CS_PIN          GPIO_NUM_10
#define DISPLAY_MOSI_PIN        GPIO_NUM_3
#define DISPLAY_MISO_PIN        GPIO_NUM_NC
#define DISPLAY_CLK_PIN         GPIO_NUM_4
#define DISPLAY_DC_PIN          GPIO_NUM_9
#define DISPLAY_RST_PIN         GPIO_NUM_NC

// Display geometry and orientation
#define DISPLAY_WIDTH           240
#define DISPLAY_HEIGHT          280
#define DISPLAY_SWAP_XY         false
#define DISPLAY_MIRROR_X        false
#define DISPLAY_MIRROR_Y        false
#define DISPLAY_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_INVERT_COLOR    true

// Panel memory window offsets (typical for 1.69" ST7789 modules)
#define DISPLAY_OFFSET_X        0
#define DISPLAY_OFFSET_Y        20

// Backlight control (set to NC if your module lacks a BL pin)
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_1
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#endif // AUDIO_I2S_METHOD_DUPLEX

#endif // _BOARD_CONFIG_H_