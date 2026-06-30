#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ============================================================
// 音频配置
// ============================================================
#define AUDIO_INPUT_SAMPLE_RATE  16000  // INMP441 麦克风采样率
#define AUDIO_OUTPUT_SAMPLE_RATE 16000  // MAX98357A 喇叭输出采样率

// Duplex I2S 模式: INMP441 和 MAX98357A 共享 BCLK/WS, 只需 4 根 GPIO
// 如果想用独立 I2S 总线 (Simplex), 需要额外 2 根引脚 (GPIO15/16), 在板子对面一排
// #define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

// Simplex 模式引脚 (独立 I2S 总线, 需跨排接线到 GPIO15/16)
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_15  // 对面一排
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_16  // 对面一排

#else
// Duplex 模式引脚 (共享 I2S 总线, 全部在左排 GPIO1~8)
#define AUDIO_I2S_GPIO_WS    GPIO_NUM_4   // WS/LRCLK → INMP441 WS + MAX98357A LRC
#define AUDIO_I2S_GPIO_BCLK  GPIO_NUM_5   // SCK/BCLK → INMP441 SCK + MAX98357A BCLK
#define AUDIO_I2S_GPIO_DIN   GPIO_NUM_6   // 数据输入 ← INMP441 DOUT (麦克风录音)
#define AUDIO_I2S_GPIO_DOUT  GPIO_NUM_7   // 数据输出 → MAX98357A DIN (喇叭播放)
#endif

// ============================================================
// 按钮配置
// ============================================================
#define BOOT_BUTTON_GPIO        GPIO_NUM_0  // 板载 BOOT 按钮 (内部, 无需接线)

// ============================================================
// SSD1306 OLED 显示屏配置 (I2C 接口)
// ============================================================
#define DISPLAY_SDA_PIN GPIO_NUM_1   // I2C 数据线 SDA → OLED SDA
#define DISPLAY_SCL_PIN GPIO_NUM_2   // I2C 时钟线 SCL → OLED SCL
// SSD1306 I2C 地址: 0x3C (最常见) 或 0x3D

#define DISPLAY_WIDTH   128           // 屏幕宽度

// 根据你的 OLED 屏幕尺寸选择:
//   128x32: 改为 32
//   128x64: 保持 64
#define DISPLAY_HEIGHT  64

#define DISPLAY_MIRROR_X true        // 水平镜像 (根据实际显示方向调整)
#define DISPLAY_MIRROR_Y true        // 垂直镜像 (根据实际显示方向调整)

#endif // _BOARD_CONFIG_H_
