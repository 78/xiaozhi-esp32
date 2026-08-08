#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// M5Stack StackChan (K151) - CoreS3 host + servo body

#include <driver/gpio.h>

#define AUDIO_INPUT_REFERENCE    false
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_0
#define AUDIO_I2S_GPIO_WS GPIO_NUM_33
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_34
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_14
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_13

#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_12
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_11
#define AUDIO_CODEC_AW88298_ADDR AW88298_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_SDA_PIN GPIO_NUM_NC
#define DISPLAY_SCL_PIN GPIO_NUM_NC
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_NC
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

/* Camera pins */
#define CAMERA_PIN_PWDN GPIO_NUM_NC
#define CAMERA_PIN_RESET GPIO_NUM_NC
#define CAMERA_PIN_XCLK  GPIO_NUM_NC // driven by a fixed 20MHz external crystal
#define CAMERA_PIN_SIOD GPIO_NUM_NC  // Using existing I2C port
#define CAMERA_PIN_SIOC GPIO_NUM_NC  // Using existing I2C port
#define CAMERA_PIN_D0 GPIO_NUM_39
#define CAMERA_PIN_D1 GPIO_NUM_40
#define CAMERA_PIN_D2 GPIO_NUM_41
#define CAMERA_PIN_D3 GPIO_NUM_42
#define CAMERA_PIN_D4 GPIO_NUM_15
#define CAMERA_PIN_D5 GPIO_NUM_16
#define CAMERA_PIN_D6 GPIO_NUM_48
#define CAMERA_PIN_D7 GPIO_NUM_47
#define CAMERA_PIN_VSYNC GPIO_NUM_46
#define CAMERA_PIN_HREF GPIO_NUM_38
#define CAMERA_PIN_PCLK GPIO_NUM_45

#define XCLK_FREQ_HZ 20000000

/* Servo body: Feetech SCS0009 serial bus servos on a dedicated UART */
#define SERVO_UART_PORT      UART_NUM_1
#define SERVO_UART_BAUD_RATE 1000000
#define SERVO_UART_TX_PIN    GPIO_NUM_6
#define SERVO_UART_RX_PIN    GPIO_NUM_7

#define SERVO_YAW_ID            1
#define SERVO_YAW_ZERO_POS      460
#define SERVO_PITCH_ID          2
#define SERVO_PITCH_ZERO_POS    620

// Raw encoder range shared by both servos.
#define SERVO_RAW_POS_MIN 0
#define SERVO_RAW_POS_MAX 1000

// Angles are in degrees, relative to the calibrated home position.
// Pitch 0 is level; M5Stack warns that the vertical servo stalls near its
// mechanical limits, so the usable travel is capped below 90 degrees.
#define SERVO_YAW_MIN_DEGREE   (-128)
#define SERVO_YAW_MAX_DEGREE   128
#define SERVO_PITCH_MIN_DEGREE 0
#define SERVO_PITCH_MAX_DEGREE 85

// raw = zero_pos + degree * SERVO_STEPS_PER_DEGREE_NUM / SERVO_STEPS_PER_DEGREE_DEN
// One encoder step is 0.3125 degree.
#define SERVO_STEPS_PER_DEGREE_NUM 16
#define SERVO_STEPS_PER_DEGREE_DEN 5

/* Body IO expander (PY32L020) on the CoreS3 internal I2C bus */
#define BODY_IO_EXPANDER_ADDR 0x6F
#define BODY_IO_PIN_VM_EN     0
#define BODY_IO_PIN_RGB_EN    13
#define BODY_RGB_LED_COUNT    12

#endif // _BOARD_CONFIG_H_
