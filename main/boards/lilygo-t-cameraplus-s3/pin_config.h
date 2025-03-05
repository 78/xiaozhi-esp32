/*
 * @Description: None
 * @version: V1.0.0
 * @Author: None
 * @Date: 2023-08-16 14:24:03
 * @LastEditors: LILYGO_L
 * @LastEditTime: 2023-12-12 10:12:31
 * @License: GPL 3.0
 */
#pragma once

// microSD
#define SD_CS 21
#define SD_SCLK 36
#define SD_MOSI 35
#define SD_MISO 37

// SPI
#define SCLK 36
#define MOSI 35
#define MISO 37

// MAX98357A
#define MAX98357A_BCLK 41
#define MAX98357A_LRCLK 42
#define MAX98357A_DOUT 38

// MSM261
#define MSM261_BCLK 18
#define MSM261_WS 39
#define MSM261_DIN 40

// FP-133H01D
#define LCD_WIDTH 240
#define LCD_HEIGHT 240
#define LCD_BL 46
#define LCD_MOSI 35
#define LCD_SCLK 36
#define LCD_CS 34
#define LCD_DC 45
#define LCD_RST 33

// SY6970
#define SY6970_SDA 1
#define SY6970_SCL 2
#define SY6970_Address 0x6A
#define SY6970_INT 47

// IIC
#define IIC_SDA 1
#define IIC_SCL 2

// OV2640
#define OV2640_PWDN -1
#define OV2640_RESET 3
#define OV2640_XCLK 7
#define OV2640_SIOD 1
#define OV2640_SIOC 2
#define OV2640_D7 6
#define OV2640_D6 8
#define OV2640_D5 9
#define OV2640_D4 11
#define OV2640_D3 13
#define OV2640_D2 15
#define OV2640_D1 14
#define OV2640_D0 12
#define OV2640_VSYNC 4
#define OV2640_HREF 5
#define OV2640_PCLK 10

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM 3
#define XCLK_GPIO_NUM 7
#define SIOD_GPIO_NUM 1
#define SIOC_GPIO_NUM 2

#define Y9_GPIO_NUM 6
#define Y8_GPIO_NUM 8
#define Y7_GPIO_NUM 9
#define Y6_GPIO_NUM 11
#define Y5_GPIO_NUM 13
#define Y4_GPIO_NUM 15
#define Y3_GPIO_NUM 14
#define Y2_GPIO_NUM 12
#define VSYNC_GPIO_NUM 4
#define HREF_GPIO_NUM 5
#define PCLK_GPIO_NUM 10

// CST816
#define CST816_Address 0x15
#define TP_SDA 1
#define TP_SCL 2
#define TP_RST 48
#define TP_INT 47

// AP1511B
#define AP1511B_FBC 16

// KEY
#define KEY1 17
