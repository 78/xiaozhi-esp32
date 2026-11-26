#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// Guition JC8048W550: ESP32-S3, 16MB flash, 8MB PSRAM, 5" 800x480 RGB panel
// with a GT911 capacitive touch controller.
//
// Pins come from the vendor demo package (Arduino demos "3_3-4 LVGL Widgets",
// "6_1 Audio_test" and "7_1 lvgl_music_gt911") and match the community ESPHome
// definition of this board. There is no schematic in the package, so none of
// this has been measured: check it against the board before trusting it.

#include <driver/gpio.h>

// Left column of the screen: the emotion, bottom-aligned and at most this wide
// (any height). The rest of the screen is the right panel (chat or idle dashboard).
#define DISPLAY_EMOJI_COLUMN_WIDTH 240

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Speaker: I2S amplifier on the speaker connector, DOUT/LRC/BCLK only.
// The vendor Audio_test demo puts BCLK on IO19, but IO19 is the touch SDA line;
// the demo that runs touch and audio together uses IO0, which is used here.
// IO0 is also the BOOT switch, so there is no button input on this board.
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_0
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_18
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_17

// Microphone: external INMP441 (I2S, L/R tied to GND = left channel) wired to
// the P2 header. The vendor pin tables show no microphone; IO11-13 are the
// SD-card lines, so the card slot must stay empty.
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_12
#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_11
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_13

// INMP441 samples are 24 bit in a 32-bit slot. A shift of 16 is unity gain; each
// step below that doubles the gain. The stock codec uses 12 (+24 dB), which
// clips close speech. 14 (+12 dB) is a starting guess, to be tuned on the device.
#define AUDIO_MIC_SAMPLE_SHIFT 14

// Display: 800x480 RGB565 over the ESP32-S3 LCD_CAM peripheral, no DISP pin.
#define DISPLAY_WIDTH 800
#define DISPLAY_HEIGHT 480
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

// 12 MHz was tried against right-edge flicker with animated emoji: the panel
// then showed only cycling colours. Keep the vendor's 16 MHz.
#define LCD_PCLK_HZ (16 * 1000 * 1000)

#define LCD_PIN_DE GPIO_NUM_40
#define LCD_PIN_VSYNC GPIO_NUM_41
#define LCD_PIN_HSYNC GPIO_NUM_39
#define LCD_PIN_PCLK GPIO_NUM_42
#define LCD_PIN_DISP GPIO_NUM_NC

// esp_lcd wants the 16 lines in this order: B0-B4, G0-G5, R0-R4.
#define LCD_PIN_DATA0 GPIO_NUM_8   // B0
#define LCD_PIN_DATA1 GPIO_NUM_3   // B1
#define LCD_PIN_DATA2 GPIO_NUM_46  // B2
#define LCD_PIN_DATA3 GPIO_NUM_9   // B3
#define LCD_PIN_DATA4 GPIO_NUM_1   // B4
#define LCD_PIN_DATA5 GPIO_NUM_5   // G0
#define LCD_PIN_DATA6 GPIO_NUM_6   // G1
#define LCD_PIN_DATA7 GPIO_NUM_7   // G2
#define LCD_PIN_DATA8 GPIO_NUM_15  // G3
#define LCD_PIN_DATA9 GPIO_NUM_16  // G4
#define LCD_PIN_DATA10 GPIO_NUM_4  // G5
#define LCD_PIN_DATA11 GPIO_NUM_45 // R0
#define LCD_PIN_DATA12 GPIO_NUM_48 // R1
#define LCD_PIN_DATA13 GPIO_NUM_47 // R2
#define LCD_PIN_DATA14 GPIO_NUM_21 // R3
#define LCD_PIN_DATA15 GPIO_NUM_14 // R4

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_2
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_BACKLIGHT_PWM_HZ 1220

// Touch: GT911 on its own I2C bus. INT is not wired to the ESP32.
#define TOUCH_I2C_PORT I2C_NUM_0
#define TOUCH_PIN_NUM_SDA GPIO_NUM_19
#define TOUCH_PIN_NUM_SCL GPIO_NUM_20
#define TOUCH_PIN_NUM_INT GPIO_NUM_NC
#define TOUCH_PIN_NUM_RST GPIO_NUM_38

#endif  // _BOARD_CONFIG_H_
