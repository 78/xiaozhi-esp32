#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ============================================================================
// FoloToy AI Passport board definition for XiaoZhi.
//
// Hardware facts come from the ai-passport repository BSP
// (components/bsp/include/bsp_pins.h + hardware development guide):
// ESP32-C3, 8 MB flash, no PSRAM, USB Serial/JTAG console.
// ============================================================================

// Audio sample rates. Match the FoloToy reference board: 24 kHz avoids
// server-side resampling mismatch (server sends 24 kHz audio).
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2S full-duplex to ES8311 (shared MCLK/BCLK/WS).
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_6
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_3
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_4   // codec -> MCU (record)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_2   // MCU -> codec (play)

// Amplifier enable is not wired to the MCU (always enabled on Passport).
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_10
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_7
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR  // 0x18

// ============================================================================
// Buttons: UP/DOWN/OK share one ADC pin (GPIO0 / ADC1_CH0) through a resistor
// ladder with an external 10 kOhm pull-up to 3.3 V. Voltage windows:
//   UP   ~0 mV      (0-150)
//   DOWN ~300 mV    (150-447)
//   OK   ~595 mV    (447-1900)
//   released ~3300 mV
// ============================================================================
#define BOOT_BUTTON_GPIO   GPIO_NUM_NC   // no dedicated digital key

// ADC windows for the three ladder keys (kept next to the board pins).
#define BSP_ADC_BUTTON_UP_MIN    0
#define BSP_ADC_BUTTON_UP_MAX    150
#define BSP_ADC_BUTTON_DOWN_MIN  150
#define BSP_ADC_BUTTON_DOWN_MAX  447
#define BSP_ADC_BUTTON_OK_MIN    447
#define BSP_ADC_BUTTON_OK_MAX    1900

// ============================================================================
// Display: ST7789 (ST7789P3) 240x320 portrait, 4-line SPI.
// MOSI-only (no MISO), reset is a software reset (RST not wired).
// ============================================================================
#define DISPLAY_SPI_SCK_PIN     GPIO_NUM_8
#define DISPLAY_SPI_MOSI_PIN    GPIO_NUM_9
#define DISPLAY_DC_PIN          GPIO_NUM_20
#define DISPLAY_SPI_CS_PIN      GPIO_NUM_1
#define DISPLAY_RST_PIN         GPIO_NUM_NC

#define DISPLAY_WIDTH    240
#define DISPLAY_HEIGHT   320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false   // portrait

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_BACKLIGHT_PIN          GPIO_NUM_21
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define DISPLAY_INVERT_COLOR true   // this panel ships inverted (needs INVON)

// CW2017 fuel gauge shares the codec I2C bus.
#define BATTERY_CW2017_ADDR 0x63

#endif // _BOARD_CONFIG_H_
