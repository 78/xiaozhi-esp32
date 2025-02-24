#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_DEFAULT_OUTPUT_VOLUME 10

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_NC
#define AUDIO_I2S_GPIO_WS GPIO_NUM_27
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_25
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_33
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_26

#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_13
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_15
#define AUDIO_CODEC_NAU88C22_ADDR 0x1A

#define BUILTIN_LED_GPIO GPIO_NUM_NC
#define BOOT_BUTTON_GPIO GPIO_NUM_32
#define VOLUME_UP_BUTTON_GPIO GPIO_NUM_35
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_34
#define POWER_BUTTON_GPIO GPIO_NUM_36

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 172
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY true

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 34

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_21
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#endif // _BOARD_CONFIG_H_
