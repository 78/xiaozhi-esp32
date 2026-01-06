#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_PDM_UPSAMPLE_FS    480

#define AUDIO_ADC_MIC_CHANNEL       5
#define AUDIO_PDM_SPEAK_P_GPIO      GPIO_NUM_7
#define AUDIO_PDM_SPEAK_N_GPIO      GPIO_NUM_8
#define AUDIO_PA_CTL_GPIO           GPIO_NUM_1

#define BOOT_BUTTON_GPIO            GPIO_NUM_28
#define DISPLAY_MOSI_PIN            GPIO_NUM_23
#define DISPLAY_CLK_PIN             GPIO_NUM_24
#define DISPLAY_DC_PIN              GPIO_NUM_26
#define DISPLAY_RST_PIN             GPIO_NUM_NC
#define DISPLAY_CS_PIN              GPIO_NUM_25

#define LCD_TP_SCL GPIO_NUM_3
#define LCD_TP_SDA GPIO_NUM_2

#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH           284
#define DISPLAY_HEIGHT          240
#define DISPLAY_MIRROR_X        false
#define DISPLAY_MIRROR_Y        true
#define DISPLAY_SWAP_XY         true

#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X        36
#define DISPLAY_OFFSET_Y        0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE        0

#endif // _BOARD_CONFIG_H_
