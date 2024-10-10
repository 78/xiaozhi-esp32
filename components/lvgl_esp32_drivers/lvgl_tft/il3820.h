/**
 * @file il3820.h
 *
 */

#ifndef IL3820_H
#define IL3820_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "sdkconfig.h"

/* Values for Waveshare 2.9inch e-Paper Module, this values shouldn't be
 * swapped to change display orientation */
#define EPD_PANEL_WIDTH         LV_HOR_RES_MAX   /* 128 */
#define EPD_PANEL_HEIGHT        LV_VER_RES_MAX  /* 296 */

/* 128 = panel width */
#define IL3820_COLUMNS          (EPD_PANEL_WIDTH / 8)

#define IL3820_DC_PIN           CONFIG_LV_DISP_PIN_DC
#define IL3820_RST_PIN          CONFIG_LV_DISP_PIN_RST
#define IL3820_USE_RST          CONFIG_LV_DISP_USE_RST
#define IL3820_BUSY_PIN         CONFIG_LV_DISP_PIN_BUSY
#define IL3820_BUSY_LEVEL       1

/* IL3820 commands */
#define IL3820_CMD_GDO_CTRL			0x01
#define IL3820_CMD_GDV_CTRL			0x03
#define IL3820_CMD_SDV_CTRL			0x04
#define IL3820_CMD_SOFTSTART			0x0c
#define IL3820_CMD_GSCAN_START			0x0f
#define IL3820_CMD_SLEEP_MODE			0x10
#define IL3820_CMD_ENTRY_MODE			0x11
#define IL3820_CMD_SW_RESET			0x12
#define IL3820_CMD_TSENS_CTRL			0x1a
#define IL3820_CMD_MASTER_ACTIVATION		0x20
#define IL3820_CMD_UPDATE_CTRL1		0x21
#define IL3820_CMD_UPDATE_CTRL2		0x22
#define IL3820_CMD_WRITE_RAM			0x24
#define IL3820_CMD_WRITE_RED_RAM		0x26
#define IL3820_CMD_VCOM_SENSE			0x28
#define IL3820_CMD_VCOM_SENSE_DURATON		0x29
#define IL3820_CMD_PRGM_VCOM_OTP		0x2a
#define IL3820_CMD_VCOM_VOLTAGE		0x2c
#define IL3820_CMD_PRGM_WS_OTP			0x30
#define IL3820_CMD_UPDATE_LUT			0x32
#define IL3820_CMD_PRGM_OTP_SELECTION		0x36
#define IL3820_CMD_OTP_SELECTION_CTRL		0x37
#define IL3820_CMD_DUMMY_LINE			0x3a
#define IL3820_CMD_GATE_LINE_WIDTH		0x3b
#define IL3820_CMD_BWF_CTRL			0x3c
#define IL3820_CMD_RAM_XPOS_CTRL		0x44
#define IL3820_CMD_RAM_YPOS_CTRL		0x45
#define IL3820_CMD_RAM_XPOS_CNTR		0x4e
#define IL3820_CMD_RAM_YPOS_CNTR		0x4f
#define IL3820_CMD_TERMINATE_FRAME_RW  0xff

/* Data entry sequence modes */
#define IL3820_DATA_ENTRY_MASK			0x07
#define IL3820_DATA_ENTRY_XDYDX		0x00
#define IL3820_DATA_ENTRY_XIYDX		0x01
#define IL3820_DATA_ENTRY_XDYIX		0x02
#define IL3820_DATA_ENTRY_XIYIX		0x03
#define IL3820_DATA_ENTRY_XDYDY		0x04
#define IL3820_DATA_ENTRY_XIYDY		0x05
#define IL3820_DATA_ENTRY_XDYIY		0x06
#define IL3820_DATA_ENTRY_XIYIY		0x07

/* Options for display update */
#define IL3820_CTRL1_INITIAL_UPDATE_LL		0x00
#define IL3820_CTRL1_INITIAL_UPDATE_LH		0x01
#define IL3820_CTRL1_INITIAL_UPDATE_HL		0x02
#define IL3820_CTRL1_INITIAL_UPDATE_HH		0x03

/* Options for display update sequence */
#define IL3820_CTRL2_ENABLE_CLK		0x80
#define IL3820_CTRL2_ENABLE_ANALOG		0x40
#define IL3820_CTRL2_TO_INITIAL		0x08
#define IL3820_CTRL2_TO_PATTERN		0x04
#define IL3820_CTRL2_DISABLE_ANALOG	0x02
#define IL3820_CTRL2_DISABLE_CLK		0x01

#define IL3820_SLEEP_MODE_DSM			0x01
#define IL3820_SLEEP_MODE_PON			0x00

/* time constants in ms */
#define IL3820_RESET_DELAY			20
#define IL3820_BUSY_DELAY			1
// normal wait time max 200ms
#define IL3820_WAIT                20

void il3820_init(void);
void il3820_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
void il3820_fullflush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
void il3820_rounder(lv_disp_drv_t * disp_drv, lv_area_t *area);
void il3820_set_px_cb(lv_disp_drv_t * disp_drv, uint8_t * buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa);
void il3820_sleep_in(void);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* __IL3820_REGS_H__ */

