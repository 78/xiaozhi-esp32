#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// Guition JC4827W543 (vendor model ESP32-4827A043, schematic v0.2)
// ESP32-S3-WROOM-1 N4R8: 4MB flash, 8MB OPI PSRAM.
//
// Every pin below is taken from the vendor pin table and the schematic, and was
// verified on hardware. Do not infer pins from other Guition models; the
// "Extended IO" headers differ between board revisions.

#include <driver/gpio.h>
#include <driver/spi_master.h>

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Microphone: INMP441 on header P3, L/R tied to GND.
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_6
#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_7
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_15

// Speaker. A stock board plays through the on-board NS4168, wired to the P7
// connector. That amplifier has no enable pin - its CTRL is pulled to VOUT-BAT
// through 1M, so it is live whenever the board is - which matters either way:
// when an external amplifier is used instead, the NS4168's three lines are left
// floating next to the QSPI display and it plays the noise they pick up as a
// steady hiss, so they have to be parked low.
#define AUDIO_NS4168_GPIO_BCLK GPIO_NUM_42
#define AUDIO_NS4168_GPIO_LRCK GPIO_NUM_2
#define AUDIO_NS4168_GPIO_DOUT GPIO_NUM_41

#ifdef CONFIG_JC4827W543_EXTERNAL_AMP
// External I2S amplifier on headers P2/P3 (MAX98357A: GAIN and SD floating).
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_9
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_14
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_16
#else
#define AUDIO_I2S_SPK_GPIO_BCLK AUDIO_NS4168_GPIO_BCLK
#define AUDIO_I2S_SPK_GPIO_LRCK AUDIO_NS4168_GPIO_LRCK
#define AUDIO_I2S_SPK_GPIO_DOUT AUDIO_NS4168_GPIO_DOUT
#endif

// The switch marked BOOT (10K pull-up, switch to ground). Note that SW1 is a
// different switch and does not reach the ESP32 at all - it drives the KEY pin
// of the charger, to wake the boost converter. IO0 is also the panel's TE line,
// which this build does not use.
#define BOOT_BUTTON_GPIO GPIO_NUM_0

// Battery. The schematic draws a 33K/100K divider (factor 1.33); the parts
// actually fitted are 160K/220K. There is no factory ADC calibration on this
// chip and no charge-status line, so the charging pin stays unconnected.
// ADC1 only - ADC2 is dead while WiFi is up.
#define BATTERY_ADC_UNIT ADC_UNIT_1
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_4  // GPIO5
#define BATTERY_UPPER_RESISTOR 160000.0f
#define BATTERY_LOWER_RESISTOR 220000.0f

// Display: NV3041A over QSPI, 480x272 IPS.
#define DISPLAY_WIDTH 480
#define DISPLAY_HEIGHT 272
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

// The panel is IPS, so it runs with colour inversion on.
#define DISPLAY_INVERT_COLOR true

#define QSPI_LCD_HOST SPI2_HOST
#define QSPI_LCD_BIT_PER_PIXEL 16
#define QSPI_LCD_PCLK_HZ (40 * 1000 * 1000)

#define QSPI_PIN_NUM_LCD_CS GPIO_NUM_45
#define QSPI_PIN_NUM_LCD_PCLK GPIO_NUM_47
#define QSPI_PIN_NUM_LCD_DATA0 GPIO_NUM_21
#define QSPI_PIN_NUM_LCD_DATA1 GPIO_NUM_48
#define QSPI_PIN_NUM_LCD_DATA2 GPIO_NUM_40
#define QSPI_PIN_NUM_LCD_DATA3 GPIO_NUM_39
// The panel's reset is not wired to the ESP32; IO38 drives the touch reset
// instead. The NV3041A comes up from power-on and the init sequence unlocks it.
#define QSPI_PIN_NUM_LCD_RST GPIO_NUM_NC

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_1
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// Touch: GT911 on its own I2C bus.
// The chip latches its I2C address from INT while RESET is released: INT low
// selects 0x5D, INT high 0x14. esp_lcd_touch_gt911 drives INT during reset, so
// with levels.interrupt = 0 the address is deterministically 0x5D.
#define TOUCH_I2C_PORT I2C_NUM_1
#define TOUCH_PIN_NUM_SDA GPIO_NUM_8
#define TOUCH_PIN_NUM_SCL GPIO_NUM_4
#define TOUCH_PIN_NUM_INT GPIO_NUM_3
#define TOUCH_PIN_NUM_RST GPIO_NUM_38

#endif  // _BOARD_CONFIG_H_
