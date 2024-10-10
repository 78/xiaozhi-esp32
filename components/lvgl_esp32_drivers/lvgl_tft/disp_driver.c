/**
 * @file disp_driver.c
 */

#include "disp_driver.h"
#include "disp_spi.h"

void disp_driver_init(void)
{
#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9341
    ili9341_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9481
    ili9481_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9488
    ili9488_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7789
    st7789_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7796S
    st7796s_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S
    st7735s_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_HX8357
    hx8357_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9486
    ili9486_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SH1107
    sh1107_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306
    ssd1306_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X
    FT81x_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820
    il3820_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_RA8875
    ra8875_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_GC9A01
   GC9A01_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A
   jd79653a_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D
   uc8151d_init();
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9163C
    ili9163c_init();
#endif
}

void disp_driver_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9341
    ili9341_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9481
    ili9481_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9488
    ili9488_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7789
    st7789_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7796S
    st7796s_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S
    st7735s_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_HX8357
	hx8357_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9486
    ili9486_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SH1107
	sh1107_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306
    ssd1306_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X
    FT81x_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820
    il3820_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_RA8875
    ra8875_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_GC9A01
    GC9A01_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A
    jd79653a_lv_fb_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D
    uc8151d_lv_fb_flush(drv, area, color_map);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9163C
    ili9163c_flush(drv, area, color_map);
#endif
}

void disp_driver_rounder(lv_disp_drv_t * disp_drv, lv_area_t * area)
{
#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306
    ssd1306_rounder(disp_drv, area);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SH1107
    sh1107_rounder(disp_drv, area);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820
    il3820_rounder(disp_drv, area);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A
    jd79653a_lv_rounder_cb(disp_drv, area);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D
    uc8151d_lv_rounder_cb(disp_drv, area);
#endif
}

void disp_driver_set_px(lv_disp_drv_t * disp_drv, uint8_t * buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
    lv_color_t color, lv_opa_t opa)
{
#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306
    ssd1306_set_px_cb(disp_drv, buf, buf_w, x, y, color, opa);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SH1107
    sh1107_set_px_cb(disp_drv, buf, buf_w, x, y, color, opa);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820
    il3820_set_px_cb(disp_drv, buf, buf_w, x, y, color, opa);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A
    jd79653a_lv_set_fb_cb(disp_drv, buf, buf_w, x, y, color, opa);
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D
    uc8151d_lv_set_fb_cb(disp_drv, buf, buf_w, x, y, color, opa);
#endif
}
