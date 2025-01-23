#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE     24000
#define AUDIO_OUTPUT_SAMPLE_RATE    24000

#define AUDIO_I2S_GPIO_WS           GPIO_NUM_13
#define AUDIO_I2S_GPIO_BCLK         GPIO_NUM_21
#define AUDIO_I2S_GPIO_DIN          GPIO_NUM_47
#define AUDIO_I2S_GPIO_DOUT         GPIO_NUM_14

#define BUILTIN_LED_GPIO            GPIO_NUM_4
#define BOOT_BUTTON_GPIO            GPIO_NUM_0

#define DISPLAY_OFFSET_X            0
#define DISPLAY_OFFSET_Y            0
#define DISPLAY_WIDTH               320
#define DISPLAY_HEIGHT              240
#define DISPLAY_SWAP_XY             true
#define DISPLAY_MIRROR_X            true
#define DISPLAY_MIRROR_Y            false

#define DISPLAY_BACKLIGHT_PIN       GPIO_NUM_NC
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#endif // _BOARD_CONFIG_H_
