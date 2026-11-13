#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// StackChan: Shinya Ishikawa's desktop robot, built on an M5Stack CoreS3.
// The CoreS3 pins below match m5stack/core-s3; everything under "StackChan base" is the robot's
// own hardware - the servo neck and the PY32 expander that powers it.

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
#define CAMERA_PIN_XCLK  GPIO_NUM_NC // 像素时钟 (固定由 20MHz 外部晶振输入) 
#define CAMERA_PIN_SIOD GPIO_NUM_NC  // 串行时钟 Using existing I2C port
#define CAMERA_PIN_SIOC GPIO_NUM_NC  // 串行时钟 Using existing I2C port
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

 


// ---------------------------------------------------------------- StackChan base

// The servo neck: two Feetech SCS0009 on one half-duplex serial bus.
#define SERVO_UART_NUM UART_NUM_1
#define SERVO_BAUDRATE 1000000
#define SERVO_TX_PIN 6
#define SERVO_RX_PIN 7
#define SERVO_YAW_ID 1
#define SERVO_PITCH_ID 2

// Pitch is the axis that can drive into a mechanical end stop, so it is clamped twice: once
// where an angle arrives from a tool call, and again at the servo-write boundary. Yaw has no
// such limit on this neck.
#define SAFE_PITCH_MIN 5
#define SAFE_PITCH_MAX 85

// Where the head is sent once boot has secured the position it powered up in.
#define BOOT_INIT_YAW_DEG 0
// Level is about 5 on this neck, not 0 and not 45. The servo's own zero sits below the
// horizon and its travel is upward only, so 0 points at the desk; 45 is roughly where the
// head rests before power is applied, which makes it an easy value to mistake for neutral and
// leaves the robot staring above the horizon.
#define BOOT_INIT_PITCH_DEG 5

#endif // _BOARD_CONFIG_H_
