/*
 * Pin definitions for LILYGO T-Circle-S3 V1.1.
 *
 * Identical to V1.0 except for the microphone: V1.0 carries an MSM261S4030H0R
 * (I2S, needs BCLK/WS/DATA) while V1.1 carries an MP34DT05-A (PDM, needs only
 * CLK/DATA). See the vendor pin_config.h in Xinyuan-LilyGO/T-Circle-S3.
 */
#pragma once

// MAX98357A (unchanged between V1.0 and V1.1)
#define MAX98357A_BCLK 5
#define MAX98357A_LRCLK 4
#define MAX98357A_DATA 6
#define MAX98357A_SD_MODE 45

// MP34DT05-A (PDM microphone, V1.1 only). There is no BCLK in PDM mode.
#define MP34DT05_CLK 9
#define MP34DT05_DATA 8

// APA102
#define APA102_DATA 38
#define APA102_CLOCK 39

// H0075Y002-V0
#define LCD_WIDTH 160
#define LCD_HEIGHT 160
#define LCD_MOSI 17
#define LCD_SCLK 15
#define LCD_DC 16
#define LCD_RST -1
#define LCD_CS 13
#define LCD_BL 18

// IIC
#define IIC_SDA 11
#define IIC_SCL 14

// CST816D
#define TP_SDA 11
#define TP_SCL 14
#define TP_RST -1
#define TP_INT 12

// Rotary Encoder
#define KNOB_DATA_A 47
#define KNOB_DATA_B 48
