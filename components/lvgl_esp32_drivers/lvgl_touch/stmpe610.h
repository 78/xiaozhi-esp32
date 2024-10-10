/**
 * @file STMPE610.h
 */

#ifndef STMPE610_H
#define STMPE610_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <stdint.h>
#include <stdbool.h>
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/*********************
 *      DEFINES
 *********************/
/** 16-bit Chip Version **/
#define STMPE_CHIP_ID 0x00

/** Reset Control **/
#define STMPE_SYS_CTRL1 0x03
#define STMPE_SYS_CTRL1_RESET 0x02

/** Clock Contrl **/
#define STMPE_SYS_CTRL2 0x04

/** SPI Config **/
#define STMPE_SPI_CFG 0x08
#define STMPE_SPI_CFG_MODE0 0x00
#define STMPE_SPI_CFG_MODE1 0x01
#define STMPE_SPI_CFG_MODE2 0x02
#define STMPE_SPI_CFG_MODE3 0x03
#define STMPE_SPI_CFG_AA 0x04

/** Touchscreen controller setup **/
#define STMPE_TSC_CTRL 0x40
#define STMPE_TSC_CTRL_EN 0x01
#define STMPE_TSC_CTRL_XYZ 0x00
#define STMPE_TSC_CTRL_XY 0x02
#define STEMP_TSC_CTRL_TRACK_0 0x00
#define STEMP_TSC_CTRL_TRACK_4 0x10
#define STEMP_TSC_CTRL_TRACK_8 0x20
#define STEMP_TSC_CTRL_TRACK_16 0x30
#define STEMP_TSC_CTRL_TRACK_32 0x40
#define STEMP_TSC_CTRL_TRACK_64 0x50
#define STEMP_TSC_CTRL_TRACK_92 0x60
#define STEMP_TSC_CTRL_TRACK_127 0x70
#define STMPE_TSC_TOUCHED 0x80

/** Interrupt control **/
#define STMPE_INT_CTRL 0x09
#define STMPE_INT_CTRL_POL_HIGH 0x04
#define STMPE_INT_CTRL_POL_LOW 0x00
#define STMPE_INT_CTRL_EDGE 0x02
#define STMPE_INT_CTRL_LEVEL 0x00
#define STMPE_INT_CTRL_ENABLE 0x01
#define STMPE_INT_CTRL_DISABLE 0x00

/** Interrupt enable **/
#define STMPE_INT_EN 0x0A
#define STMPE_INT_EN_TOUCHDET 0x01
#define STMPE_INT_EN_FIFOTH 0x02
#define STMPE_INT_EN_FIFOOF 0x04
#define STMPE_INT_EN_FIFOFULL 0x08
#define STMPE_INT_EN_FIFOEMPTY 0x10
#define STMPE_INT_EN_ADC 0x40
#define STMPE_INT_EN_GPIO 0x80

/** Interrupt status **/
#define STMPE_INT_STA 0x0B
#define STMPE_INT_STA_TOUCHDET 0x01

/** ADC control **/
#define STMPE_ADC_CTRL1 0x20
#define STMPE_ADC_CTRL1_INT   0x00
#define STMPE_ADC_CTRL1_EXT   0x02
#define STMPE_ADC_CTRL1_12BIT 0x08
#define STMPE_ADC_CTRL1_10BIT 0x00
#define STMPE_ADC_CTRL1_36CLK 0x00
#define STMPE_ADC_CTRL1_44CLK 0x10
#define STMPE_ADC_CTRL1_56CLK 0x20
#define STMPE_ADC_CTRL1_64CLK 0x30
#define STMPE_ADC_CTRL1_80CLK 0x40
#define STMPE_ADC_CTRL1_96CLK 0x50
#define STMPE_ADC_CTRL1_124CLK 0x60

/** ADC control **/
#define STMPE_ADC_CTRL2 0x21
#define STMPE_ADC_CTRL2_1_625MHZ 0x00
#define STMPE_ADC_CTRL2_3_25MHZ 0x01
#define STMPE_ADC_CTRL2_6_5MHZ 0x02

/** Touchscreen controller configuration **/
#define STMPE_TSC_CFG 0x41
#define STMPE_TSC_CFG_1SAMPLE 0x00
#define STMPE_TSC_CFG_2SAMPLE 0x40
#define STMPE_TSC_CFG_4SAMPLE 0x80
#define STMPE_TSC_CFG_8SAMPLE 0xC0
#define STMPE_TSC_CFG_DELAY_10US 0x00
#define STMPE_TSC_CFG_DELAY_50US 0x08
#define STMPE_TSC_CFG_DELAY_100US 0x10
#define STMPE_TSC_CFG_DELAY_500US 0x18
#define STMPE_TSC_CFG_DELAY_1MS 0x20
#define STMPE_TSC_CFG_DELAY_5MS 0x28
#define STMPE_TSC_CFG_DELAY_10MS 0x30
#define STMPE_TSC_CFG_DELAY_50MS 0x38
#define STMPE_TSC_CFG_SETTLE_10US 0x00
#define STMPE_TSC_CFG_SETTLE_100US 0x01
#define STMPE_TSC_CFG_SETTLE_500US 0x02
#define STMPE_TSC_CFG_SETTLE_1MS 0x03
#define STMPE_TSC_CFG_SETTLE_5MS 0x04
#define STMPE_TSC_CFG_SETTLE_10MS 0x05
#define STMPE_TSC_CFG_SETTLE_50MS 0x06
#define STMPE_TSC_CFG_SETTLE_100MS 0x07

/** FIFO level to generate interrupt **/
#define STMPE_FIFO_TH 0x4A

/** Current filled level of FIFO **/
#define STMPE_FIFO_SIZE 0x4C

/** Current status of FIFO **/
#define STMPE_FIFO_STA 0x4B
#define STMPE_FIFO_STA_RESET 0x01
#define STMPE_FIFO_STA_OFLOW 0x80
#define STMPE_FIFO_STA_FULL 0x40
#define STMPE_FIFO_STA_EMPTY 0x20
#define STMPE_FIFO_STA_THTRIG 0x10

/** Touchscreen controller drive I **/
#define STMPE_TSC_I_DRIVE 0x58
#define STMPE_TSC_I_DRIVE_20MA 0x00
#define STMPE_TSC_I_DRIVE_50MA 0x01

/** Data port for TSC data address **/
#define STMPE_TSC_DATA_X 0x4D
#define STMPE_TSC_DATA_Y 0x4F
#define STMPE_TSC_DATA_Z 0x51
#define STMPE_TSC_FRACTION_Z 0x56

/** GPIO **/
#define STMPE_GPIO_SET_PIN 0x10
#define STMPE_GPIO_CLR_PIN 0x11
#define STMPE_GPIO_DIR 0x13
#define STMPE_GPIO_ALT_FUNCT 0x17


/** Calibration Constants **/
#define STMPE610_X_MIN       CONFIG_LV_TOUCH_X_MIN
#define STMPE610_Y_MIN       CONFIG_LV_TOUCH_Y_MIN
#define STMPE610_X_MAX       CONFIG_LV_TOUCH_X_MAX
#define STMPE610_Y_MAX       CONFIG_LV_TOUCH_Y_MAX
#define STMPE610_XY_SWAP     CONFIG_LV_TOUCH_XY_SWAP
#define STMPE610_X_INV       CONFIG_LV_TOUCH_INVERT_X
#define STMPE610_Y_INV       CONFIG_LV_TOUCH_INVERT_Y

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void stmpe610_init(void);
bool stmpe610_read(lv_indev_drv_t * drv, lv_indev_data_t * data);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STMPE610_H */
