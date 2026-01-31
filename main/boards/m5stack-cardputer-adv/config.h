#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>


#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2S Audio Configuration
#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_NC
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_41
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_43
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_42
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_46

// Audio Codec I2C (SYS_I2C/I2C0)
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_8
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_9
#define AUDIO_CODEC_ES8311_ADDR     ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_GPIO_PA         GPIO_NUM_NC

// Buttons
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define USER_BUTTON_GPIO        BOOT_BUTTON_GPIO

// Display pins
#define DISPLAY_CLK_PIN         GPIO_NUM_36
#define DISPLAY_MOSI_PIN        GPIO_NUM_35
#define DISPLAY_CS_PIN          GPIO_NUM_37
#define DISPLAY_DC_PIN          GPIO_NUM_34
#define DISPLAY_RST_PIN         GPIO_NUM_33
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_38

// Display configuration
#define DISPLAY_WIDTH           240
#define DISPLAY_HEIGHT          135
#define DISPLAY_MIRROR_X        true
#define DISPLAY_MIRROR_Y        false
#define DISPLAY_SWAP_XY         true
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X        0
#define DISPLAY_OFFSET_Y        0
#define DISPLAY_PANEL_OFFSET_X  40
#define DISPLAY_PANEL_OFFSET_Y  53
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE        0

#endif // _BOARD_CONFIG_H_

