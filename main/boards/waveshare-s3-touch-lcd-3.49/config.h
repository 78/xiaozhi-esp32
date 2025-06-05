#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE    true

#define AUDIO_I2S_GPIO_MCLK  GPIO_NUM_7
#define AUDIO_I2S_GPIO_WS    GPIO_NUM_46
#define AUDIO_I2S_GPIO_BCLK  GPIO_NUM_15
#define AUDIO_I2S_GPIO_DIN   GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT  GPIO_NUM_45

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_47
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_48
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO        GPIO_NUM_0


/*disp-qspi-lcd*/
#define LCD_CS       (gpio_num_t)9
#define LCD_PCLK     (gpio_num_t)10
#define LCD_D0       (gpio_num_t)11 
#define LCD_D1       (gpio_num_t)12
#define LCD_D2       (gpio_num_t)13
#define LCD_D3       (gpio_num_t)14
#define LCD_RST      (gpio_num_t)21
#define LCD_LIGHT    (gpio_num_t)8

#define EXAMPLE_LCD_H_RES 180
#define EXAMPLE_LCD_V_RES 640
 
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#endif // _BOARD_CONFIG_H_
