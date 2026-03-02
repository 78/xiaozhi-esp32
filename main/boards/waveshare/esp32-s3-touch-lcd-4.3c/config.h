#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include "custom_io_expander_ch32v003.h"

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE    true

#define BSP_I2S_SCLK            (GPIO_NUM_44)
#define BSP_I2S_MCLK            (GPIO_NUM_6)
#define BSP_I2S_LCLK            (GPIO_NUM_16)
#define BSP_I2S_DOUT            (GPIO_NUM_15)    // To Codec ES8311
#define BSP_I2S_DSIN            (GPIO_NUM_43)   // From ADC ES7210

#define BSP_POWER_AMP_IO         (IO_EXPANDER_PIN_NUM_3)
#define BSP_PA_PIN       (GPIO_NUM_NC)

#define BSP_I2C_SCL             (GPIO_NUM_9)
#define BSP_I2C_SDA             (GPIO_NUM_8)

#define BSP_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define BSP_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BSP_IO_EXPANDER_I2C_ADDRESS CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS
#define BOOT_BUTTON_GPIO        GPIO_NUM_0

/* Display */
#define BSP_LCD_VSYNC     (GPIO_NUM_3)
#define BSP_LCD_HSYNC     (GPIO_NUM_46)
#define BSP_LCD_DE        (GPIO_NUM_5)
#define BSP_LCD_PCLK      (GPIO_NUM_7)
#define BSP_LCD_DISP      (GPIO_NUM_NC)

// Blue data signals
#define BSP_LCD_DATA0        (GPIO_NUM_14) ///< B3
#define BSP_LCD_DATA1        (GPIO_NUM_38) ///< B4
#define BSP_LCD_DATA2        (GPIO_NUM_18) ///< B5
#define BSP_LCD_DATA3        (GPIO_NUM_17) ///< B6
#define BSP_LCD_DATA4        (GPIO_NUM_10) ///< B7

// Green data signals
#define BSP_LCD_DATA5        (GPIO_NUM_39) ///< G2
#define BSP_LCD_DATA6        (GPIO_NUM_0)  ///< G3
#define BSP_LCD_DATA7        (GPIO_NUM_45) ///< G4
#define BSP_LCD_DATA8        (GPIO_NUM_48) ///< G5
#define BSP_LCD_DATA9        (GPIO_NUM_47) ///< G6
#define BSP_LCD_DATA10       (GPIO_NUM_21) ///< G7

// Red data signals
#define BSP_LCD_DATA11       (GPIO_NUM_1)  ///< R3
#define BSP_LCD_DATA12       (GPIO_NUM_2)  ///< R4
#define BSP_LCD_DATA13       (GPIO_NUM_42) ///< R5
#define BSP_LCD_DATA14       (GPIO_NUM_41) ///< R6
#define BSP_LCD_DATA15       (GPIO_NUM_40) ///< R7

#define BSP_LCD_BACKLIGHT     (IO_EXPANDER_PIN_NUM_2)
#define BSP_LCD_RST           (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_RST     (IO_EXPANDER_PIN_NUM_1)
#define BSP_LCD_TOUCH_INT     (GPIO_NUM_4)

/* LCD display color format */
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
/* LCD display color bytes endianess */
#define BSP_LCD_BIGENDIAN           (1)
/* LCD display color bits */
#define BSP_LCD_BITS_PER_PIXEL      (16)
#define BSP_LCD_BIT_PER_PIXEL       (18)
#define BSP_RGB_DATA_WIDTH          (16)

/* LCD display definition */
#define BSP_LCD_H_RES              (800)
#define BSP_LCD_V_RES              (480)


#define DISPLAY_MIRROR_X  false
#define DISPLAY_MIRROR_Y  false
#define DISPLAY_SWAP_XY   false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN            GPIO_NUM_NC
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT  true


#endif // _BOARD_CONFIG_H_
