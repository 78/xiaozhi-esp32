#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE         24000
#define AUDIO_OUTPUT_SAMPLE_RATE        24000

#define AUDIO_I2S_MIC_GPIO_WS           GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK          GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN          GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT         GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK         GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK         GPIO_NUM_16
        
#define BUILTIN_LED_GPIO                GPIO_NUM_2
#define BUILTIN_LED_NUM                 12
#define TOUCH_BUTTON_GPIO               GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO           GPIO_NUM_1
#define VOLUME_DOWN_BUTTON_GPIO         GPIO_NUM_43


#define DISPLAY_LCD_PIXEL_CLOCK_HZ      12000000// CONFIG_DISPLAY_LCD_PIXEL_CLOCK_HZ

#define DISPLAY_PIN_NUM_DATA0           GPIO_NUM_14
#define DISPLAY_PIN_NUM_DATA1           GPIO_NUM_21
#define DISPLAY_PIN_NUM_DATA2           GPIO_NUM_47
#define DISPLAY_PIN_NUM_DATA3           GPIO_NUM_48
#define DISPLAY_PIN_NUM_DATA4           GPIO_NUM_45
#define DISPLAY_PIN_NUM_DATA5           GPIO_NUM_38
#define DISPLAY_PIN_NUM_DATA6           GPIO_NUM_39
#define DISPLAY_PIN_NUM_DATA7           GPIO_NUM_40

#define DISPLAY_PIN_NUM_PCLK            GPIO_NUM_8
#define DISPLAY_PIN_NUM_CS              GPIO_NUM_NC
#define DISPLAY_PIN_NUM_DC              GPIO_NUM_13
#define DISPLAY_PIN_NUM_RST             GPIO_NUM_NC

#define DISPLAY_LCD_CMD_BITS            8
#define DISPLAY_LCD_PARAM_BITS          8


#define DISPLAY_DMA_BURST_SIZE          64 // 16, 32, 64. Higher burst size can improve the performance when the DMA buffer comes from PSRAM

#define DISPLAY_SDA_PIN                 GPIO_NUM_18
#define DISPLAY_SCL_PIN                 GPIO_NUM_17

#define DISPLAY_WIDTH                   240
#define DISPLAY_HEIGHT                  280
#define DISPLAY_MIRROR_X                false
#define DISPLAY_MIRROR_Y                false
#define DISPLAY_SWAP_XY                 false

#define DISPLAY_OFFSET_X                0
#define DISPLAY_OFFSET_Y                15

#define DISPLAY_BACKLIGHT_PIN           GPIO_NUM_44
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define ML307_RX_PIN                    GPIO_NUM_42
#define ML307_TX_PIN                    GPIO_NUM_41

#define MODE_WIFI                       0
#define MODE_4G                         1


#endif // _BOARD_CONFIG_H_
