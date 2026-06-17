#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE       true
#define AUDIO_I2S_GPIO_MCLK         GPIO_NUM_38
#define AUDIO_I2S_GPIO_WS           GPIO_NUM_40
#define AUDIO_I2S_GPIO_BCLK         GPIO_NUM_39
#define AUDIO_I2S_GPIO_DIN          GPIO_NUM_42
#define AUDIO_I2S_GPIO_DOUT         GPIO_NUM_41

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_46
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_47
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_48
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO        GPIO_NUM_0

#define LCD_D0_PIN          GPIO_NUM_9
#define LCD_D1_PIN          GPIO_NUM_10
#define LCD_D2_PIN          GPIO_NUM_11
#define LCD_D3_PIN          GPIO_NUM_12
#define LCD_CS_PIN          GPIO_NUM_15
#define LCD_SCK_PIN         GPIO_NUM_14
#define LCD_RST_PIN         GPIO_NUM_13
#define LCD_TE_PIN          GPIO_NUM_8

#define TP_RST_PIN          GPIO_NUM_16
#define TP_INT_PIN          GPIO_NUM_17


#define LCD_WIDTH           466    
#define LCD_HEIGHT          466 
 
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0



#endif // _BOARD_CONFIG_H_
