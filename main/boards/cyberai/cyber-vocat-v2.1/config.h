#ifndef CYBER_VOCAT_V2_1_CONFIG_H_
#define CYBER_VOCAT_V2_1_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/uart.h>

#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_INPUT_REFERENCE true

#define PG1_POWER_KEY_GPIO GPIO_NUM_40
#define PG1_POWER_KEY_ACTIVE_LEVEL 1
#define PG2_HOLD_GPIO GPIO_NUM_38
#define PG2_HOLD_ACTIVE_LEVEL 1
#define PG2_RUNNING_LEVEL 0
#define POWER_KEY_LONG_PRESS_MS 2000

// Empty-battery shutdown: both thresholds must hold continuously while discharging.
#define LOW_BATTERY_SHUTDOWN_PERCENT 1
#define LOW_BATTERY_SHUTDOWN_MV 3500
#define LOW_BATTERY_SHUTDOWN_CONFIRM_MS 10000
#define PG2_POWER_OFF_PULSE_MS 200

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_WS GPIO_NUM_7
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_16
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_6

#define AUDIO_CODEC_PA_PIN GPIO_NUM_4
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_2
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_1
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR ES7210_CODEC_DEFAULT_ADDR

#define QMA6100P_I2C_ADDR 0x12

// GPIO0 is connected to CH340 auto-download circuitry, not a user BOOT button.

#define DISPLAY_WIDTH 360
#define DISPLAY_HEIGHT 360
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define QSPI_LCD_H_RES (360)
#define QSPI_LCD_V_RES (360)
#define QSPI_LCD_BIT_PER_PIXEL (16)

#define QSPI_LCD_HOST SPI2_HOST
#define QSPI_PIN_NUM_LCD_PCLK GPIO_NUM_21
#define QSPI_PIN_NUM_LCD_CS GPIO_NUM_10
#define QSPI_PIN_NUM_LCD_DATA0 GPIO_NUM_14
#define QSPI_PIN_NUM_LCD_DATA1 GPIO_NUM_9
#define QSPI_PIN_NUM_LCD_DATA2 GPIO_NUM_13
#define QSPI_PIN_NUM_LCD_DATA3 GPIO_NUM_46
#define QSPI_PIN_NUM_LCD_RST GPIO_NUM_47
#define QSPI_PIN_NUM_LCD_TE GPIO_NUM_12
#define QSPI_PIN_NUM_LCD_BL GPIO_NUM_3

#define UART1_TX GPIO_NUM_41
#define UART1_RX GPIO_NUM_42
#define TOUCH_PAD2 GPIO_NUM_20
#define TOUCH_PAD1 GPIO_NUM_NC
#define HEAD_TOUCH_GPIO TOUCH_PAD2
#define HEAD_TOUCH_ACTIVE_LEVEL 1

#define SPEAKING_LED_GPIO GPIO_NUM_19
#define SPEAKING_LED_ACTIVE_LEVEL 0

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define TP_PORT (I2C_NUM_0)
// Touch and LCD share one reset line, driven by the LCD initialization.
#define TP_PIN_NUM_RST QSPI_PIN_NUM_LCD_RST
#define TP_PIN_NUM_INT (GPIO_NUM_11)

#define DISPLAY_BACKLIGHT_PIN QSPI_PIN_NUM_LCD_BL
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define TAIJIPI_ST77916_PANEL_BUS_QSPI_CONFIG(sclk, d0, d1, d2, d3, max_trans_sz) \
    {                                                                             \
        .data0_io_num = d0,                                                       \
        .data1_io_num = d1,                                                       \
        .sclk_io_num = sclk,                                                      \
        .data2_io_num = d2,                                                       \
        .data3_io_num = d3,                                                       \
        .max_transfer_sz = max_trans_sz,                                          \
    }

// SD卡（PG2 已换到 GPIO38，故 D0 可用 GPIO39）
#define BSP_SD_CLK GPIO_NUM_48
#define BSP_SD_CMD GPIO_NUM_45
#define BSP_SD_D0 GPIO_NUM_39
#define MOUNT_POINT "/sdcard"

// ML307 4G module (high level = enabled)
#define ML307_RX_PIN GPIO_NUM_18
#define ML307_TX_PIN GPIO_NUM_8
#define ML307_UART_NUM UART_NUM_2
#define ML307_EN_PIN GPIO_NUM_17

#endif
