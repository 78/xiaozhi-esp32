/*
 * @Description: t_display_p4_config
 * @Author: LILYGO_L
 * @Date: 2024-12-06 10:32:28
 * @LastEditTime: 2025-09-10 11:56:28
 */
#pragma once

////////////////////////////////////////////////// gpio config //////////////////////////////////////////////////

// IIC
#define IIC_1_SDA 7
#define IIC_1_SCL 8
#define IIC_2_SDA 20
#define IIC_2_SCL 21

// BOOT
#define ESP32P4_BOOT 35

// XL9535
#define XL9535_SDA IIC_1_SDA
#define XL9535_SCL IIC_1_SCL
#define XL9535_INT 5
// XL9535引脚功能
#define XL9535_3_3_V_POWER_EN Cpp_Bus_Driver::Xl95x5::Pin::IO0
#define XL9535_SKY13453_VCTL Cpp_Bus_Driver::Xl95x5::Pin::IO1
#define XL9535_SCREEN_RST Cpp_Bus_Driver::Xl95x5::Pin::IO2
#define XL9535_TOUCH_RST Cpp_Bus_Driver::Xl95x5::Pin::IO3
#define XL9535_TOUCH_INT Cpp_Bus_Driver::Xl95x5::Pin::IO4
#define XL9535_ETHERNET_RST Cpp_Bus_Driver::Xl95x5::Pin::IO5
#define XL9535_5_0_V_POWER_EN Cpp_Bus_Driver::Xl95x5::Pin::IO6
#define XL9535_EXTERNAL_SENSOR_INT Cpp_Bus_Driver::Xl95x5::Pin::IO7
#define XL9535_ESP32P4_VCCA_POWER_EN Cpp_Bus_Driver::Xl95x5::Pin::IO10
#define XL9535_GPS_WAKE_UP Cpp_Bus_Driver::Xl95x5::Pin::IO11
#define XL9535_RTC_INT Cpp_Bus_Driver::Xl95x5::Pin::IO12
#define XL9535_ESP32C6_WAKE_UP Cpp_Bus_Driver::Xl95x5::Pin::IO13
#define XL9535_ESP32C6_EN Cpp_Bus_Driver::Xl95x5::Pin::IO14
#define XL9535_SD_EN Cpp_Bus_Driver::Xl95x5::Pin::IO15
#define XL9535_SX1262_RST Cpp_Bus_Driver::Xl95x5::Pin::IO16
#define XL9535_SX1262_DIO1 Cpp_Bus_Driver::Xl95x5::Pin::IO17

// ES8311
#define ES8311_SDA IIC_2_SDA
#define ES8311_SCL IIC_2_SCL
#define ES8311_ADC_DATA 11
#define ES8311_DAC_DATA 10
#define ES8311_BCLK 12
#define ES8311_MCLK 13
#define ES8311_WS_LRCK 9

// AW86224
#define AW86224_SDA IIC_2_SDA
#define AW86224_SCL IIC_2_SCL

// SGM38121
#define SGM38121_SDA IIC_2_SDA
#define SGM38121_SCL IIC_2_SCL

// PCF8563
#define PCF8563_SDA IIC_1_SDA
#define PCF8563_SCL IIC_1_SCL

// BQ27220
#define BQ27220_SDA IIC_1_SDA
#define BQ27220_SCL IIC_1_SCL

// SPI
#define SPI_1_SCLK 2
#define SPI_1_MOSI 3
#define SPI_1_MISO 4

// SX1262
#define SX1262_CS 24
#define SX1262_BUSY 6
#define SX1262_SCLK SPI_1_SCLK
#define SX1262_MOSI SPI_1_MOSI
#define SX1262_MISO SPI_1_MISO

// L76K
#define GPS_TX 22
#define GPS_RX 23

// ICM20948
#define ICM20948_SDA IIC_2_SDA
#define ICM20948_SCL IIC_2_SCL

// HI8561
#define HI8561_SCREEN_BL 51
#define HI8561_TOUCH_SDA IIC_1_SDA
#define HI8561_TOUCH_SCL IIC_1_SCL

// GT9895
#define GT9895_TOUCH_SDA IIC_1_SDA
#define GT9895_TOUCH_SCL IIC_1_SCL

// Camera
#define CAMERA_SDA IIC_2_SDA
#define CAMERA_SCL IIC_2_SCL

// SDIO
#define SDIO_1_CLK 43
#define SDIO_1_CMD 44
#define SDIO_1_D0 39
#define SDIO_1_D1 40
#define SDIO_1_D2 41
#define SDIO_1_D3 42

#define SDIO_2_CLK 18
#define SDIO_2_CMD 19
#define SDIO_2_D0 14
#define SDIO_2_D1 15
#define SDIO_2_D2 16
#define SDIO_2_D3 17

// SD
// SDMMC
#define SD_SDIO_CLK SDIO_1_CLK
#define SD_SDIO_CMD SDIO_1_CMD
#define SD_SDIO_D0 SDIO_1_D0
#define SD_SDIO_D1 SDIO_1_D1
#define SD_SDIO_D2 SDIO_1_D2
#define SD_SDIO_D3 SDIO_1_D3
// SDSPI
#define SD_SCLK SDIO_1_CLK
#define SD_MOSI SDIO_1_CMD
#define SD_MISO SDIO_1_D0
#define SD_CS SDIO_1_D3

