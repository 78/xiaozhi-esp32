#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_13
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_12
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_10
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_9
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_11

// NS4150 amplifier STD pin, net PA_CTRL on the schematic.
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_20
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// One shared I2C bus carries ES8311, GSL3680 touch, RX8025T RTC and the CSI sensor.
#define BOARD_I2C_SDA_PIN  GPIO_NUM_7
#define BOARD_I2C_SCL_PIN  GPIO_NUM_8
#define AUDIO_CODEC_I2C_SDA_PIN  BOARD_I2C_SDA_PIN
#define AUDIO_CODEC_I2C_SCL_PIN  BOARD_I2C_SCL_PIN

#define BOOT_BUTTON_GPIO        GPIO_NUM_35

#define DISPLAY_WIDTH  800
#define DISPLAY_HEIGHT 1280

#define LCD_BIT_PER_PIXEL          (16)
#define PIN_NUM_LCD_RST            GPIO_NUM_27
#define LCD_MIPI_DSI_LANE_NUM      (2)
#define LCD_MIPI_DSI_LANE_BITRATE_MBPS (1500)

// Vendor SDK (esp_lcd_jd9365 v1.0.2) values. The registry driver's
// JD9365_800_1280_PANEL_60HZ_DPI_CONFIG_CF macro uses 80MHz / vsync_bp=12 /
// vsync_fp=30, which this panel does not lock onto.
#define LCD_DPI_CLOCK_FREQ_MHZ     (60)
#define LCD_HSYNC_PULSE_WIDTH      (20)
#define LCD_HSYNC_BACK_PORCH       (20)
#define LCD_HSYNC_FRONT_PORCH      (40)
#define LCD_VSYNC_PULSE_WIDTH      (4)
#define LCD_VSYNC_BACK_PORCH       (8)
#define LCD_VSYNC_FRONT_PORCH      (20)

#define MIPI_DSI_PHY_PWR_LDO_CHAN          (3)
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV    (2500)

#define DISPLAY_SWAP_XY   false
#define DISPLAY_MIRROR_X  false
#define DISPLAY_MIRROR_Y  false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

// LCD_PWM on the schematic, drives the MP3202 boost converter enable pin.
// LEDC cannot route to this pad on ESP32-P4, so the board uses on/off GPIO control.
#define DISPLAY_BACKLIGHT_PIN            GPIO_NUM_23
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT  false

#define TOUCH_RST_PIN  GPIO_NUM_22
#define TOUCH_INT_PIN  GPIO_NUM_21

#endif // _BOARD_CONFIG_H_
