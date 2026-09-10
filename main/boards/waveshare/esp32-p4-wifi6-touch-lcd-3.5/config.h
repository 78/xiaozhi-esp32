#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE         24000
#define AUDIO_OUTPUT_SAMPLE_RATE        24000

#define AUDIO_INPUT_REFERENCE           true

#define AUDIO_I2S_GPIO_MCLK             GPIO_NUM_13
#define AUDIO_I2S_GPIO_WS               GPIO_NUM_10
#define AUDIO_I2S_GPIO_BCLK             GPIO_NUM_12
#define AUDIO_I2S_GPIO_DIN              GPIO_NUM_11
#define AUDIO_I2S_GPIO_DOUT             GPIO_NUM_9

#define AUDIO_CODEC_PA_PIN              GPIO_NUM_53
#define AUDIO_CODEC_I2C_SDA_PIN         GPIO_NUM_7
#define AUDIO_CODEC_I2C_SCL_PIN         GPIO_NUM_8
#define AUDIO_CODEC_ES8311_ADDR         ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR         ES7210_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO                GPIO_NUM_35   

#define DISPLAY_WIDTH                   (320)
#define DISPLAY_HEIGHT                  (480)
#define PIN_NUM_LCD_RST                 GPIO_NUM_27
#define DISPLAY_BACKLIGHT_PIN           GPIO_NUM_28
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define TOUCH_RST_PIN                   GPIO_NUM_29
#define TOUCH_INT_PIN                   GPIO_NUM_50
#define LCD_SPI_MOSI_PIN                GPIO_NUM_20
#define LCD_SPI_CLK_PIN                 GPIO_NUM_21
#define LCD_SPI_CS_PIN                  GPIO_NUM_23
#define LCD_SPI_DC_PIN                  GPIO_NUM_26

#define DISPLAY_SWAP_XY                 false
#define DISPLAY_MIRROR_X                true
#define DISPLAY_MIRROR_Y                false

#define DISPLAY_OFFSET_X                0
#define DISPLAY_OFFSET_Y                0


#endif // _BOARD_CONFIG_H_
