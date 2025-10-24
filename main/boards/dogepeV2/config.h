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
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_10  // Shared WS/LRCLK (match DogePet V1)
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_12   // Shared BCLK (match DogePet V1)
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_13   // INMP441 SD pin (outputs on RIGHT channel when L/R=GND)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_11  // MAX98357A DIN pin (match DogePet V1)

#define AUDIO_PA_CTRL_GPIO      GPIO_NUM_NC  // PA power control (optional)
#define AUDIO_CODEC_PA_PIN      GPIO_NUM_NC  // Same as PA_CTRL for compatibility
#endif

// Buttons
// - BOOT button kept on GPIO0 (wake + long-press Wi‑Fi config)
// - Conversation button: dedicated pin to start/stop AI
#define BOOT_BUTTON_GPIO            GPIO_NUM_0
#define CONVERSATION_BUTTON_GPIO    GPIO_NUM_1

// LED
#define BUILTIN_LED_GPIO        GPIO_NUM_48


// Pins (fits S3 SuperMini + common ST7789 boards)
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_7
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_MOSI_PIN      GPIO_NUM_2
#define DISPLAY_MISO_PIN      GPIO_NUM_NC
#define DISPLAY_CLK_PIN       GPIO_NUM_3
#define DISPLAY_DC_PIN        GPIO_NUM_4
#define DISPLAY_RST_PIN       GPIO_NUM_5
#define DISPLAY_CS_PIN        GPIO_NUM_6


// Kconfig-selected LCD variants
#ifdef CONFIG_LCD_ST7789_240X240
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_INVERT_COLOR true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_SPI_MODE 0
#endif

#ifdef CONFIG_LCD_ST7789_240X280
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   280
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X  20
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0
#endif

#endif // _DOGEPET_CONFIG_H_
