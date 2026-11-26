#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16
#else
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_7
#endif

#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

// Display SPI Pin Configuration
#define DISPLAY_SPI_HOST        SPI3_HOST
#define DISPLAY_MOSI_PIN        GPIO_NUM_47
#define DISPLAY_CLK_PIN         GPIO_NUM_21
#define DISPLAY_DC_PIN          GPIO_NUM_40
#define DISPLAY_RST_PIN         GPIO_NUM_45
#define DISPLAY_CS_PIN          GPIO_NUM_41
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_NC

// ILI9341 240x320 Display Configuration (Non-IPS Default)
#if defined(CONFIG_LCD_ILI9341_240X320)
#define LCD_TYPE_ILI9341_SERIAL
#define DISPLAY_WIDTH                   240
#define DISPLAY_HEIGHT                  320
#define DISPLAY_MIRROR_X                true
#define DISPLAY_MIRROR_Y                false
#define DISPLAY_SWAP_XY                 false
#define DISPLAY_INVERT_COLOR            true
#define DISPLAY_RGB_ORDER               LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X                0
#define DISPLAY_OFFSET_Y                0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE                0
#else
// Default fallback to ILI9341 240x320 Non-IPS
#define LCD_TYPE_ILI9341_SERIAL
#define DISPLAY_WIDTH                   240
#define DISPLAY_HEIGHT                  320
#define DISPLAY_MIRROR_X                true
#define DISPLAY_MIRROR_Y                false
#define DISPLAY_SWAP_XY                 false
#define DISPLAY_INVERT_COLOR            false
#define DISPLAY_RGB_ORDER               LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X                0
#define DISPLAY_OFFSET_Y                0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE                0
#endif

// Optional MCP Lamp Controller
#define LAMP_GPIO GPIO_NUM_18

#endif // _BOARD_CONFIG_H_

