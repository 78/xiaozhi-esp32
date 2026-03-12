#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Generic ESP32-S3-DevKitC-1 baseline profile (Mark One)
// Adjust pins below to match your actual wiring.

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Simplex I2S/PDM style mapping used by NoAudioCodecSimplexPdm
#define AUDIO_I2S_MIC_GPIO_SCK   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_DIN   GPIO_NUM_5
#define AUDIO_I2S_SPK_GPIO_DOUT  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_BCLK  GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_LRCK  GPIO_NUM_15

// Common defaults for ESP32-S3 DevKit boards
#define BUILTIN_LED_GPIO         GPIO_NUM_48
#define BOOT_BUTTON_GPIO         GPIO_NUM_0
#define TOUCH_BUTTON_GPIO        GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO    GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO  GPIO_NUM_NC
#define RESET_NVS_BUTTON_GPIO     GPIO_NUM_NC
#define RESET_FACTORY_BUTTON_GPIO GPIO_NUM_NC

// 128x64 OLED (I2C)
#define DISPLAY_SDA_PIN GPIO_NUM_41
#define DISPLAY_SCL_PIN GPIO_NUM_42
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false

// Display controller selection:
// 1 = SH1106, 0 = SSD1306
#define DISPLAY_USE_SH1106 1

// Robot arm servos (PWM via LEDC)
// Keep separate from OLED I2C pins.
#define SERVO1_GPIO GPIO_NUM_39
#define SERVO2_GPIO GPIO_NUM_40

#endif // _BOARD_CONFIG_H_
