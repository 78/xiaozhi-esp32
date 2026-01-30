#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2S Audio Configuration (I2S0)
#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_2
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_6
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_4
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_3

// Audio Codec I2C (SYS_I2C/I2C1)
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_11
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_9
#define AUDIO_CODEC_ES8311_ADDR     ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_GPIO_PA         GPIO_NUM_NC

// Buttons
#define BOOT_BUTTON_GPIO        GPIO_NUM_0

// I2C Configuration
#define SYS_I2C_PORT            I2C_NUM_1

// IO Expander (M5IOE1)
#define IOE1_I2C_ADDR           0x6E
#define IOE1_PIN_LCD_BL         M5IOE1_PIN_9
#define IOE1_PIN_LCD_PWR        M5IOE1_PIN_10
#define IOE1_PIN_LCD_RST        M5IOE1_PIN_11
#define IOE1_PIN_TOUCH_RST      M5IOE1_PIN_8
#define IOE1_PIN_AUDIO_PWR      M5IOE1_PIN_1
#define IOE1_PIN_PA_EN          M5IOE1_PIN_3

// Touch
#define TOUCH_INT_PIN           GPIO_NUM_1

// Display configuration (MIPI DSI)
#define DISPLAY_WIDTH           480
#define DISPLAY_HEIGHT          480
#define DISPLAY_OFFSET_X        0
#define DISPLAY_OFFSET_Y        0
#define DISPLAY_MIRROR_X        false
#define DISPLAY_MIRROR_Y        false
#define DISPLAY_SWAP_XY         false

#define DISPLAY_MIPI_LANE_NUM          2
#define DISPLAY_MIPI_LANE_BITRATE_MBPS 600
#define DISPLAY_PIXEL_CLOCK_MHZ        24
#define DISPLAY_HSYNC_PW               2
#define DISPLAY_HSYNC_BP               40
#define DISPLAY_HSYNC_FP               40
#define DISPLAY_VSYNC_PW               4
#define DISPLAY_VSYNC_BP               8
#define DISPLAY_VSYNC_FP               200

// MIPI DSI PHY power
#define MIPI_DSI_PHY_PWR_LDO_CHAN       3
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500

#endif // _BOARD_CONFIG_H_
