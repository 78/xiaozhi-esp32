#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>



#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE    true

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_42
#define AUDIO_I2S_GPIO_WS GPIO_NUM_39
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_41
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_40
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_38
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_8
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_1
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_2
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

//SD卡
#define BSP_SD_CLK          GPIO_NUM_47
#define BSP_SD_CMD          GPIO_NUM_48
#define BSP_SD_D0           GPIO_NUM_21
#define MOUNT_POINT              "/sdcard"
#define SD_CD                  GPIO_NUM_9

//#define BUILTIN_LED_GPIO        GPIO_NUM_45
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_46
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_3


#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_16
#define DISPLAY_MOSI_PIN      GPIO_NUM_5
#define DISPLAY_CLK_PIN       GPIO_NUM_6
#define DISPLAY_DC_PIN        GPIO_NUM_15
#define DISPLAY_RST_PIN       GPIO_NUM_4
#define DISPLAY_CS_PIN        GPIO_NUM_7

/* 竖屏 */
// #define LCD_TYPE_NV3030B_SERIAL
// #define DISPLAY_WIDTH   240          //宽度
// #define DISPLAY_HEIGHT  296          //高度
// #define DISPLAY_MIRROR_X false       //是否镜像X轴
// #define DISPLAY_MIRROR_Y false       //是否镜像Y轴
// #define DISPLAY_SWAP_XY false        //是否交换X,Y轴
// #define DISPLAY_INVERT_COLOR    true     //是否反转颜色
// #define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_RGB     //RGB顺序
// #define DISPLAY_OFFSET_X  0          //偏移X轴
// #define DISPLAY_OFFSET_Y  0          //偏移Y轴
// #define DISPLAY_BACKLIGHT_OUTPUT_INVERT false            //背光输出是否反转
// #define DISPLAY_SPI_MODE 0           //SPI模式

/* 横屏 */
#define DISPLAY_WIDTH   296
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true
#define DISPLAY_INVERT_COLOR    false
#define DISPLAY_RGB_ORDER  LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE 0

//ADC
#define VBAT_ADC_CHANNEL         ADC_CHANNEL_9      // S3对应引脚: IO10
#define ADC_ATTEN                ADC_ATTEN_DB_11    // 11dB衰减，支持0-3.1V输入范围
#define ADC_WIDTH                ADC_BITWIDTH_12
#define FULL_BATTERY_VOLTAGE     4100               // 放电范围上限4.1V
#define EMPTY_BATTERY_VOLTAGE    3300               // 放电范围下限3.2V

//充电检测
#define CHRG_PIN GPIO_NUM_14

// A MCP Test: Control a lamp
#define LAMP_GPIO GPIO_NUM_45



#endif // _BOARD_CONFIG_H_
