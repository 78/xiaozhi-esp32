#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE    true

#define BOARD_IIC_BUS_PORT  I2C_NUM_0
#define BOARD_IIC_BUS_SDA   (GPIO_NUM_41)
#define BOARD_IIC_BUS_SCL   (GPIO_NUM_45)

#define SENSOR_QMI8658_IIC_ADDRESS  (0x6B)
#define SENSOR_QMI8658_IIC_FREQ     (400000)
#define SENSOR_QMI8658_IIC_SDA      (BOARD_IIC_BUS_SDA)
#define SENSOR_QMI8658_IIC_SCL      (BOARD_IIC_BUS_SCL)

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_NC
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_16
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_18
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_NC
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_17
#define AUDIO_MUTE_PIN      GPIO_NUM_15

#define AUDIO_MIC_WS_PIN    GPIO_NUM_NC
#define AUDIO_MIC_SD_PIN    GPIO_NUM_47
#define AUDIO_MIC_SCK_PIN   GPIO_NUM_48

#define QSPI_LCD_WIDTH_RES  (466)
#define QSPI_LCD_HEIGHT_RES (466)
#define QSPI_LCD_BIT_PER_PIXEL (16)

#define QSPI_LCD_HOST           SPI2_HOST
#define QSPI_PIN_NUM_LCD_EN     GPIO_NUM_40
#define QSPI_PIN_NUM_LCD_PCLK   GPIO_NUM_13
#define QSPI_PIN_NUM_LCD_CS     GPIO_NUM_7
#define QSPI_PIN_NUM_LCD_DATA0  GPIO_NUM_12
#define QSPI_PIN_NUM_LCD_DATA1  GPIO_NUM_8
#define QSPI_PIN_NUM_LCD_DATA2  GPIO_NUM_14
#define QSPI_PIN_NUM_LCD_DATA3  GPIO_NUM_9
#define QSPI_PIN_NUM_LCD_RST    GPIO_NUM_11
#define QSPI_PIN_NUM_LCD_TE     GPIO_NUM_10
#define QSPI_PIN_NUM_LCD_BL     GPIO_NUM_NC

#define DISPLAY_WIDTH       QSPI_LCD_WIDTH_RES
#define DISPLAY_HEIGHT      QSPI_LCD_HEIGHT_RES

#define DISPLAY_MIRROR_X    false
#define DISPLAY_MIRROR_Y    false
#define DISPLAY_SWAP_XY     false

#define DISPLAY_GAP_X  6
#define DISPLAY_GAP_Y  0
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define TP_PORT             (BOARD_IIC_BUS_PORT)
#define TP_PIN_NUM_TP_SDA   (BOARD_IIC_BUS_SDA)
#define TP_PIN_NUM_TP_SCL   (BOARD_IIC_BUS_SCL)
#define TP_PIN_NUM_TP_RST   (GPIO_NUM_46)
#define TP_PIN_NUM_TP_INT   (GPIO_NUM_42)

#define LOCAL_SH8601_PANEL_BUS_QSPI_CONFIG(sclk, d0, d1, d2, d3, max_trans_sz) \
    {                                                           \
        .data0_io_num = d0,                                     \
        .data1_io_num = d1,                                     \
        .sclk_io_num = sclk,                                    \
        .data2_io_num = d2,                                     \
        .data3_io_num = d3,                                     \
        .max_transfer_sz = max_trans_sz,                        \
    }

#define LOCAL_LCD_TOUCH_IO_I2C_CST9217_ADDRESS    0x15

#define BOOT_BUTTON_GPIO        GPIO_NUM_NC

#define SD_MMC_D0_PIN   GPIO_NUM_2
#define SD_MMC_D1_PIN   GPIO_NUM_1
#define SD_MMC_D2_PIN   GPIO_NUM_6
#define SD_MMC_D3_PIN   GPIO_NUM_5
#define SD_MMC_CLK_PIN  GPIO_NUM_3
#define SD_MMC_CMD_PIN  GPIO_NUM_4

#endif // _BOARD_CONFIG_H_
