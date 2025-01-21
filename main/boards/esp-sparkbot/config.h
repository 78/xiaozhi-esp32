#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

#define AUDIO_INPUT_REFERENCE    false

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_45
#define AUDIO_I2S_GPIO_WS GPIO_NUM_41
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_39
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_40
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_42

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_46
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_4
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_5
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_DC_GPIO     GPIO_NUM_43
#define DISPLAY_CS_GPIO     GPIO_NUM_44
#define DISPLAY_CLK_GPIO    GPIO_NUM_21
#define DISPLAY_MOSI_GPIO   GPIO_NUM_47
#define DISPLAY_RST_GPIO    GPIO_NUM_NC

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_46
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false


#endif // _BOARD_CONFIG_H_
