#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Audio (board has ES8311/PA, but app can run with dummy codec)
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Buttons / LEDs
#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

// Optional LCD via MIPI-DSI adapter (7" 1024x600)
#define DISPLAY_WIDTH    1024
#define DISPLAY_HEIGHT   600
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

// LCD adapter wiring (per Espressif user guide):
// RST_LCD -> GPIO27, PWM(backlight) -> GPIO26
#define PIN_NUM_LCD_RST                 GPIO_NUM_27
#define DISPLAY_BACKLIGHT_PIN           GPIO_NUM_26
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// MIPI DSI PHY supply (used by other P4 boards)
#define MIPI_DSI_PHY_PWR_LDO_CHAN          (3)
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV    (2500)

#endif // _BOARD_CONFIG_H_