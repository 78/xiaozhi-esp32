#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>

// MAX98357A
// #define MAX98357A_BCLK              GPIO_NUM_15
// #define MAX98357A_LRCLK             GPIO_NUM_16
// #define MAX98357A_DATA              GPIO_NUM_7
// #define MAX98357A_SD_MODE           GPIO_NUM_NC

// lcd
#define LCD_SCLK_PIN                GPIO_NUM_21
#define LCD_MOSI_PIN                GPIO_NUM_47
// #define LCD_MISO_PIN             GPIO_NUM_13
#define LCD_DC_PIN                  GPIO_NUM_40
#define LCD_CS_PIN                  GPIO_NUM_41

#define LCD_SPI_HOST                SPI3_HOST

#define DISPLAY_WIDTH               240
#define DISPLAY_HEIGHT              240
#define DISPLAY_MIRROR_X            false
#define DISPLAY_MIRROR_Y            false
#define DISPLAY_SWAP_XY             false

#define DISPLAY_OFFSET_X            0
#define DISPLAY_OFFSET_Y            0

#define DISPLAY_BACKLIGHT_PIN       GPIO_NUM_42
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true


//led
#define CONTROL_LED_PIN             GPIO_NUM_35

// key
#define MENU_KEY_PIN                GPIO_NUM_0
#define VOLUME_UP_KEY_PIN           GPIO_NUM_38
#define VOLUME_DOWN_KEY_PIN         GPIO_NUM_39

//audio
#define AUDIO_INPUT_SAMPLE_RATE     16000
#define AUDIO_OUTPUT_SAMPLE_RATE    24000
#define AUDIO_I2S_MIC_GPIO_WS       GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK      GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN      GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT     GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK     GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK     GPIO_NUM_16

#endif
