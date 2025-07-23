#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include "pin_config.h"

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#ifdef CONFIG_BOARD_TYPE_LILYGO_T_CAMERAPLUS_S3_V1_0_V1_1
#define AUDIO_INPUT_REFERENCE true
#define AUDIO_MIC_I2S_GPIO_BCLK static_cast<gpio_num_t>(MSM261_BCLK)
#define AUDIO_MIC_I2S_GPIO_WS static_cast<gpio_num_t>(MSM261_WS)
#define AUDIO_MIC_I2S_GPIO_DATA static_cast<gpio_num_t>(MSM261_DATA)
#elif defined CONFIG_BOARD_TYPE_LILYGO_T_CAMERAPLUS_S3_V1_2
#define AUDIO_INPUT_REFERENCE false
#define AUDIO_MIC_I2S_GPIO_BCLK GPIO_NUM_NC
#define AUDIO_MIC_I2S_GPIO_WS static_cast<gpio_num_t>(MP34DT05TR_LRCLK)
#define AUDIO_MIC_I2S_GPIO_DATA static_cast<gpio_num_t>(MP34DT05TR_DATA)

#define AUDIO_MIC_SPKR_EN static_cast<gpio_num_t>(MP34DT05TR_MAX98357_EN)
#endif

#define AUDIO_SPKR_I2S_GPIO_BCLK static_cast<gpio_num_t>(MAX98357A_BCLK)
#define AUDIO_SPKR_I2S_GPIO_LRCLK static_cast<gpio_num_t>(MAX98357A_LRCLK)
#define AUDIO_SPKR_I2S_GPIO_DATA static_cast<gpio_num_t>(MAX98357A_DATA)

#define TOUCH_I2C_SDA_PIN static_cast<gpio_num_t>(TP_SDA)
#define TOUCH_I2C_SCL_PIN static_cast<gpio_num_t>(TP_SCL)

#define BUILTIN_LED_GPIO GPIO_NUM_NC
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define KEY1_BUTTON_GPIO static_cast<gpio_num_t>(KEY1)
#define VOLUME_UP_BUTTON_GPIO GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_WIDTH LCD_WIDTH
#define DISPLAY_HEIGHT LCD_HEIGHT
#define DISPLAY_MOSI LCD_MOSI
#define DISPLAY_SCLK LCD_SCLK
#define DISPLAY_DC LCD_DC
#define DISPLAY_RST LCD_RST
#define DISPLAY_CS LCD_CS
#define DISPLAY_BL static_cast<gpio_num_t>(LCD_BL)
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_BACKLIGHT_PIN DISPLAY_BL
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define AP1511B_GPIO static_cast<gpio_num_t>(AP1511B_FBC)

#endif // _BOARD_CONFIG_H_
