#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE     24000
#define AUDIO_OUTPUT_SAMPLE_RATE    24000

#define BOOT_BUTTON_GPIO            GPIO_NUM_0

#define AUDIO_I2S_GPIO_MCLK         GPIO_NUM_12
#define AUDIO_I2S_GPIO_BCLK         GPIO_NUM_13
#define AUDIO_I2S_GPIO_WS           GPIO_NUM_14
#define AUDIO_I2S_GPIO_DIN          GPIO_NUM_15
#define AUDIO_I2S_GPIO_DOUT         GPIO_NUM_16
#define AUDIO_CODEC_PA_PIN          GPIO_NUM_9
#define AUDIO_INPUT_REFERENCE       true
#define AUDIO_CODEC_ES8311_ADDR     ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR     ES7210_CODEC_DEFAULT_ADDR

#define I2C_SCL_IO                  GPIO_NUM_10       
#define I2C_SDA_IO                  GPIO_NUM_11        

#define DISPLAY_SDA_PIN             I2C_SDA_IO
#define DISPLAY_SCL_PIN             I2C_SCL_IO

#define DISPLAY_MISO_PIN            GPIO_NUM_40
#define DISPLAY_MOSI_PIN            GPIO_NUM_42
#define DISPLAY_SCLK_PIN            GPIO_NUM_41
#define DISPLAY_CS_PIN              GPIO_NUM_47
#define DISPLAY_DC_PIN              GPIO_NUM_45
#define DISPLAY_RESET_PIN           GPIO_NUM_48
#define DISPLAY_BACKLIGHT_PIN       GPIO_NUM_46

#define DISPLAY2_CS_PIN             GPIO_NUM_38
#define DISPLAY2_RESET_PIN          GPIO_NUM_8
#define DISPLAY2_BACKLIGHT_PIN      GPIO_NUM_39

#define DISPLAY_SPI_SCLK_HZ         (80 * 1000 * 1000)

#define DISPLAY_WIDTH           240
#define DISPLAY_HEIGHT          240
#define DISPLAY_MIRROR_X        false
#define DISPLAY_MIRROR_Y        false
#define DISPLAY_SWAP_XY         true
#define DISPLAY2_MIRROR_X       true
#define DISPLAY2_MIRROR_Y       true
#define DISPLAY2_SWAP_XY        true
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X        0
#define DISPLAY_OFFSET_Y        0
#define DISPLAY_SPI_MODE        0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#endif // _BOARD_CONFIG_H_
