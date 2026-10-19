#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include "pin_config.h"

/*
 * The V1.1 microphone is PDM and is driven by a simplex codec, so there is no
 * playback reference channel available for AEC.
 *
 * The capture rate is deliberately 32 kHz, not the 16 kHz the wake-word engine
 * wants. The ESP32-S3 derives the PDM clock as 64 x sample rate, so 16 kHz gives
 * only ~1.02 MHz - below the MP34DT05-A minimum of ~1.2 MHz, where the mic stays
 * in power-down and its data line sits at zero. 32 kHz gives ~2.05 MHz, and the
 * audio service resamples 32 kHz down to 16 kHz for the detector.
 */
#define AUDIO_INPUT_REFERENCE false
#define AUDIO_INPUT_SAMPLE_RATE 32000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_MIC_PDM_GPIO_CLK static_cast<gpio_num_t>(MP34DT05_CLK)
#define AUDIO_MIC_PDM_GPIO_DATA static_cast<gpio_num_t>(MP34DT05_DATA)

#define AUDIO_SPKR_I2S_GPIO_BCLK static_cast<gpio_num_t>(MAX98357A_BCLK)
#define AUDIO_SPKR_I2S_GPIO_LRCLK static_cast<gpio_num_t>(MAX98357A_LRCLK)
#define AUDIO_SPKR_I2S_GPIO_DATA static_cast<gpio_num_t>(MAX98357A_DATA)
#define AUDIO_SPKR_ENABLE static_cast<gpio_num_t>(MAX98357A_SD_MODE)

#define TOUCH_I2C_SDA_PIN static_cast<gpio_num_t>(TP_SDA)
#define TOUCH_I2C_SCL_PIN static_cast<gpio_num_t>(TP_SCL)

#define BUILTIN_LED_GPIO GPIO_NUM_NC
#define BOOT_BUTTON_GPIO GPIO_NUM_0
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

#endif // _BOARD_CONFIG_H_