// ESP32C6 SDIO
#define ESP32C6_SDIO_CLK SDIO_2_CLK
#define ESP32C6_SDIO_CMD SDIO_2_CMD
#define ESP32C6_SDIO_D0 SDIO_2_D0
#define ESP32C6_SDIO_D1 SDIO_2_D1
#define ESP32C6_SDIO_D2 SDIO_2_D2
#define ESP32C6_SDIO_D3 SDIO_2_D3

// Extended io
#define EXT_2X8P_SPI_SCLK SPI_1_SCLK
#define EXT_2X8P_SPI_MOSI SPI_1_MOSI
#define EXT_2X8P_SPI_MISO SPI_1_MISO

#define EXT_2X8P_IO_26 26
#define EXT_2X8P_IO_27 27
#define EXT_2X8P_IO_33 33
#define EXT_2X8P_IO_32 32
#define EXT_2X8P_IO_25 25
#define EXT_2X8P_IO_36 36
#define EXT_2X8P_IO_53 53
#define EXT_2X8P_IO_54 54
#define EXT_1X4P_1_IO_47 47
#define EXT_1X4P_1_IO_48 48
#define EXT_1X4P_2_IO_45 45
#define EXT_1X4P_2_IO_46 46

////////////////////////////////////////////////// gpio config //////////////////////////////////////////////////

////////////////////////////////////////////////// other define config //////////////////////////////////////////////////

// XL9535
#define XL9535_IIC_ADDRESS 0x20

// ES8311
#define ES8311_IIC_ADDRESS 0x18

// AW86224
#define AW86224_IIC_ADDRESS 0x58

// SGM38121
#define SGM38121_IIC_ADDRESS 0x28

// PCF8563
#define PCF8563_IIC_ADDRESS 0x51

// BQ27220
#define BQ27220_IIC_ADDRESS 0x55

// ICM20948
#define ICM20948_IIC_ADDRESS 0x68

// HI8561
#define HI8561_SCREEN_WIDTH 540
#define HI8561_SCREEN_HEIGHT 1168
#define HI8561_SCREEN_MIPI_DSI_DPI_CLK_MHZ 60
// #define HI8561_SCREEN_MIPI_DSI_DPI_CLK_MHZ 45
#define HI8561_SCREEN_MIPI_DSI_HSYNC 28
#define HI8561_SCREEN_MIPI_DSI_HBP 26
#define HI8561_SCREEN_MIPI_DSI_HFP 20
#define HI8561_SCREEN_MIPI_DSI_VSYNC 2
#define HI8561_SCREEN_MIPI_DSI_VBP 22
#define HI8561_SCREEN_MIPI_DSI_VFP 200
#define HI8561_SCREEN_DATA_LANE_NUM 2
#define HI8561_SCREEN_LANE_BIT_RATE_MBPS 1000
#define HI8561_TOUCH_IIC_ADDRESS 0x68

// RM69A10
#define RM69A10_SCREEN_WIDTH 568
#define RM69A10_SCREEN_HEIGHT 1232
// #define RM69A10_SCREEN_MIPI_DSI_DPI_CLK_MHZ 80
#define RM69A10_SCREEN_MIPI_DSI_DPI_CLK_MHZ 60
#define RM69A10_SCREEN_MIPI_DSI_HSYNC 50
#define RM69A10_SCREEN_MIPI_DSI_HBP 150
#define RM69A10_SCREEN_MIPI_DSI_HFP 50
#define RM69A10_SCREEN_MIPI_DSI_VSYNC 40
#define RM69A10_SCREEN_MIPI_DSI_VBP 120
#define RM69A10_SCREEN_MIPI_DSI_VFP 80
#define RM69A10_SCREEN_DATA_LANE_NUM 2
#define RM69A10_SCREEN_LANE_BIT_RATE_MBPS 1000

// GT9895
#define GT9895_IIC_ADDRESS 0x5D
#define GT9895_MAX_X_SIZE 1060
#define GT9895_MAX_Y_SIZE 2400
#define GT9895_X_SCALE_FACTOR static_cast<float>(RM69A10_SCREEN_WIDTH) / static_cast<float>(GT9895_MAX_X_SIZE)
#define GT9895_Y_SCALE_FACTOR static_cast<float>(RM69A10_SCREEN_HEIGHT) / static_cast<float>(GT9895_MAX_Y_SIZE)

// CAMERA
#define CAMERA_WIDTH 1920
#define CAMERA_HEIGHT 1080
// #define CAMERA_WIDTH 1280
// #define CAMERA_HEIGHT 720
// #define CAMERA_WIDTH 800
// #define CAMERA_HEIGHT 800
// #define CAMERA_WIDTH 640
// #define CAMERA_HEIGHT 480

#define CAMERA_DATA_LANE_NUM 2
#define CAMERA_LANE_BIT_RATE_MBPS 1000
#define CAMERA_MIPI_DSI_DPI_CLK_MHZ 60

// SD
#define SD_BASE_PATH "/sdcard"

////////////////////////////////////////////////// other define config //////////////////////////////////////////////////