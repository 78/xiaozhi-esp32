#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Audio codec
#define AUDIO_INPUT_SAMPLE_RATE     16000
#define AUDIO_OUTPUT_SAMPLE_RATE    16000

#define AUDIO_INPUT_REFERENCE       true

#define AUDIO_I2S_GPIO_MCLK         GPIO_NUM_33
#define AUDIO_I2S_GPIO_WS           GPIO_NUM_37
#define AUDIO_I2S_GPIO_BCLK         GPIO_NUM_34
#define AUDIO_I2S_GPIO_DIN          GPIO_NUM_36
#define AUDIO_I2S_GPIO_DOUT         GPIO_NUM_38

#define AUDIO_CODEC_PA_PIN          GPIO_NUM_40
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_21
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_20
#define AUDIO_CODEC_ES8311_ADDR     ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7243E_ADDR    (0x10)

// button
#define BOOT_BUTTON_GPIO            GPIO_NUM_35

// camera
#define CAMERA_PIN_PWDN             GPIO_NUM_23

// backlight
#define LCD_PIN_BL                  GPIO_NUM_16

#endif // _BOARD_CONFIG_H_
