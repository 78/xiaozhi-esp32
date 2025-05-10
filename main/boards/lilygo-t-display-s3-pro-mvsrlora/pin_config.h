/*
 * @Description: None
 * @Author: None
 * @Date: 2023-08-16 14:24:03
 * @LastEditTime: 2025-04-23 11:25:00
 * @License: GPL 3.0
 */
#pragma once

// ST7796
#define LCD_WIDTH 222
#define LCD_HEIGHT 480
#define LCD_BL 48
#define LCD_MOSI 17
#define LCD_MISO 8
#define LCD_SCLK 18
#define LCD_CS 39
#define LCD_DC 9
#define LCD_RST 47

// IIC
#define IIC_SDA 5
#define IIC_SCL 6

// CST226SE
#define CST226SE_IIC_ADDRESS 0x5A
#define TOUCH_RST 13
#define TOUCH_INT 21
#define TOUCH_IIC_SDA IIC_SDA
#define TOUCH_IIC_SCL IIC_SCL

// SY6970
#define SY6970_SDA 5
#define SY6970_SCL 6
#define SY6970_Address 0x6A
#define SY6970_INT 21

// SD
#define SD_CS 14
#define SD_MISO 8
#define SD_MOSI 17
#define SD_SCLK 18

// RT9080
#define RT9080_EN 42

// MAX98357A
#define MAX98357A_BCLK 4
#define MAX98357A_LRCLK 15
#define MAX98357A_DATA 11
#define MAX98357A_EN 41

// Vibration Motor
#define VIBRATINO_MOTOR_PWM 45

// PCF85063
#define PCF85063_IIC_SDA 5
#define PCF85063_IIC_SCL 6
#define PCF85063_INT 21

// LR1121
#define LR1121_BUSY 46
#define LR1121_INT 40
#define LR1121_SCLK 18
#define LR1121_MOSI 17
#define LR1121_MISO 8
#define LR1121_CS 7
#define LR1121_RST 10

// ICM20948
#define ICM20948_ADDRESS 0x28
#define ICM20948_SDA 5
#define ICM20948_SCL 6
#define ICM20948_INT 21

// MP34DT05TRF
#define MP34DT05TR_LRCLK 1
#define MP34DT05TR_DATA 2
#define MP34DT05TR_EN 3

