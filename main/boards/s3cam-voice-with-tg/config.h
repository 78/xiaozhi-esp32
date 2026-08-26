#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Microphone input gain applied after the >>12 conversion (1x matches the known-good build)
#define AUDIO_INPUT_GAIN         1.0f

// I2S Microphone (INMP441) & Amp (MAX98357A) Simplex Configuration
#define AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_1
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_2
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_42

#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_39
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_40
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_41

// Display ST7735S TFT (SPI - 128x160)
#define DISPLAY_MOSI_PIN        GPIO_NUM_20   // SDA (MOSI) on the TFT
#define DISPLAY_CLK_PIN         GPIO_NUM_19   // SCK on the TFT
#define DISPLAY_DC_PIN          GPIO_NUM_47   // A0 (DC) on the TFT
#define DISPLAY_CS_PIN          GPIO_NUM_45   // CS on the TFT
#define DISPLAY_RST_PIN         GPIO_NUM_21   // RESET on the TFT
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_38   // LED on the TFT
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_WIDTH           128
#define DISPLAY_HEIGHT          160
#define DISPLAY_SPI_MODE        0
#define DISPLAY_OFFSET_X        0
#define DISPLAY_OFFSET_Y        0
#define DISPLAY_MIRROR_X        true
#define DISPLAY_MIRROR_Y        true
#define DISPLAY_SWAP_XY         false
#define DISPLAY_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_INVERT_COLOR    true

// Relay Control removed

// Camera OV3660 Configuration (auto-detected, compatible with OV2640 pinout)
#define CAMERA_PIN_PWDN         -1
#define CAMERA_PIN_RESET        -1
#define CAMERA_PIN_XCLK         GPIO_NUM_15
#define CAMERA_PIN_SIOD         GPIO_NUM_4
#define CAMERA_PIN_SIOC         GPIO_NUM_5
#define CAMERA_PIN_D7           GPIO_NUM_16
#define CAMERA_PIN_D6           GPIO_NUM_17
#define CAMERA_PIN_D5           GPIO_NUM_18
#define CAMERA_PIN_D4           GPIO_NUM_10
#define CAMERA_PIN_D3           GPIO_NUM_8
#define CAMERA_PIN_D2           GPIO_NUM_9
#define CAMERA_PIN_D1           GPIO_NUM_11
#define CAMERA_PIN_D0           GPIO_NUM_12
#define CAMERA_PIN_VSYNC        GPIO_NUM_6
#define CAMERA_PIN_HREF         GPIO_NUM_7
#define CAMERA_PIN_PCLK         GPIO_NUM_13
#define XCLK_FREQ_HZ            20000000

// Buttons & LEDs
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define BUILTIN_LED_GPIO        GPIO_NUM_48

// Telegram Credentials Default (Override via NVS)
#define TELEGRAM_BOT_TOKEN      "YOUR_TELEGRAM_BOT_TOKEN_HERE"
#define TELEGRAM_CHAT_ID        "YOUR_TELEGRAM_CHAT_ID_HERE"

#endif // _BOARD_CONFIG_H_
