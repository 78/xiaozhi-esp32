#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Audio Pins (I2S - Same as Zhengchen 1.54)
#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

// Buttons
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define AUDIO_WAKE_BUTTON_GPIO GPIO_NUM_11

// Display (ST7789 SPI)
#define DISPLAY_SDA GPIO_NUM_41
#define DISPLAY_SCL GPIO_NUM_42
#define DISPLAY_RES GPIO_NUM_45
#define DISPLAY_DC GPIO_NUM_40
#define DISPLAY_CS GPIO_NUM_21

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_SWAP_XY true
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y true
#define BACKLIGHT_INVERT false
#define DISPLAY_OFFSET_X 80
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_20
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// Servos (Legs) - Moved to accommodate Audio
#define FL_GPIO_NUM GPIO_NUM_17
#define FR_GPIO_NUM GPIO_NUM_18
#define BL_GPIO_NUM GPIO_NUM_39
#define BR_GPIO_NUM GPIO_NUM_38 

// Servo (Tail) - Moved to accommodate Audio   
#define TAIL_GPIO_NUM GPIO_NUM_12               

// NOTE: Bluetooth KCX_BT_EMITTER pins are now configured via Kconfig menuconfig:
//   - CONFIG_BLUETOOTH_CONNECT_PIN (default: GPIO 3)
//   - CONFIG_BLUETOOTH_LINK_PIN (default: GPIO 46)
// Enable bluetooth module: CONFIG_ENABLE_BLUETOOTH_MODULE=y

#endif // _BOARD_CONFIG_H_
