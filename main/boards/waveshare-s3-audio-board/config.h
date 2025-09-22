#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>

#define AUDIO_INPUT_SAMPLE_RATE     24000
#define AUDIO_OUTPUT_SAMPLE_RATE    24000

#define BOOT_BUTTON_GPIO            GPIO_NUM_0
#define BUILTIN_LED_GPIO            GPIO_NUM_38

#define AUDIO_I2S_GPIO_MCLK         GPIO_NUM_12
#define AUDIO_I2S_GPIO_BCLK         GPIO_NUM_13
#define AUDIO_I2S_GPIO_WS           GPIO_NUM_14
#define AUDIO_I2S_GPIO_DIN          GPIO_NUM_15
#define AUDIO_I2S_GPIO_DOUT         GPIO_NUM_16
#define AUDIO_CODEC_PA_PIN          GPIO_NUM_NC
#define AUDIO_INPUT_REFERENCE       true
#define AUDIO_CODEC_ES8311_ADDR     ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR     ES7210_CODEC_DEFAULT_ADDR

#define I2C_SCL_IO                  GPIO_NUM_10       
#define I2C_SDA_IO                  GPIO_NUM_11        

#define I2C_ADDRESS                 ESP_IO_EXPANDER_I2C_TCA9555_ADDRESS_000

#define DISPLAY_SDA_PIN             I2C_SDA_IO
#define DISPLAY_SCL_PIN             I2C_SCL_IO

#define DISPLAY_MISO_PIN            GPIO_NUM_8
#define DISPLAY_MOSI_PIN            GPIO_NUM_9
#define DISPLAY_SCLK_PIN            GPIO_NUM_4
#define DISPLAY_CS_PIN              GPIO_NUM_3
#define DISPLAY_DC_PIN              GPIO_NUM_7
#define DISPLAY_RESET_PIN           GPIO_NUM_NC
#define DISPLAY_BACKLIGHT_PIN       GPIO_NUM_5

#define DISPLAY_SPI_SCLK_HZ         (20 * 1000 * 1000)

/* Camera pins */
#define CAMERA_PIN_PWDN     -1
#define CAMERA_PIN_RESET    -1
#define CAMERA_PIN_XCLK     43
#define CAMERA_PIN_SIOD     -1
#define CAMERA_PIN_SIOC     -1

#define CAMERA_PIN_D7       48
#define CAMERA_PIN_D6       47
#define CAMERA_PIN_D5       46
#define CAMERA_PIN_D4       45
#define CAMERA_PIN_D3       39
#define CAMERA_PIN_D2       18
#define CAMERA_PIN_D1       17
#define CAMERA_PIN_D0       2
#define CAMERA_PIN_VSYNC    21
#define CAMERA_PIN_HREF     1
#define CAMERA_PIN_PCLK     44

#define XCLK_FREQ_HZ 20000000




#ifdef CONFIG_AUDIO_BOARD_LCD_JD9853
#define LCD_TYPE_JD9853_SERIAL
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  172

#define DISPLAY_SWAP_XY true
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y true
#define DISPLAY_INVERT_COLOR    true
#define BACKLIGHT_INVERT false
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#endif

#ifdef CONFIG_AUDIO_BOARD_LCD_ST7789
#define LCD_TYPE_ST7789_SERIAL
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320

#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_INVERT_COLOR    true
#define BACKLIGHT_INVERT false
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#endif



#endif // _BOARD_CONFIG_H_