#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define PWR_BUTTON_GPIO         GPIO_NUM_6
#define PWR_Control_PIN         GPIO_NUM_7

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_2
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_15
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_39
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_47
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_48
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_38

#define I2C_SCL_IO          GPIO_NUM_10       
#define I2C_SDA_IO          GPIO_NUM_11        


#define I2C_ADDRESS         ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000

#define DISPLAY_WIDTH       412
#define DISPLAY_HEIGHT      412
#define DISPLAY_MIRROR_X    false
#define DISPLAY_MIRROR_Y    false
#define DISPLAY_SWAP_XY     false

#define QSPI_LCD_H_RES           (412)
#define QSPI_LCD_V_RES           (412)
#define QSPI_LCD_BIT_PER_PIXEL   (16)

#define QSPI_LCD_HOST           SPI2_HOST
#define QSPI_PIN_NUM_LCD_PCLK   GPIO_NUM_40
#define QSPI_PIN_NUM_LCD_CS     GPIO_NUM_21
#define QSPI_PIN_NUM_LCD_DATA0  GPIO_NUM_46
#define QSPI_PIN_NUM_LCD_DATA1  GPIO_NUM_45
#define QSPI_PIN_NUM_LCD_DATA2  GPIO_NUM_42
#define QSPI_PIN_NUM_LCD_DATA3  GPIO_NUM_41
#define QSPI_PIN_NUM_LCD_RST    GPIO_NUM_NC
#define QSPI_PIN_NUM_LCD_BL     GPIO_NUM_5

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define TP_PORT          (I2C_NUM_1)
#define TP_PIN_NUM_SDA   (I2C_SDA_IO)
#define TP_PIN_NUM_SCL   (I2C_SCL_IO)
#define TP_PIN_NUM_RST   (GPIO_NUM_NC)
#define TP_PIN_NUM_INT   (GPIO_NUM_4)

#define DISPLAY_BACKLIGHT_PIN           QSPI_PIN_NUM_LCD_BL
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define TAIJIPI_SPD2010_PANEL_BUS_QSPI_CONFIG(sclk, d0, d1, d2, d3, max_trans_sz) \
    {                                                                             \
        .data0_io_num = d0,                                                       \
        .data1_io_num = d1,                                                       \
        .sclk_io_num = sclk,                                                      \
        .data2_io_num = d2,                                                       \
        .data3_io_num = d3,                                                       \
        .max_transfer_sz = max_trans_sz,                                          \
    }


#endif // _BOARD_CONFIG_H_
