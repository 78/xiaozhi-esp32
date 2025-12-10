#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#ifdef CONFIG_SD_CARD_MMC_INTERFACE
// Define to use 4-bit SDMMC bus width; comment out to use 1-bit bus width
#define CARD_SDMMC_BUS_WIDTH_4BIT

#ifdef CARD_SDMMC_BUS_WIDTH_4BIT
#define CARD_SDMMC_CLK_GPIO GPIO_NUM_38 // CLK pin
#define CARD_SDMMC_CMD_GPIO GPIO_NUM_40 // MOSI pin or DI
#define CARD_SDMMC_D0_GPIO  GPIO_NUM_39 // MISO pin or DO
#define CARD_SDMMC_D1_GPIO  GPIO_NUM_41
#define CARD_SDMMC_D2_GPIO  GPIO_NUM_48
#define CARD_SDMMC_D3_GPIO  GPIO_NUM_47  // CS pin
#else
#define CARD_SDMMC_CLK_GPIO GPIO_NUM_38
#define CARD_SDMMC_CMD_GPIO GPIO_NUM_40
#define CARD_SDMMC_D0_GPIO GPIO_NUM_39
#endif
#endif // CONFIG_SD_CARD_MMC_INTERFACE

#ifdef CONFIG_SD_CARD_SPI_INTERFACE
#define CARD_SPI_MOSI_GPIO GPIO_NUM_40  // DI
#define CARD_SPI_MISO_GPIO GPIO_NUM_39  // DO
#define CARD_SPI_SCLK_GPIO GPIO_NUM_38  // CLK
#define CARD_SPI_CS_GPIO   GPIO_NUM_47  // CS
#endif // CONFIG_SD_CARD_SPI_INTERFACE

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Audio I2S section
#define AUDIO_I2S_GPIO_MCLK      GPIO_NUM_4  //MCLK
#define AUDIO_I2S_GPIO_BCLK      GPIO_NUM_5  //SCK
#define AUDIO_I2S_GPIO_DIN       GPIO_NUM_6  //DIN
#define AUDIO_I2S_GPIO_WS        GPIO_NUM_7  //LRC
#define AUDIO_I2S_GPIO_DOUT      GPIO_NUM_8  //DOUT
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_1  //PA

// Audio I2C section
#define AUDIO_CODEC_I2C_NUM      I2C_NUM_0
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_15
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_16
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// Touchscreen section (FT6236G)
#define TOUCH_I2C_NUM            I2C_NUM_0  // Shared audio I2C bus
#define TOUCH_I2C_SCL_PIN        GPIO_NUM_15
#define TOUCH_I2C_SDA_PIN        GPIO_NUM_16
#define TOUCH_RST_PIN            GPIO_NUM_18  // Touchscreen reset, active low
#define TOUCH_INT_PIN            GPIO_NUM_17  // Touch interrupt, input low level when touched
#define TOUCH_I2C_ADDR           0x38         // Default address of FT6236G

// Boot pin
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define BUILTIN_LED_GPIO GPIO_NUM_42

// Screen display section
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_45

#define DISPLAY_RST_PIN       GPIO_NUM_NC
#define DISPLAY_SCK_PIN       GPIO_NUM_12
#define DISPLAY_DC_PIN        GPIO_NUM_46
#define DISPLAY_CS_PIN        GPIO_NUM_10
#define DISPLAY_MOSI_PIN      GPIO_NUM_11
#define DISPLAY_MIS0_PIN      GPIO_NUM_13
#define DISPLAY_SPI_SCLK_HZ   (20 * 1000 * 1000)

#define LCD_SPI_HOST          SPI3_HOST

#define LCD_TYPE_ILI9341_SERIAL
#define DISPLAY_WIDTH         240
#define DISPLAY_HEIGHT        320
#define DISPLAY_MIRROR_X      true
#define DISPLAY_MIRROR_Y      false
#define DISPLAY_SWAP_XY       false
#define DISPLAY_INVERT_COLOR  true
#define DISPLAY_RGB_ORDER     LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X      0
#define DISPLAY_OFFSET_Y      0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE      0

#endif  // _BOARD_CONFIG_H_
