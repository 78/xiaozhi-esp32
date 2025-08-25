#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// 音频相关配置
#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE true

// ES8311编解码器I2S接口引脚定义
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_38 // 主时钟，ES8311需要此信号
#define AUDIO_I2S_GPIO_WS GPIO_NUM_1    // 帧时钟（左右声道）
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_40 // 位时钟
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_2   // 数据输入（麦克风信号输入到ESP32）
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_3  // 数据输出（ESP32输出信号到扬声器）

// ES8311 I2C控制接口定义
#define AUDIO_CODEC_PA_PIN GPIO_NUM_NC                    // NS4150B功放使能引脚（如无使能控制则设为GPIO_NUM_NC）
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_41               // I2C数据线
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_42               // I2C时钟线
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR // ES8311默认I2C地址 (0x18)

// boot相关配置
#define BOOT_GPIO GPIO_NUM_0

// 按钮相关配置
#define BUTTON_GPIO GPIO_NUM_8

// 电源相关配置
#define PWR_CTRL_GPIO GPIO_NUM_18
#define PWR_CHARGE_DONE_GPIO GPIO_NUM_9
#define PWR_CHARGING_GPIO GPIO_NUM_46

// LED相关配置
#define LED_RED_GPIO GPIO_NUM_16
#define LED_GREEN_GPIO GPIO_NUM_17

#endif