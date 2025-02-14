
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_DEFAULT_OUTPUT_VOLUME 90

// 如果使用 Duplex I2S 模式，请注释下面一行
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_42
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_41
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_2
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_3
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_46
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_1

#else

#define AUDIO_I2S_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7

#endif


#define BUILTIN_LED_GPIO        GPIO_NUM_38
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define RESET_NVS_BUTTON_GPIO       GPIO_NUM_NC
#define RESET_FACTORY_BUTTON_GPIO   GPIO_NUM_NC

#define DISPLAY_SDA_PIN GPIO_NUM_NC
#define DISPLAY_SCL_PIN GPIO_NUM_NC
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  280
#define DISPLAY_SWAP_XY  false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define BACKLIGHT_INVERT false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  20
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_48
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define ML307_RX_PIN GPIO_NUM_12
#define ML307_TX_PIN GPIO_NUM_13

#endif // _BOARD_CONFIG_H_
