/**
 * @file ra8875.h
 *
 */

#ifndef RA8875_H
#define RA8875_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/*********************
 *      DEFINES
 *********************/
#define RA8875_RST      CONFIG_LV_DISP_PIN_RST
#define RA8875_USE_RST  CONFIG_LV_DISP_USE_RST

// System & Configuration Registers
#define RA8875_REG_PWRR   (0x01)     // Power and Display Control Register (PWRR)
#define RA8875_REG_MRWC   (0x02)     // Memory Read/Write Command (MRWC)
#define RA8875_REG_PCSR   (0x04)     // Pixel Clock Setting Register (PCSR)
#define RA8875_REG_SYSR   (0x10)     // System Configuration Register (SYSR)
#define RA8875_REG_HDWR   (0x14)     // LCD Horizontal Display Width Register (HDWR)
#define RA8875_REG_HNDFTR (0x15)     // Horizontal Non-Display Period Fine Tuning Option Register (HNDFTR)
#define RA8875_REG_HNDR   (0x16)     // LCD Horizontal Non-Display Period Register (HNDR)
#define RA8875_REG_HSTR   (0x17)     // HSYNC Start Position Register (HSTR)
#define RA8875_REG_HPWR   (0x18)     // HSYNC Pulse Width Register (HPWR)
#define RA8875_REG_VDHR0  (0x19)     // LCD Vertical Display Height Register (VDHR0)
#define RA8875_REG_VDHR1  (0x1A)     // LCD Vertical Display Height Register (VDHR1)
#define RA8875_REG_VNDR0  (0x1B)     // LCD Vertical Non-Display Period Register (VNDR0)
#define RA8875_REG_VNDR1  (0x1C)     // LCD Vertical Non-Display Period Register (VNDR1)
#define RA8875_REG_VSTR0  (0x1D)     // VSYNC Start Position Register (VSTR0)
#define RA8875_REG_VSTR1  (0x1E)     // VSYNC Start Position Register (VSTR1)
#define RA8875_REG_VPWR   (0x1F)     // VSYNC Pulse Width Register (VPWR)

// LCD Display Control Registers
#define RA8875_REG_DPCR   (0x20)     // Display Configuration Register (DPCR)

// Active Window & Scroll Window Setting Registers
#define RA8875_REG_HSAW0  (0x30)     // Horizontal Start Point 0 of Active Window (HSAW0)
#define RA8875_REG_HSAW1  (0x31)     // Horizontal Start Point 1 of Active Window (HSAW1)
#define RA8875_REG_VSAW0  (0x32)     // Vertical Start Point 0 of Active Window (VSAW0)
#define RA8875_REG_VSAW1  (0x33)     // Vertical Start Point 1 of Active Window (VSAW1)
#define RA8875_REG_HEAW0  (0x34)     // Horizontal End Point 0 of Active Window (HEAW0)
#define RA8875_REG_HEAW1  (0x35)     // Horizontal End Point 1 of Active Window (HEAW1)
#define RA8875_REG_VEAW0  (0x36)     // Vertical End Point 0 of Active Window (VEAW0)
#define RA8875_REG_VEAW1  (0x37)     // Vertical End Point 1 of Active Window (VEAW1)

// Cursor Setting Registers
#define RA8875_REG_MWCR0  (0x40)     // Memory Write Control Register 0 (MWCR0)
#define RA8875_REG_MWCR1  (0x41)     // Memory Write Control Register 1 (MWCR1)
#define RA8875_REG_CURH0  (0x46)     // Memory Write Cursor Horizontal Position Register 0 (CURH0)
#define RA8875_REG_CURH1  (0x47)     // Memory Write Cursor Horizontal Position Register 1 (CURH1)
#define RA8875_REG_CURV0  (0x48)     // Memory Write Cursor Vertical Position Register 0 (CURV0)
#define RA8875_REG_CURV1  (0x49)     // Memory Write Cursor Vertical Position Register 1 (CURV1)

// Block Transfer Engine(BTE) Control Registers
#define RA8875_REG_LTPR0  (0x52)     // Layer Transparency Register 0 (LTPR0)
#define RA8875_REG_LTPR1  (0x53)     // Layer Transparency Register 1 (LTPR1)

// Touch Panel Control Registers
#define RA8875_REG_TPCR0  (0x70)     // Touch Panel Control Register 0 (TPCR0)
#define RA8875_REG_TPCR1  (0x71)     // Touch Panel Control Register 1 (TPCR1)
#define RA8875_REG_TPXH   (0x72)     // Touch Panel X High Byte Data Register (TPXH)
#define RA8875_REG_TPYH   (0x73)     // Touch Panel Y High Byte Data Register (TPYH)
#define RA8875_REG_TPXYL  (0x74)     // Touch Panel X/Y Low Byte Data Register (TPXYL)

// PLL Setting Registers
#define RA8875_REG_PLLC1  (0x88)     // PLL Control Register 1 (PLLC1)
#define RA8875_REG_PLLC2  (0x89)     // PLL Control Register 2 (PLLC2)

// Memory Clear Register
#define RA8875_REG_MCLR   (0x8E)     // Memory Clear Control Register (MCLR)

// Interrupt Control Registers
#define RA8875_REG_INTC1  (0xF0)     // Interrupt Control Register1 (INTC1)
#define RA8875_REG_INTC2  (0xF1)     // Interrupt Control Register1 (INTC2)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void ra8875_init(void);
void ra8875_enable_backlight(bool backlight);
void ra8875_enable_display(bool enable);
void ra8875_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);

void ra8875_sleep_in(void);
void ra8875_sleep_out(void);

uint8_t ra8875_read_cmd(uint8_t cmd);
void ra8875_write_cmd(uint8_t cmd, uint8_t data);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*RA8875_H*/
