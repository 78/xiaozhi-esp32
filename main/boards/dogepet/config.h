#ifndef _DOGEPET_CONFIG_H_
#define _DOGEPET_CONFIG_H_

#include <driver/gpio.h>

// Flash/PSRAM info (documentary; sizing is from sdkconfig)
// 4MB flash, 2MB PSRAM (QUAD)

// Audio sample rates for duplex I2S
// IMPORTANT: Must match for duplex mode with shared clock pins (like DogePet)
#define AUDIO_INPUT_REFERENCE    true
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
// This matches DogePet's working configuration
#define AUDIO_I2S_METHOD_DUPLEX

#ifdef AUDIO_I2S_METHOD_DUPLEX

// Shared I2S clock pins for both microphone and speaker
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_NC
#define AUDIO_I2S_GPIO_WS GPIO_NUM_16   // Shared WS/LRCLK
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_17 // Shared BCLK
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_13 // INMP441 SD pin
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_33 // MAX98357A DIN pin

#define AUDIO_PA_CTRL_GPIO      GPIO_NUM_NC  // PA power control (optional)
#define AUDIO_CODEC_PA_PIN      GPIO_NUM_NC  // Same as PA_CTRL for compatibility
#endif

// Buttons
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define BUTTON_A_GPIO           GPIO_NUM_40
#define BUTTON_B_GPIO           GPIO_NUM_41
#define BUTTON_C_GPIO           GPIO_NUM_39

// LED
#define BUILTIN_LED_GPIO        GPIO_NUM_48

// IMU removed to save space

// VBAT ADC (optional). If connected through divider to a GPIO with ADC1 channel,
// you can wire it here. Example uses ADC1_CH0 on GPIO1 or customize as needed.
#define VBAT_ADC_UNIT   ADC_UNIT_2
#define VBAT_ADC_CH     ADC_CHANNEL_4  // GPIO15 on ESP32-S3
#define VBAT_UPPER_R    10000.0f
#define VBAT_LOWER_R    10000.0f
/* No charge detect pin for now */

// SPI TFT ST7789 1.54" 240x240
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

// Pins (fits S3 SuperMini + common ST7789 boards)
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_5
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_MOSI_PIN      GPIO_NUM_2
#define DISPLAY_MISO_PIN      GPIO_NUM_4
#define DISPLAY_CLK_PIN       GPIO_NUM_3
#define DISPLAY_DC_PIN        GPIO_NUM_7
#define DISPLAY_RST_PIN       GPIO_NUM_NC
#define DISPLAY_CS_PIN        GPIO_NUM_6

// SD card (optional, same SPI bus)
#define SD_CARD_CS_PIN        GPIO_NUM_8

// Kconfig-selected LCD variants
#ifdef CONFIG_LCD_ST7789_240X240
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_INVERT_COLOR true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_SPI_MODE 0
#endif

/* ST7789 7-pin panels typically require SPI mode 3 */
#ifdef CONFIG_LCD_ST7789_240X240_7PIN
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_INVERT_COLOR true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_SPI_MODE 3
#endif


#ifdef CONFIG_LCD_GC9A01_240X240
#define LCD_TYPE_GC9A01_SERIAL
#define DISPLAY_INVERT_COLOR true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_SPI_MODE 0
#endif

#endif // _DOGEPET_CONFIG_H_
