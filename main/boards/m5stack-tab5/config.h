#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

/* ---------------------------------------------------------------- */
// Audio CODEC ES7210 + ES8311
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE    true

#define AUDIO_I2S_GPIO_MCLK      GPIO_NUM_30
#define AUDIO_I2S_GPIO_WS        GPIO_NUM_29
#define AUDIO_I2S_GPIO_BCLK      GPIO_NUM_27
#define AUDIO_I2S_GPIO_DIN       GPIO_NUM_28
#define AUDIO_I2S_GPIO_DOUT      GPIO_NUM_26

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC // PI4IOE 控制 
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_31
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_32
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BUILTIN_LED_GPIO         GPIO_NUM_NC
#define BOOT_BUTTON_GPIO         GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO    GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO  GPIO_NUM_NC

/* ---------------------------------------------------------------- */
// 显示屏相关参数配置
#define DISPLAY_WIDTH    720
#define DISPLAY_HEIGHT   1280
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_22
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#define TOUCH_INT_GPIO  GPIO_NUM_23  // 触摸中断

const ili9881c_lcd_init_cmd_t tab5_lcd_ili9881c_specific_init_code_default[] = {
    // {cmd, { data }, data_size, delay}
    /**** CMD_Page 1 ****/
    {0xFF, (uint8_t[]){0x98, 0x81, 0x01}, 3, 0},
    {0xB7, (uint8_t[]){0x03}, 1, 0},  // set 2 lane
    /**** CMD_Page 3 ****/
    {0xFF, (uint8_t[]){0x98, 0x81, 0x03}, 3, 0},
    {0x01, (uint8_t[]){0x00}, 1, 0},
    {0x02, (uint8_t[]){0x00}, 1, 0},
    {0x03, (uint8_t[]){0x73}, 1, 0},
    {0x04, (uint8_t[]){0x00}, 1, 0},
    {0x05, (uint8_t[]){0x00}, 1, 0},
    {0x06, (uint8_t[]){0x08}, 1, 0},
    {0x07, (uint8_t[]){0x00}, 1, 0},
    {0x08, (uint8_t[]){0x00}, 1, 0},
    {0x09, (uint8_t[]){0x1B}, 1, 0},
    {0x0a, (uint8_t[]){0x01}, 1, 0},
    {0x0b, (uint8_t[]){0x01}, 1, 0},
    {0x0c, (uint8_t[]){0x0D}, 1, 0},
    {0x0d, (uint8_t[]){0x01}, 1, 0},
    {0x0e, (uint8_t[]){0x01}, 1, 0},
    {0x0f, (uint8_t[]){0x26}, 1, 0},
    {0x10, (uint8_t[]){0x26}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 1, 0},
    {0x12, (uint8_t[]){0x00}, 1, 0},
    {0x13, (uint8_t[]){0x02}, 1, 0},
    {0x14, (uint8_t[]){0x00}, 1, 0},
    {0x15, (uint8_t[]){0x00}, 1, 0},
    {0x16, (uint8_t[]){0x00}, 1, 0},
    {0x17, (uint8_t[]){0x00}, 1, 0},
    {0x18, (uint8_t[]){0x00}, 1, 0},
    {0x19, (uint8_t[]){0x00}, 1, 0},
    {0x1a, (uint8_t[]){0x00}, 1, 0},
    {0x1b, (uint8_t[]){0x00}, 1, 0},
    {0x1c, (uint8_t[]){0x00}, 1, 0},
    {0x1d, (uint8_t[]){0x00}, 1, 0},
    {0x1e, (uint8_t[]){0x40}, 1, 0},
    {0x1f, (uint8_t[]){0x00}, 1, 0},
    {0x20, (uint8_t[]){0x06}, 1, 0},
    {0x21, (uint8_t[]){0x01}, 1, 0},
    {0x22, (uint8_t[]){0x00}, 1, 0},
    {0x23, (uint8_t[]){0x00}, 1, 0},
    {0x24, (uint8_t[]){0x00}, 1, 0},
    {0x25, (uint8_t[]){0x00}, 1, 0},
    {0x26, (uint8_t[]){0x00}, 1, 0},
    {0x27, (uint8_t[]){0x00}, 1, 0},
    {0x28, (uint8_t[]){0x33}, 1, 0},
    {0x29, (uint8_t[]){0x03}, 1, 0},
    {0x2a, (uint8_t[]){0x00}, 1, 0},
    {0x2b, (uint8_t[]){0x00}, 1, 0},
    {0x2c, (uint8_t[]){0x00}, 1, 0},
    {0x2d, (uint8_t[]){0x00}, 1, 0},
    {0x2e, (uint8_t[]){0x00}, 1, 0},
    {0x2f, (uint8_t[]){0x00}, 1, 0},
    {0x30, (uint8_t[]){0x00}, 1, 0},
    {0x31, (uint8_t[]){0x00}, 1, 0},
    {0x32, (uint8_t[]){0x00}, 1, 0},
    {0x33, (uint8_t[]){0x00}, 1, 0},
    {0x34, (uint8_t[]){0x00}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x36, (uint8_t[]){0x00}, 1, 0},
    {0x37, (uint8_t[]){0x00}, 1, 0},
    {0x38, (uint8_t[]){0x00}, 1, 0},
    {0x39, (uint8_t[]){0x00}, 1, 0},
    {0x3a, (uint8_t[]){0x00}, 1, 0},
    {0x3b, (uint8_t[]){0x00}, 1, 0},
    {0x3c, (uint8_t[]){0x00}, 1, 0},
    {0x3d, (uint8_t[]){0x00}, 1, 0},
    {0x3e, (uint8_t[]){0x00}, 1, 0},
    {0x3f, (uint8_t[]){0x00}, 1, 0},
    {0x40, (uint8_t[]){0x00}, 1, 0},
    {0x41, (uint8_t[]){0x00}, 1, 0},
    {0x42, (uint8_t[]){0x00}, 1, 0},
    {0x43, (uint8_t[]){0x00}, 1, 0},
    {0x44, (uint8_t[]){0x00}, 1, 0},

    {0x50, (uint8_t[]){0x01}, 1, 0},
    {0x51, (uint8_t[]){0x23}, 1, 0},
    {0x52, (uint8_t[]){0x45}, 1, 0},
    {0x53, (uint8_t[]){0x67}, 1, 0},
    {0x54, (uint8_t[]){0x89}, 1, 0},
    {0x55, (uint8_t[]){0xab}, 1, 0},
    {0x56, (uint8_t[]){0x01}, 1, 0},
    {0x57, (uint8_t[]){0x23}, 1, 0},
    {0x58, (uint8_t[]){0x45}, 1, 0},
    {0x59, (uint8_t[]){0x67}, 1, 0},
    {0x5a, (uint8_t[]){0x89}, 1, 0},
    {0x5b, (uint8_t[]){0xab}, 1, 0},
    {0x5c, (uint8_t[]){0xcd}, 1, 0},
    {0x5d, (uint8_t[]){0xef}, 1, 0},

    {0x5e, (uint8_t[]){0x11}, 1, 0},
    {0x5f, (uint8_t[]){0x02}, 1, 0},
    {0x60, (uint8_t[]){0x00}, 1, 0},
    {0x61, (uint8_t[]){0x07}, 1, 0},
    {0x62, (uint8_t[]){0x06}, 1, 0},
    {0x63, (uint8_t[]){0x0E}, 1, 0},
    {0x64, (uint8_t[]){0x0F}, 1, 0},
    {0x65, (uint8_t[]){0x0C}, 1, 0},
    {0x66, (uint8_t[]){0x0D}, 1, 0},
    {0x67, (uint8_t[]){0x02}, 1, 0},
    {0x68, (uint8_t[]){0x02}, 1, 0},
    {0x69, (uint8_t[]){0x02}, 1, 0},
    {0x6a, (uint8_t[]){0x02}, 1, 0},
    {0x6b, (uint8_t[]){0x02}, 1, 0},
    {0x6c, (uint8_t[]){0x02}, 1, 0},
    {0x6d, (uint8_t[]){0x02}, 1, 0},
    {0x6e, (uint8_t[]){0x02}, 1, 0},
    {0x6f, (uint8_t[]){0x02}, 1, 0},
    {0x70, (uint8_t[]){0x02}, 1, 0},
    {0x71, (uint8_t[]){0x02}, 1, 0},
    {0x72, (uint8_t[]){0x02}, 1, 0},
    {0x73, (uint8_t[]){0x05}, 1, 0},
    {0x74, (uint8_t[]){0x01}, 1, 0},
    {0x75, (uint8_t[]){0x02}, 1, 0},
    {0x76, (uint8_t[]){0x00}, 1, 0},
    {0x77, (uint8_t[]){0x07}, 1, 0},
    {0x78, (uint8_t[]){0x06}, 1, 0},
    {0x79, (uint8_t[]){0x0E}, 1, 0},
    {0x7a, (uint8_t[]){0x0F}, 1, 0},
    {0x7b, (uint8_t[]){0x0C}, 1, 0},
    {0x7c, (uint8_t[]){0x0D}, 1, 0},
    {0x7d, (uint8_t[]){0x02}, 1, 0},
    {0x7e, (uint8_t[]){0x02}, 1, 0},
    {0x7f, (uint8_t[]){0x02}, 1, 0},
    {0x80, (uint8_t[]){0x02}, 1, 0},
    {0x81, (uint8_t[]){0x02}, 1, 0},
    {0x82, (uint8_t[]){0x02}, 1, 0},
    {0x83, (uint8_t[]){0x02}, 1, 0},
    {0x84, (uint8_t[]){0x02}, 1, 0},
    {0x85, (uint8_t[]){0x02}, 1, 0},
    {0x86, (uint8_t[]){0x02}, 1, 0},
    {0x87, (uint8_t[]){0x02}, 1, 0},
    {0x88, (uint8_t[]){0x02}, 1, 0},
    {0x89, (uint8_t[]){0x05}, 1, 0},
    {0x8A, (uint8_t[]){0x01}, 1, 0},

    /**** CMD_Page 4 ****/
    {0xFF, (uint8_t[]){0x98, 0x81, 0x04}, 3, 0},
    {0x38, (uint8_t[]){0x01}, 1, 0},
    {0x39, (uint8_t[]){0x00}, 1, 0},
    {0x6C, (uint8_t[]){0x15}, 1, 0},
    {0x6E, (uint8_t[]){0x1A}, 1, 0},
    {0x6F, (uint8_t[]){0x25}, 1, 0},
    {0x3A, (uint8_t[]){0xA4}, 1, 0},
    {0x8D, (uint8_t[]){0x20}, 1, 0},
    {0x87, (uint8_t[]){0xBA}, 1, 0},
    {0x3B, (uint8_t[]){0x98}, 1, 0},

    /**** CMD_Page 1 ****/
    {0xFF, (uint8_t[]){0x98, 0x81, 0x01}, 3, 0},
    {0x22, (uint8_t[]){0x0A}, 1, 0},
    {0x31, (uint8_t[]){0x00}, 1, 0},
    {0x50, (uint8_t[]){0x6B}, 1, 0},
    {0x51, (uint8_t[]){0x66}, 1, 0},
    {0x53, (uint8_t[]){0x73}, 1, 0},
    {0x55, (uint8_t[]){0x8B}, 1, 0},
    {0x60, (uint8_t[]){0x1B}, 1, 0},
    {0x61, (uint8_t[]){0x01}, 1, 0},
    {0x62, (uint8_t[]){0x0C}, 1, 0},
    {0x63, (uint8_t[]){0x00}, 1, 0},

    // Gamma P
    {0xA0, (uint8_t[]){0x00}, 1, 0},
    {0xA1, (uint8_t[]){0x15}, 1, 0},
    {0xA2, (uint8_t[]){0x1F}, 1, 0},
    {0xA3, (uint8_t[]){0x13}, 1, 0},
    {0xA4, (uint8_t[]){0x11}, 1, 0},
    {0xA5, (uint8_t[]){0x21}, 1, 0},
    {0xA6, (uint8_t[]){0x17}, 1, 0},
    {0xA7, (uint8_t[]){0x1B}, 1, 0},
    {0xA8, (uint8_t[]){0x6B}, 1, 0},
    {0xA9, (uint8_t[]){0x1E}, 1, 0},
    {0xAA, (uint8_t[]){0x2B}, 1, 0},
    {0xAB, (uint8_t[]){0x5D}, 1, 0},
    {0xAC, (uint8_t[]){0x19}, 1, 0},
    {0xAD, (uint8_t[]){0x14}, 1, 0},
    {0xAE, (uint8_t[]){0x4B}, 1, 0},
    {0xAF, (uint8_t[]){0x1D}, 1, 0},
    {0xB0, (uint8_t[]){0x27}, 1, 0},
    {0xB1, (uint8_t[]){0x49}, 1, 0},
    {0xB2, (uint8_t[]){0x5D}, 1, 0},
    {0xB3, (uint8_t[]){0x39}, 1, 0},

    // Gamma N
    {0xC0, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x01}, 1, 0},
    {0xC2, (uint8_t[]){0x0C}, 1, 0},
    {0xC3, (uint8_t[]){0x11}, 1, 0},
    {0xC4, (uint8_t[]){0x15}, 1, 0},
    {0xC5, (uint8_t[]){0x28}, 1, 0},
    {0xC6, (uint8_t[]){0x1B}, 1, 0},
    {0xC7, (uint8_t[]){0x1C}, 1, 0},
    {0xC8, (uint8_t[]){0x62}, 1, 0},
    {0xC9, (uint8_t[]){0x1C}, 1, 0},
    {0xCA, (uint8_t[]){0x29}, 1, 0},
    {0xCB, (uint8_t[]){0x60}, 1, 0},
    {0xCC, (uint8_t[]){0x16}, 1, 0},
    {0xCD, (uint8_t[]){0x17}, 1, 0},
    {0xCE, (uint8_t[]){0x4A}, 1, 0},
    {0xCF, (uint8_t[]){0x23}, 1, 0},
    {0xD0, (uint8_t[]){0x24}, 1, 0},
    {0xD1, (uint8_t[]){0x4F}, 1, 0},
    {0xD2, (uint8_t[]){0x5F}, 1, 0},
    {0xD3, (uint8_t[]){0x39}, 1, 0},

    /**** CMD_Page 0 ****/
    {0xFF, (uint8_t[]){0x98, 0x81, 0x00}, 3, 0},
    {0x35, (uint8_t[]){0x00}, 0, 0},
    // {0x11, (uint8_t []){0x00}, 0},
    {0xFE, (uint8_t[]){0x00}, 0, 0},
    {0x29, (uint8_t[]){0x00}, 0, 0},
    //============ Gamma END===========
};

#endif // _BOARD_CONFIG_H_
