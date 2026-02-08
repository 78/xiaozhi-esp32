#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Audio configuration
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// MAX98357A Amplifier
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_8
#define AUDIO_SPK_ENABLE        GPIO_NUM_5

// INMP441 Microphone
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_2
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_3
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_4

// WS2812B LEDs
#define WS2812B_BOTTOM_GPIO     GPIO_NUM_15
#define WS2812B_VERTICAL_GPIO   GPIO_NUM_16

// SD Card (reserved)
#define SD_MISO_PIN             GPIO_NUM_13
#define SD_SCK_PIN              GPIO_NUM_12
#define SD_MOSI_PIN             GPIO_NUM_11
#define SD_CS_PIN               GPIO_NUM_10

// Buttons (if any, using BOOT button)
#define BOOT_BUTTON_GPIO        GPIO_NUM_0

#endif // _BOARD_CONFIG_H_