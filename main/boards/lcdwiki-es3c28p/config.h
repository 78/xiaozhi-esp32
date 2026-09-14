#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Audio: ES8311 codec + FM8002E amplifier + on-board MEMS microphone
#define AUDIO_I2S_GPIO_MCLK      GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK      GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN       GPIO_NUM_6
#define AUDIO_I2S_GPIO_WS        GPIO_NUM_7
#define AUDIO_I2S_GPIO_DOUT      GPIO_NUM_8
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_1  // Active low

// I2C bus shared by the ES8311 codec, the FT6336G touch panel and the
// external 1.25mm 4P I2C header.
#define AUDIO_CODEC_I2C_NUM      I2C_NUM_0
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_15
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_16
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO         GPIO_NUM_0
#define BUILTIN_LED_GPIO         GPIO_NUM_42

// Battery voltage divider (2x 200k) on ADC1 channel 8 == GPIO9
#define BATTERY_ADC_UNIT         ADC_UNIT_1
#define BATTERY_ADC_CHANNEL      ADC_CHANNEL_8
#define BATTERY_UPPER_RESISTOR   200000
#define BATTERY_LOWER_RESISTOR   200000

// Display: 2.8" IPS TFT, ILI9341V over 4-line SPI.
// The panel reset is tied to the module EN net, so it has no dedicated GPIO.
#define LCD_SPI_HOST             SPI3_HOST
#define DISPLAY_SPI_SCLK_HZ      (40 * 1000 * 1000)
#define DISPLAY_SPI_MODE         0
#define DISPLAY_CS_PIN           GPIO_NUM_10
#define DISPLAY_DC_PIN           GPIO_NUM_46
#define DISPLAY_SCK_PIN          GPIO_NUM_12
#define DISPLAY_MOSI_PIN         GPIO_NUM_11
#define DISPLAY_MISO_PIN         GPIO_NUM_13
#define DISPLAY_RST_PIN          GPIO_NUM_NC
#define DISPLAY_BACKLIGHT_PIN    GPIO_NUM_45
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define LCD_TYPE_ILI9341_SERIAL
#define DISPLAY_WIDTH            240
#define DISPLAY_HEIGHT           320
#define DISPLAY_MIRROR_X         true
#define DISPLAY_MIRROR_Y         false
#define DISPLAY_SWAP_XY          false
#define DISPLAY_INVERT_COLOR     true
#define DISPLAY_RGB_ORDER        LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X         0
#define DISPLAY_OFFSET_Y         0

// Touch: FT6336G capacitive controller on the shared I2C bus
#define TOUCH_RST_PIN            GPIO_NUM_18
#define TOUCH_INT_PIN            GPIO_NUM_17

#endif  // _BOARD_CONFIG_H_
