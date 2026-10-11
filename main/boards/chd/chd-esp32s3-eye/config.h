#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE     16000
#define AUDIO_OUTPUT_SAMPLE_RATE    16000

#define AUDIO_I2S_GPIO_MCLK         GPIO_NUM_2
#define AUDIO_I2S_GPIO_WS           GPIO_NUM_45
#define AUDIO_I2S_GPIO_BCLK         GPIO_NUM_42
#define AUDIO_I2S_GPIO_DIN          GPIO_NUM_41
#define AUDIO_I2S_GPIO_DOUT         GPIO_NUM_46

#define AUDIO_CODEC_PA_PIN          GPIO_NUM_40
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_4
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_5
#define AUDIO_CODEC_ES8311_ADDR     ES8311_CODEC_DEFAULT_ADDR

#define BUILTIN_LED_GPIO            GPIO_NUM_NC
#define BOOT_BUTTON_GPIO            GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO     GPIO_NUM_NC

#define DISPLAY_WIDTH               240
#define DISPLAY_HEIGHT              240
#define DISPLAY_MIRROR_X            false
#define DISPLAY_MIRROR_Y            false
#define DISPLAY_SWAP_XY             false

#define DISPLAY_OFFSET_X            0
#define DISPLAY_OFFSET_Y            0

#define DISPLAY_DC_GPIO             GPIO_NUM_43
#define DISPLAY_CS_GPIO             GPIO_NUM_44
#define DISPLAY_CLK_GPIO            GPIO_NUM_21
#define DISPLAY_MOSI_GPIO           GPIO_NUM_47
#define DISPLAY_RST_GPIO            GPIO_NUM_NC

#define DISPLAY_BACKLIGHT_PIN       GPIO_NUM_48
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

/* Camera PINs*/
#define CAMERA_PIN_XCLK             (GPIO_NUM_15)
#define CAMERA_PIN_PCLK             (GPIO_NUM_13)
#define CAMERA_PIN_VSYNC            (GPIO_NUM_6)
#define CAMERA_PIN_HSYNC            (GPIO_NUM_7)
#define CAMERA_PIN_D0               (GPIO_NUM_11)
#define CAMERA_PIN_D1               (GPIO_NUM_9)
#define CAMERA_PIN_D2               (GPIO_NUM_8)
#define CAMERA_PIN_D3               (GPIO_NUM_10)
#define CAMERA_PIN_D4               (GPIO_NUM_12)
#define CAMERA_PIN_D5               (GPIO_NUM_18)
#define CAMERA_PIN_D6               (GPIO_NUM_17)
#define CAMERA_PIN_D7               (GPIO_NUM_16)

#define CAMERA_PIN_PWDN             (GPIO_NUM_NC)
#define CAMERA_PIN_RESET            (GPIO_NUM_NC)
#define CAMERA_PIN_XCLK             (GPIO_NUM_15)
#define CAMERA_PIN_PCLK             (GPIO_NUM_13)
#define CAMERA_PIN_VSYNC            (GPIO_NUM_6)
#define CAMERA_PIN_HSYNC            (GPIO_NUM_7)

#define CAMERA_XCLK_FREQ            (16000000)

/* I2C */
#define BSP_I2C_PORT                (1)
#define BSP_I2C_SCL                 (-1) // (GPIO_NUM_18)
#define BSP_I2C_SDA                 (-1) // (GPIO_NUM_8)

#endif // _BOARD_CONFIG_H_
