#pragma once

#include <driver/gpio.h>
#include "sdkconfig.h"

/* Audio configuration */
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000
#define AUDIO_INPUT_REFERENCE    false
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

/* ADC configuration */
#define ADC_ATTEN                ADC_ATTEN_DB_12
#define ADC_WIDTH                ADC_BITWIDTH_DEFAULT
#define FULL_BATTERY_VOLTAGE     4100
#define EMPTY_BATTERY_VOLTAGE    3200

/* I2C configuration */
#define I2C_MASTER_FREQ_HZ      (400 * 1000)

/* Button configuration */
#define LONG_PRESS_TIMEOUT_US   (5 * 1000000ULL)

#ifdef CONFIG_IDF_TARGET_ESP32S3

/* Audio I2S GPIOs */
#define AUDIO_I2S_GPIO_MCLK      GPIO_NUM_NC
#define AUDIO_I2S_GPIO_WS        GPIO_NUM_17
#define AUDIO_I2S_GPIO_BCLK      GPIO_NUM_16
#define AUDIO_I2S_GPIO_DIN       GPIO_NUM_15
#define AUDIO_I2S_GPIO_DOUT      GPIO_NUM_18

/* Audio CODEC GPIOs */
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_40
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_2
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_1

/* Button GPIOs */
#define BOOT_BUTTON_GPIO         GPIO_NUM_0
#define KEY_BUTTON_GPIO          GPIO_NUM_12
#define LED_GPIO                 GPIO_NUM_11

/* ADC GPIOs */
#define VBAT_ADC_CHANNEL         ADC_CHANNEL_9  // S3: IO10
#define MCU_VCC_CTL              GPIO_NUM_4     // set 1 to power on MCU
#define PERP_VCC_CTL             GPIO_NUM_6     // set 1 to power on peripherals

/* IMU GPIOs */
#define IMU_INT_GPIO             GPIO_NUM_5

#elif defined(CONFIG_IDF_TARGET_ESP32C5)

/* Audio I2S GPIOs */
#define AUDIO_I2S_GPIO_MCLK      GPIO_NUM_NC
#define AUDIO_I2S_GPIO_WS        GPIO_NUM_8
#define AUDIO_I2S_GPIO_BCLK      GPIO_NUM_7
#define AUDIO_I2S_GPIO_DIN       GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT      GPIO_NUM_9

/* Audio CODEC GPIOs */
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_23
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_25
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_26

/* Button GPIOs */
#define BOOT_BUTTON_GPIO         GPIO_NUM_28
#define KEY_BUTTON_GPIO          GPIO_NUM_5
#define LED_GPIO                 GPIO_NUM_27

/* ADC GPIOs */
#define VBAT_ADC_CHANNEL         ADC_CHANNEL_3  // C5: IO4
#define MCU_VCC_CTL              GPIO_NUM_2     // set 1 to power on MCU
#define PERP_VCC_CTL             GPIO_NUM_0     // set 1 to power on peripherals

/* IMU GPIOs */
#define IMU_INT_GPIO             GPIO_NUM_3

#endif // CONFIG_IDF_TARGET

