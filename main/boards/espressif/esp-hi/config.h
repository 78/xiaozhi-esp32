#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 配置PDM上采样fs参数（取值范围<=480）。部分设备在441时表现更稳定
#define AUDIO_PDM_UPSAMPLE_FS    441

#define AUDIO_ADC_MIC_CHANNEL       2
#define AUDIO_PDM_SPEAK_P_GPIO      GPIO_NUM_6
#define AUDIO_PDM_SPEAK_N_GPIO      GPIO_NUM_7
#define AUDIO_PA_CTL_GPIO           GPIO_NUM_3

#define BUILTIN_LED_GPIO            GPIO_NUM_NC
#define BOOT_BUTTON_GPIO            GPIO_NUM_9
#define MOVE_WAKE_BUTTON_GPIO       GPIO_NUM_0
#define AUDIO_WAKE_BUTTON_GPIO      GPIO_NUM_1

#define DISPLAY_MOSI_PIN            GPIO_NUM_4
#define DISPLAY_CLK_PIN             GPIO_NUM_5
#define DISPLAY_DC_PIN              GPIO_NUM_10
#define DISPLAY_RST_PIN             GPIO_NUM_NC
#define DISPLAY_CS_PIN              GPIO_NUM_NC

#define FL_GPIO_NUM                 GPIO_NUM_21
#define FR_GPIO_NUM                 GPIO_NUM_19
#define BL_GPIO_NUM                 GPIO_NUM_20
#define BR_GPIO_NUM                 GPIO_NUM_18

#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH           160
#define DISPLAY_HEIGHT          80
#define DISPLAY_MIRROR_X        false
#define DISPLAY_MIRROR_Y        true
#define DISPLAY_SWAP_XY         true

#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X        0
#define DISPLAY_OFFSET_Y        0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE        0

#endif // _BOARD_CONFIG_H_
