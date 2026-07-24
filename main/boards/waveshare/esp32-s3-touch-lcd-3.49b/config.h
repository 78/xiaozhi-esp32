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

#define LCD_CS       GPIO_NUM_9
#define LCD_PCLK     GPIO_NUM_10
#define LCD_D0       GPIO_NUM_11
#define LCD_D1       GPIO_NUM_12
#define LCD_D2       GPIO_NUM_13
#define LCD_D3       GPIO_NUM_14

// On the 3.49B (V1.1) hardware revision, LCD reset and backlight enable are
// no longer wired to direct GPIOs. They are instead controlled through the
// TCA9554 IO expander (see IO_EXPANDER_PIN_NUM_5 / IO_EXPANDER_PIN_NUM_1
// usage in the board source).
#define LCD_RST      (-1)
#define LCD_LIGHT    (-1)

#define DISPLAY_WIDTH  172
#define DISPLAY_HEIGHT 640
#define LVGL_DMA_BUFF_LEN (DISPLAY_WIDTH * 64 * 2)
#define LVGL_SPIRAM_BUFF_LEN (DISPLAY_WIDTH * DISPLAY_HEIGHT * 2)

#define DISPLAY_ROTATION_90 false

#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

// The 3.49B hardware revision routes the backlight PWM signal to GPIO42
// instead of GPIO8. Backlight power is additionally gated by the TCA9554
// IO expander's BL_EN pin, which must be enabled for the PWM duty cycle to
// have any effect on the physical backlight.
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_42
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#endif // _BOARD_CONFIG_H_
