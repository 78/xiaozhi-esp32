#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

// I2S pins for MAX98357A (output) and INMP441 (input)
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_4   // Bit Clock
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_5   // Word Select (LRC)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_6   // Data Out to MAX98357A
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_7   // Data In from INMP441

// MAX98357A gain control (optional, can be tied to VDD/GND for fixed gain)
#define MAX98357A_SD_MODE_PIN GPIO_NUM_8  // Shutdown/Gain control

// Built-in LED and button
#define BUILTIN_LED_GPIO     GPIO_NUM_2
#define BOOT_BUTTON_GPIO     GPIO_NUM_9

// Display configuration (SSD1306 OLED)
#define DISPLAY_I2C_SDA_PIN  GPIO_NUM_10
#define DISPLAY_I2C_SCL_PIN  GPIO_NUM_3
#define DISPLAY_WIDTH        128
#define DISPLAY_HEIGHT       64
#define DISPLAY_MIRROR_X     true
#define DISPLAY_MIRROR_Y     true

#endif // _BOARD_CONFIG_H_
