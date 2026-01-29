#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// M5Stack StopWatch Board configuration

#include <driver/gpio.h>
#include "M5IOE1.h"

#define AUDIO_INPUT_REFERENCE    false
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2C Configuration
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_47
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_48
#define AUDIO_CODEC_ES8311_ADDR     ES8311_CODEC_DEFAULT_ADDR

// I2S Audio Configuration
#define AUDIO_I2S_GPIO_MCLK         GPIO_NUM_18
#define AUDIO_I2S_GPIO_BCLK         GPIO_NUM_17
#define AUDIO_I2S_GPIO_WS           GPIO_NUM_15
#define AUDIO_I2S_GPIO_DOUT         GPIO_NUM_21
#define AUDIO_I2S_GPIO_DIN          GPIO_NUM_16
#define AUDIO_CODEC_GPIO_PA         GPIO_NUM_NC // AW8737A PA is controlled by IOE1
#define AUDIO_CODEC_AW8737A_PIN     M5IOE1_PIN_10 // M5IOE1_G10 for AW8737A pulse control

// Display QSPI Configuration (CO5300, 466x466 circular)
#define DISPLAY_QSPI_SCK            GPIO_NUM_40
#define DISPLAY_QSPI_CS             GPIO_NUM_39
#define DISPLAY_QSPI_D0             GPIO_NUM_41
#define DISPLAY_QSPI_D1             GPIO_NUM_42
#define DISPLAY_QSPI_D2             GPIO_NUM_46
#define DISPLAY_QSPI_D3             GPIO_NUM_45
#define DISPLAY_TE                  GPIO_NUM_38 // Tearing Effect
#define DISPLAY_RST                 GPIO_NUM_NC // Controlled via M5IO1E1_G5

// Display Parameters
#define DISPLAY_WIDTH               466
#define DISPLAY_HEIGHT              466
#define DISPLAY_MIRROR_X            false
#define DISPLAY_MIRROR_Y            false
#define DISPLAY_SWAP_XY             false
#define DISPLAY_INVERT_COLOR        false
#define DISPLAY_RGB_ORDER           LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X            0
#define DISPLAY_OFFSET_Y            0
#define DISPLAY_SPI_MODE            0

// Buttons
#define BUILTIN_LED_GPIO            GPIO_NUM_NC
#define BUTTON_1_GPIO               GPIO_NUM_2    // Button 1: Wake up conversation
#define BUTTON_2_GPIO               GPIO_NUM_1    // Button 2: Adjust volume
#define USER_BUTTON_GPIO            BUTTON_1_GPIO  // Alias for compatibility

// Backlight (AMOLED doesn't need backlight, but keep for compatibility)
#define DISPLAY_BACKLIGHT_PIN       GPIO_NUM_NC

#endif // _BOARD_CONFIG_H_

