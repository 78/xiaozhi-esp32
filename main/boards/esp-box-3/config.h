#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE    true

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_2
#define AUDIO_I2S_GPIO_WS GPIO_NUM_45
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_17
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_16
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_15

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_46
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_8
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_18
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_47
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false


// ---- ESP32-S3-BOX-3-SENSOR sub-board ----
// Per official Espressif schematics (SENSOR-01 V1.1 + SENSOR-02 V1.1):
//   AHT30 (I²C 0x38)         on shared I²C bus pins below
//   MS58-3909S68U4 radar     on the SAME I²C bus + digital OUT pin
//   IR receiver (IRM-H638T)  digital input
//   IR transmitter (IR67-21C) PWM-modulated drive
//   Battery ADC               via voltage divider 301k/100k (×4.01 scaling)
//   MicroSD                   SPI mode
// Sensor I²C bus is independent from the audio codec I²C (IO8/IO18) — uses
// the second I²C peripheral so audio init isn't disturbed.
#define SENSOR_I2C_PORT         I2C_NUM_0
#define SENSOR_I2C_SDA_PIN      GPIO_NUM_41
#define SENSOR_I2C_SCL_PIN      GPIO_NUM_40

#define SENSOR_AHT30_ADDR       0x38
#define SENSOR_RADAR_ADDR       0x4C   // MS58-3909S68U4 default I²C address
#define SENSOR_RADAR_OUT_PIN    GPIO_NUM_21    // RI_OUT — fast presence flag

#define SENSOR_IR_TX_PIN        GPIO_NUM_39
#define SENSOR_IR_RX_PIN        GPIO_NUM_38
// Per SENSOR-02 V1.1 schematic (zoomed render of IR Receiver block):
//   Q2 (AO3401A P-MOSFET): source = VCC_3V3, drain = IR_3V3, gate = "RXD"
//   R12 (10K) pulls gate to RXD net.
// "RXD" on the SENSOR sub-board's goldfinger maps to UART0 RX on the
// BOX-3 main board, which is GPIO 44 (per espressif/esp-bsp esp-box-3.h
// PMOD2 IO4 = GPIO_NUM_44 = UART0 RX by default).
// Driving IO44 LOW pulls the P-MOSFET gate down → MOSFET conducts →
// IR_3V3 = VCC_3V3 → IRM-H638T receiver IC powers up.
// Safe because xiaozhi uses USB-Serial-JTAG (GPIO 19/20) for the console;
// UART0 is unused.
#define SENSOR_IR_POWER_PIN     GPIO_NUM_44

#define SENSOR_BATTERY_ADC_PIN  GPIO_NUM_10    // BAT_MEAS_ADC, IO10 = ADC1_CH9
#define SENSOR_BATTERY_DIVIDER  4.01f          // (R15+R16)/R16 = (301k+100k)/100k

#define SENSOR_SD_CS_PIN        GPIO_NUM_12    // SD_DAT3
#define SENSOR_SD_MOSI_PIN      GPIO_NUM_11    // SD_CMD
#define SENSOR_SD_CLK_PIN       GPIO_NUM_14    // SD_CLK
#define SENSOR_SD_MISO_PIN      GPIO_NUM_13    // SD_DAT0


#endif // _BOARD_CONFIG_H_
