#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ==================== 音频 ====================
// 本地编解码器采样率（PDM 麦克风 + NS4168 功放，无外部 codec 芯片）
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// ==================== 按键 ====================
// 主按键：单击切换对话，启动时进入配网模式
// TODO: 真机上的主按键 KEY_M 在 IO15，这里仍是 IO0（ESP-BOX-3 遗留值），待修正
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

// 说明：音频、NFC、LED 的 GPIO 号直接写在 cx-esp32s3.cc 里，
//       引脚真值表见本板 README.md。

#endif // _BOARD_CONFIG_H_
