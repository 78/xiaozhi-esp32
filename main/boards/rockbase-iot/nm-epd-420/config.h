#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ─── Audio (ES8311 codec + LMD4737 PDM DMIC, shared I2S) ────────────────────
// Mic path: LMD4737 (PDM) -> ES8311 DMIC input -> I2S ASDOUT -> GPIO16.
// NOTE: the PDM microphone clock shares the I2S BCLK net. At 24 kHz / 16-bit
// stereo slots BCLK = 768 kHz, slightly below the LMD4737 1.0-3.5 MHz spec.
// If recording is noisy on real hardware, consider 32-bit I2S slots.
#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_21
#define AUDIO_I2S_GPIO_WS GPIO_NUM_17
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_16   // ES8311 ASDOUT (ADC -> ESP)
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_18  // ESP -> ES8311 DSDIN (DAC)

#define AUDIO_CODEC_PA_PIN GPIO_NUM_41  // external Class-D amp enable (HIGH = on)
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_39
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_38
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_USE_DMIC true  // LMD4737 PDM mic via ES8311 DMIC input

// ─── Module power gates (active HIGH) ────────────────────────────────────────
#define CODEC_EN_GPIO GPIO_NUM_44  // ES8311 codec power enable
#define LORA_EN_GPIO GPIO_NUM_47   // LoRa module power enable (kept off)
#define TEMP_CTL_GPIO GPIO_NUM_40  // AHT20 power gate
#define ADC_EN_GPIO GPIO_NUM_43    // Battery ADC circuit enable

// ─── AHT20 temperature / humidity sensor ─────────────────────────────────────
#define AHT20_I2C_ADDR 0x38

// ─── Buttons (active LOW, external pull-up) ──────────────────────────────────
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define USER_BUTTON_GPIO GPIO_NUM_45

// ─── Battery voltage ADC ─────────────────────────────────────────────────────
// GPIO3 = ADC1_CHANNEL_2, 2:1 resistor divider behind ADC_EN gate.
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_2
#define BATTERY_ADC_DIV 2

// ─── E-paper display: GYE042A87 4.2" 400x300 B/W, SSD1683, FSPI bus ─────────
#define EPD_SPI_NUM SPI2_HOST
#define EPD_SCK_PIN GPIO_NUM_2
#define EPD_MOSI_PIN GPIO_NUM_1
#define EPD_CS_PIN GPIO_NUM_46
#define EPD_DC_PIN GPIO_NUM_4
#define EPD_RST_PIN GPIO_NUM_5
#define EPD_BUSY_PIN GPIO_NUM_6  // HIGH while the controller is busy

#define DISPLAY_WIDTH 400
#define DISPLAY_HEIGHT 300

#endif  // _BOARD_CONFIG_H_
