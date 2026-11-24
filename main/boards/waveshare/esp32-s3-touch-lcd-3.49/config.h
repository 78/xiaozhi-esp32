#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include "lvgl.h"

#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE    true

#define AUDIO_I2S_GPIO_MCLK  GPIO_NUM_7
#define AUDIO_I2S_GPIO_WS    GPIO_NUM_46
#define AUDIO_I2S_GPIO_BCLK  GPIO_NUM_15
#define AUDIO_I2S_GPIO_DIN   GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT  GPIO_NUM_45

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_47
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_48
#define Dev_Touch_I2C_SDA_PIN    GPIO_NUM_17
#define Dev_Touch_I2C_SCL_PIN    GPIO_NUM_18
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define I2C_Touch_ADDRESS       0x3b
#define I2C_Touch_SDA_PIN       GPIO_NUM_17
#define I2C_Touch_SCL_PIN       GPIO_NUM_18

#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define PWR_BUTTON_GPIO         GPIO_NUM_16

// Battery voltage on GPIO4 (ADC1 channel 3) through a 1:3 divider, per Waveshare's
// 01_ADC_Test demo (battery = pin voltage * 3). Only the resistor ratio matters.
#define BATTERY_ADC_UNIT        ADC_UNIT_1
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_3
#define BATTERY_UPPER_RESISTOR  200000.0f
#define BATTERY_LOWER_RESISTOR  100000.0f

#define LCD_CS       GPIO_NUM_9
#define LCD_PCLK     GPIO_NUM_10
#define LCD_D0       GPIO_NUM_11
#define LCD_D1       GPIO_NUM_12
#define LCD_D2       GPIO_NUM_13
#define LCD_D3       GPIO_NUM_14
#define LCD_RST      GPIO_NUM_21
#define LCD_LIGHT    (-1)

#define DISPLAY_WIDTH  172
#define DISPLAY_HEIGHT 640
#define LVGL_DMA_BUFF_LEN (DISPLAY_WIDTH * 64 * 2)
#define LVGL_SPIRAM_BUFF_LEN (DISPLAY_WIDTH * DISPLAY_HEIGHT * 2)

#define DISPLAY_ROTATION_90 true

#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0


#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_8
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true


#endif // _BOARD_CONFIG_H_
