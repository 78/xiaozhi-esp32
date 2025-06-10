#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ======================================================================
//                            通用配置
// ======================================================================
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_INPUT_REFERENCE    true

// ======================================================================
//                          主控引脚分配
// ======================================================================

// --- I2S 音频接口 ---
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_38
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_13
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_14
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_12
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_45

// --- I2C 主总线 (连接音频CODEC, 触摸屏, IO扩展) ---
#define I2C_MASTER_SDA_PIN  GPIO_NUM_1
#define I2C_MASTER_SCL_PIN  GPIO_NUM_2
#define I2C_MASTER_PORT     I2C_NUM_1 // 使用I2C1

// --- 板载按键 ---
#define BOOT_BUTTON_GPIO    GPIO_NUM_0

// --- 4G 模块 UART ---
#define ML307_TX_PIN GPIO_NUM_11
#define ML307_RX_PIN GPIO_NUM_10
#define ML307_RX_BUFFER_SIZE 4096

// --- DVP 摄像头接口 ---
#define CAMERA_PIN_PWDN  -1 // 由 AW9523B 控制
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK  5
#define CAMERA_PIN_SIOD  I2C_MASTER_SDA_PIN // 与I2C共用
#define CAMERA_PIN_SIOC  I2C_MASTER_SCL_PIN // 与I2C共用

#define CAMERA_PIN_D7 9
#define CAMERA_PIN_D6 4
#define CAMERA_PIN_D5 6
#define CAMERA_PIN_D4 15
#define CAMERA_PIN_D3 17
#define CAMERA_PIN_D2 8
#define CAMERA_PIN_D1 18
#define CAMERA_PIN_D0 16
#define CAMERA_PIN_VSYNC 3
#define CAMERA_PIN_HREF 46
#define CAMERA_PIN_PCLK 7
#define XCLK_FREQ_HZ 24000000

// --- SPI 屏幕接口 ---
#define DISPLAY_SPI_HOST    SPI3_HOST
#define DISPLAY_SPI_MOSI    GPIO_NUM_40
#define DISPLAY_SPI_SCLK    GPIO_NUM_41
#define DISPLAY_SPI_DC      GPIO_NUM_39
// CS由AW9523B控制

// --- 屏幕参数 ---
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

// ======================================================================
//                        I2C 设备地址和定义
// ======================================================================
#define AUDIO_CODEC_ES8311_ADDR  0x18
#define AUDIO_CODEC_ES7210_ADDR  0x40
#define AXP2101_I2C_ADDR         0x34
#define AW9523B_I2C_ADDR         0x58

// ======================================================================
//                       AW9523B IO 扩展芯片定义
// ======================================================================
#define AW9523B_INTERRUPT_PIN GPIO_NUM_42

// -- AW9523B 引脚功能分配 (根据您的最终原理图) --
// P0 端口 (引脚号 0-7)
#define AW9523B_PIN_LCD_CS       0  // P0_0
#define AW9523B_PIN_PA_EN        1  // P0_1
#define AW9523B_PIN_DVP_PWDN     2  // P0_2
#define AW9523B_PIN_RGB_R        3  // P0_3 (Red)
#define AW9523B_PIN_RGB_G        4  // P0_4 (Green)
#define AW9523B_PIN_RGB_B        5  // P0_5 (Blue)
#define AW9523B_PIN_BACKLIGHT    6  // P0_6 (LCD Backlight)
// P0_7 is unused

// P1 端口 (引脚号 8-15)
#define AW9523B_PIN_RTC_INT      8  // P1_0
#define AW9523B_PIN_PJ_SET       9  // P1_1 (耳机检测)
// P1_2 to P1_7 are unused

#endif // _BOARD_CONFIG_H_