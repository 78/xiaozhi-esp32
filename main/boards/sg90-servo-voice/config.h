#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// 音频配置
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 使用 Simplex I2S 模式（麦克风输入 + 可选扬声器输出）
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

// 麦克风输入引脚 (INMP441)
// ESP32-S3 ↔ INMP441: GPIO4(WS), GPIO5(SCK), GPIO6(SD)
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6

// 扬声器输出引脚 (MAX98357A)
// ESP32-S3 ↔ MAX98357A: GPIO7(DIN), GPIO15(BCLK), GPIO16(LRC)
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

#else

// Duplex I2S 模式引脚
#define AUDIO_I2S_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7

#endif

// 控制按钮引脚
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_5
#define ASR_BUTTON_GPIO         GPIO_NUM_19

// 状态指示LED
#define BUILTIN_LED_GPIO        GPIO_NUM_2

// 显示屏引脚（可选OLED）
#define DISPLAY_SDA_PIN GPIO_NUM_4
#define DISPLAY_SCL_PIN GPIO_NUM_15
#define DISPLAY_WIDTH   128

#if CONFIG_OLED_SSD1306_128X32
#define DISPLAY_HEIGHT  32
#elif CONFIG_OLED_SSD1306_128X64
#define DISPLAY_HEIGHT  64
#else
// 默认使用64像素高度，如果没有显示屏则在代码中处理
#define DISPLAY_HEIGHT  64
#endif

#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true

// SG90舵机控制引脚
#define SERVO_GPIO GPIO_NUM_18

// 舵机参数配置
#define SERVO_MIN_PULSEWIDTH_US 500           // 最小脉宽（微秒）对应0度
#define SERVO_MAX_PULSEWIDTH_US 2500          // 最大脉宽（微秒）对应180度
#define SERVO_MIN_DEGREE 0                    // 最小角度
#define SERVO_MAX_DEGREE 180                  // 最大角度
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD 20000           // 20000 ticks, 20ms (50Hz)

// 舵机默认位置和限制
#define SERVO_DEFAULT_ANGLE 90                // 默认中心位置
#define SERVO_MAX_SPEED_DEGREE_PER_SEC 180    // 最大转速限制

// 板子版本信息
#define SG90_SERVO_VOICE_VERSION "1.0.0"

#endif // _BOARD_CONFIG_H_
