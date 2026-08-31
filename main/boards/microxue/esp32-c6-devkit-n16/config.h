#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

/*
 * Audio I2S pin mapping - 可根据需要调整。推荐把红外电源接到 3.3V（见 README）。
 */
#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_19
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_22
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_21
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_20   // INMP441 SD -> MCU DIN
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_23   // MCU DOUT -> MAX98357 DIN

/* Audio codec I2C (if used) */
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_18
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_8

/* IR pins (按推荐) */
#define IR_RX_GPIO GPIO_NUM_4   // RXT -> 接收输出
#define IR_TX_GPIO GPIO_NUM_5   // XTD -> 发射输入（TTL）或驱动晶体管基极

/* Buttons / optional */
#define BOOT_BUTTON_GPIO GPIO_NUM_9
#define PWR_BUTTON_GPIO GPIO_NUM_2

#endif // _BOARD_CONFIG_H_
