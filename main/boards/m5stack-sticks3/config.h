#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2S0 pins
#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_18
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_17
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_15
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_14
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_16

// Audio Codec I2C (SYS_I2C/I2C0)
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_47
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_48
#define AUDIO_CODEC_ES8311_ADDR     ES8311_CODEC_DEFAULT_ADDR

// PA control via PM1_G3 (not a GPIO pin)
#define AUDIO_CODEC_GPIO_PA     GPIO_NUM_NC

// Button
#define BOOT_BUTTON_GPIO        GPIO_NUM_11
#define USER_BUTTON_GPIO        GPIO_NUM_12

// Display pins
#define DISPLAY_MOSI_PIN        GPIO_NUM_39
#define DISPLAY_CLK_PIN         GPIO_NUM_40
#define DISPLAY_CS_PIN          GPIO_NUM_41
#define DISPLAY_DC_PIN          GPIO_NUM_45
#define DISPLAY_RST_PIN         GPIO_NUM_21
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_38

// Display configuration
#define DISPLAY_WIDTH           135
#define DISPLAY_HEIGHT          240
#define DISPLAY_MIRROR_X        false
#define DISPLAY_MIRROR_Y        false 
#define DISPLAY_SWAP_XY         false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X        52
#define DISPLAY_OFFSET_Y        40
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE        0

// PMIC PM1 address
#define PM1_I2C_ADDR 0x6E

#endif // _BOARD_CONFIG_H_

