/*
 * @Date: 2025-03-01 10:58:13
 * @LastEditors: zhouke
 * @LastEditTime: 2025-03-02 21:24:38
 * @FilePath: \xiaozhi-esp32\main\boards\fancheng-0.96-4g\config.h
 */
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000


#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16


#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39

#define DISPLAY_SDA_PIN GPIO_NUM_8
#define DISPLAY_SCL_PIN GPIO_NUM_20
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64


#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true


#define ML307_RX_PIN GPIO_NUM_11
#define ML307_TX_PIN GPIO_NUM_12


#endif // _BOARD_CONFIG_H_
