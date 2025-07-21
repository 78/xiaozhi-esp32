#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/spi_master.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000   
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_INPUT_REFERENCE    true

#define CORDEC_POWER_CTRL  GPIO_NUM_48

#define POWER_CTRL  GPIO_NUM_9
#define LED_G       GPIO_NUM_43
#define SD_MISO     GPIO_NUM_17
#define SD_SCK      GPIO_NUM_16
#define SD_MOSI     GPIO_NUM_38

#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_42
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_39
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_40
#define AUDIO_I2S_GPIO_DIN_1    GPIO_NUM_15
#define AUDIO_I2S_GPIO_DIN_2    GPIO_NUM_3
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_41

#define AUDIO_CODEC_PA_PIN_1    GPIO_NUM_4
#define AUDIO_CODEC_PA_PIN_2     GPIO_NUM_15
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_2
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_1
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_WIDTH       360
#define DISPLAY_HEIGHT      360
#define DISPLAY_MIRROR_X    false
#define DISPLAY_MIRROR_Y    false
#define DISPLAY_SWAP_XY     false

#define QSPI_LCD_H_RES           (360)
#define QSPI_LCD_V_RES           (360)
#define QSPI_LCD_BIT_PER_PIXEL   (16)

#define QSPI_LCD_HOST           SPI2_HOST
#define QSPI_PIN_NUM_LCD_PCLK   GPIO_NUM_18
#define QSPI_PIN_NUM_LCD_CS     GPIO_NUM_14
#define QSPI_PIN_NUM_LCD_DATA0  GPIO_NUM_46
#define QSPI_PIN_NUM_LCD_DATA1  GPIO_NUM_13
#define QSPI_PIN_NUM_LCD_DATA2  GPIO_NUM_11
#define QSPI_PIN_NUM_LCD_DATA3  GPIO_NUM_12
#define QSPI_PIN_NUM_LCD_RST_1  GPIO_NUM_3
#define QSPI_PIN_NUM_LCD_RST_2  GPIO_NUM_47
#define QSPI_PIN_NUM_LCD_BL     GPIO_NUM_44

#define UART1_TX_1     GPIO_NUM_6
#define UART1_TX_2     GPIO_NUM_5
#define UART1_RX_1     GPIO_NUM_5
#define UART1_RX_2     GPIO_NUM_4
#define TOUCH_PAD2_1     GPIO_NUM_NC
#define TOUCH_PAD2_2     GPIO_NUM_6
#define TOUCH_PAD1     GPIO_NUM_7

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define TP_PORT          (I2C_NUM_1)
#define TP_PIN_NUM_RST   (GPIO_NUM_NC)
#define TP_PIN_NUM_INT   (GPIO_NUM_10)

#define DISPLAY_BACKLIGHT_PIN           QSPI_PIN_NUM_LCD_BL
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

#endif // _BOARD_CONFIG_H_
