#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_REFERENCE       false
#define AUDIO_INPUT_SAMPLE_RATE     24000
#define AUDIO_OUTPUT_SAMPLE_RATE    24000
#define AUDIO_CODEC_ES8311_ADDR     ES8311_CODEC_DEFAULT_ADDR

// SYS_I2C / I2C1
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_47
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_48

#define AUDIO_I2S_GPIO_MCLK         GPIO_NUM_18
#define AUDIO_I2S_GPIO_WS           GPIO_NUM_15
#define AUDIO_I2S_GPIO_BCLK         GPIO_NUM_17
#define AUDIO_I2S_GPIO_DIN          GPIO_NUM_16
#define AUDIO_I2S_GPIO_DOUT         GPIO_NUM_21
#define AUDIO_CODEC_GPIO_PA         GPIO_NUM_NC

// QSPI display (CO5300)
#define DISPLAY_QSPI_HOST           SPI2_HOST
#define DISPLAY_QSPI_CS             GPIO_NUM_39
#define DISPLAY_QSPI_SCK            GPIO_NUM_40
#define DISPLAY_QSPI_D0             GPIO_NUM_41
#define DISPLAY_QSPI_D1             GPIO_NUM_42
#define DISPLAY_QSPI_D2             GPIO_NUM_46
#define DISPLAY_QSPI_D3             GPIO_NUM_45
#define DISPLAY_TE_PIN              GPIO_NUM_38

#define DISPLAY_WIDTH               466
#define DISPLAY_HEIGHT              466
#define DISPLAY_MIRROR_X            false
#define DISPLAY_MIRROR_Y            false
#define DISPLAY_SWAP_XY             false
#define DISPLAY_OFFSET_X            0
#define DISPLAY_OFFSET_Y            0

// Round screen UI layout (466x466)
#define DISPLAY_ROUND_EDGE_INSET        56 // top/bottom arc safe zone
#define DISPLAY_STATUS_BAR_TOP_OFF      56 // status text: upper half, below WiFi/battery icons
#define DISPLAY_CHAT_BAR_BOTTOM_OFF     66 // subtitle: lifted from bottom arc

// Touch (CST820B @ 0x15)
#define TOUCH_INT_PIN               GPIO_NUM_13

// Buttons
#define BUTTON1_GPIO                GPIO_NUM_2
#define BUTTON2_GPIO                GPIO_NUM_1

// M5IOE1 @ 0x4F
#define M5IOE1_I2C_ADDR             0x4F
#define IOE_PIN_LCD_POWER           M5IOE1_PIN_8
#define IOE_PIN_LCD_RST             M5IOE1_PIN_5
#define IOE_PIN_TOUCH_RST           M5IOE1_PIN_4
#define IOE_PIN_CODEC_POWER         M5IOE1_PIN_3
#define IOE_PIN_PA_EN               M5IOE1_PIN_10
#define IOE_PIN_MOTOR               M5IOE1_PIN_9

// M5PM1 @ 0x6E
#define PMIC_PIN_CHARGE_STATE       M5PM1_GPIO_NUM_2
#define PMIC_PIN_CHARGE_PROG        M5PM1_GPIO_NUM_3

#endif // _BOARD_CONFIG_H_
