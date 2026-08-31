#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

/* Audio I2S pin mapping */
#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_19
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_22
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_21
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_20
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_23

/* Audio codec I2C if needed */
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_18
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_8

/* IR pins */
#define IR_RX_GPIO GPIO_NUM_4
#define IR_TX_GPIO GPIO_NUM_5

/* Buttons / optional */
#define BOOT_BUTTON_GPIO GPIO_NUM_9
#define PWR_BUTTON_GPIO GPIO_NUM_2

#endif // _BOARD_CONFIG_H_
