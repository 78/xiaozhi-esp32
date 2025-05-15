#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define BSP_IO_EXPANDER_I2C_ADDRESS     ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000
#define EXAMPLE_LCD_IO_SPI_CS           IO_EXPANDER_PIN_NUM_1
#define EXAMPLE_LCD_IO_SPI_SCK          IO_EXPANDER_PIN_NUM_2
#define EXAMPLE_LCD_IO_SPI_SDO          IO_EXPANDER_PIN_NUM_3
#define EXAMPLE_LCD_IO_RST              -1
#define EXP_GPIO_START_NUM              100

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE    true

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_WS GPIO_NUM_7
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_16
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_15
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_6

#define AUDIO_CODEC_PA_PIN       EXP_GPIO_START_NUM + IO_EXPANDER_PIN_NUM_0
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_47
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_48
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define CONFIG_EXAMPLE_USE_DOUBLE_FB    1
#if CONFIG_EXAMPLE_USE_DOUBLE_FB
#define EXAMPLE_LCD_NUM_FB              2
#else
#define EXAMPLE_LCD_NUM_FB              1
#endif // CONFIG_EXAMPLE_USE_DOUBLE_FB

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Refresh Rate = 16000000/(5+30+10+480)/(5+20+20+480) = 58Hz
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (16 * 1000 * 1000)
#define EXAMPLE_LCD_H_RES              DISPLAY_WIDTH
#define EXAMPLE_LCD_V_RES              DISPLAY_HEIGHT
#define EXAMPLE_LCD_HSYNC              5
#define EXAMPLE_LCD_HBP                30
#define EXAMPLE_LCD_HFP                10
#define EXAMPLE_LCD_VSYNC              5
#define EXAMPLE_LCD_VBP                20
#define EXAMPLE_LCD_VFP                20
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  480

#define EXAMPLE_LCD_BIT_PER_PIXEL               (18)
#define EXAMPLE_RGB_BIT_PER_PIXEL               (16)
#define EXAMPLE_RGB_DATA_WIDTH                  (16)
#define EXAMPLE_LCD_DMA_SZIE                    64
#define EXAMPLE_LCD_RGB_BOUNCE_BUFFER_HEIGHT    20
#define EXAMPLE_RGB_BOUNCE_BUFFER_SIZE          (DISPLAY_HEIGHT * EXAMPLE_LCD_RGB_BOUNCE_BUFFER_HEIGHT)

#define EXAMPLE_LCD_IO_RGB_DE                   17
#define EXAMPLE_LCD_IO_RGB_PCLK                 9
#define EXAMPLE_LCD_IO_RGB_VSYNC                3
#define EXAMPLE_LCD_IO_RGB_HSYNC                46
#define EXAMPLE_LCD_IO_RGB_DISP                 -1

#define EXAMPLE_LCD_IO_RGB_DATA0           1
#define EXAMPLE_LCD_IO_RGB_DATA1           2
#define EXAMPLE_LCD_IO_RGB_DATA2           42
#define EXAMPLE_LCD_IO_RGB_DATA3           41
#define EXAMPLE_LCD_IO_RGB_DATA4           40

#define EXAMPLE_LCD_IO_RGB_DATA5           39
#define EXAMPLE_LCD_IO_RGB_DATA6           38
#define EXAMPLE_LCD_IO_RGB_DATA7           45
#define EXAMPLE_LCD_IO_RGB_DATA8           21
#define EXAMPLE_LCD_IO_RGB_DATA9           14

#define EXAMPLE_LCD_IO_RGB_DATA10          13
#define EXAMPLE_LCD_IO_RGB_DATA11          12
#define EXAMPLE_LCD_IO_RGB_DATA12          11
#define EXAMPLE_LCD_IO_RGB_DATA13          10
#define EXAMPLE_LCD_IO_RGB_DATA14          8
#define EXAMPLE_LCD_IO_RGB_DATA15          18


#endif // _BOARD_CONFIG_H_
