#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/ledc.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 如果使用 Duplex I2S 模式，请注释下面一行
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_3
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_7

#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_11
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_12
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_13

#else

#define AUDIO_I2S_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7

#endif


#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39

#define DISPLAY_SDA_PIN GPIO_NUM_5
#define DISPLAY_SCL_PIN GPIO_NUM_6
#define DISPLAY_WIDTH   128

#if CONFIG_OLED_SSD1306_128X32
#define DISPLAY_HEIGHT  32
#elif CONFIG_OLED_SSD1306_128X64
#define DISPLAY_HEIGHT  64
#elif CONFIG_OLED_SH1106_128X64
#define DISPLAY_HEIGHT  64
#define SH1106
#else
#error "OLED display type is not selected"
#endif

#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true

// Tower Pro SG90 9g analog servos (pan/tilt).
// Datasheet: 50 Hz period (20 ms), 1000 us = 0°, 1500 us = 90°, 2000 us = 180°.
// Do not use 500–2500 us here: that over-travels cheap SG90s and makes them buzz/stall.
// GPIO 8/9 are free vs I2S (3,4,7,11,12,13) and OLED I2C (5,6).
#define SERVO_PAN_GPIO          GPIO_NUM_8
#define SERVO_TILT_GPIO         GPIO_NUM_9
#define SERVO_PAN_LEDC_CHANNEL  LEDC_CHANNEL_0
#define SERVO_TILT_LEDC_CHANNEL LEDC_CHANNEL_1
#define SERVO_LEDC_TIMER        LEDC_TIMER_1
#define SERVO_PWM_FREQ_HZ       50
#define SERVO_MIN_PULSE_US      1000
#define SERVO_MAX_PULSE_US      2000
#define SERVO_PAN_MIN           30
#define SERVO_PAN_MAX           150
#define SERVO_TILT_MIN          45
#define SERVO_TILT_MAX          135
#define SERVO_PAN_HOME          90
#define SERVO_TILT_HOME         90

// A MCP Test: Control a lamp
#define LAMP_GPIO GPIO_NUM_18

#endif // _BOARD_CONFIG_H_
