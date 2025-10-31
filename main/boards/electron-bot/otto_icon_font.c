/*******************************************************************************
 * Size: 20 px
 * Bpp: 1
 * Opts: --bpp 1 --size 20 --no-compress --stride 1 --align 1 --font fontawesome-webfont.ttf --range 61744,61633,61480 --format lvgl -o OTTO_ICON_FONT.c
 ******************************************************************************/

#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef ENABLE_OTTO_ICON_FONT
#define ENABLE_OTTO_ICON_FONT 1
#endif

#if ENABLE_OTTO_ICON_FONT

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F028 "" */
    0x0, 0x6, 0x0, 0x10, 0xe0, 0x6, 0x6, 0x3, 0xc6, 0x60, 0xf8, 0x65, 0xff, 0x24, 0xff, 0xe6, 0x4f, 0xfc, 0x49, 0xff, 0x89, 0x3f, 0xf3, 0x27, 0xfe, 0x49, 0x87, 0xc3, 0x20, 0x78, 0xcc, 0x3, 0x3, 0x0,
    0x21, 0xc0, 0x0, 0x30,

    /* U+F0C1 "" */
    0x1e, 0x0, 0xf, 0xc0, 0x7, 0x38, 0x3, 0x87, 0x0, 0xc0, 0xc0, 0x30, 0xb0, 0x7, 0x3c, 0x0, 0xef, 0x0, 0x1f, 0xfe, 0x3, 0xff, 0xc0, 0x7, 0x38, 0x3, 0xe7, 0x0, 0xd0, 0xc0, 0x30, 0x30, 0x6, 0x1c, 0x0,
    0xce, 0x0, 0x1f, 0x0, 0x3, 0x80,

    /* U+F130 "" */
    0x7, 0x0, 0xfe, 0x7, 0xf0, 0x3f, 0x81, 0xfc, 0xf, 0xe0, 0x7f, 0x13, 0xf9, 0x9f, 0xcc, 0xfe, 0x67, 0xf3, 0xbf, 0xb4, 0x73, 0x18, 0x30, 0x7f, 0x0, 0x60, 0x2, 0x1, 0xff, 0x0};

/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
                                                        {.bitmap_index = 0, .adv_w = 297, .box_w = 19, .box_h = 16, .ofs_x = 0, .ofs_y = -1},
                                                        {.bitmap_index = 38, .adv_w = 297, .box_w = 18, .box_h = 18, .ofs_x = 0, .ofs_y = -1},
                                                        {.bitmap_index = 79, .adv_w = 206, .box_w = 13, .box_h = 18, .ofs_x = 0, .ofs_y = -1}};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {0x0, 0x99, 0x108};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {.range_start = 61480, .range_length = 265, .glyph_id_start = 1, .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 3, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t otto_icon_font_dsc = {
#else
static lv_font_fmt_txt_dsc_t otto_icon_font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t OTTO_ICON_FONT = {
#else
lv_font_t OTTO_ICON_FONT = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt, /*Function pointer to get glyph's bitmap*/
    .line_height = 18,                              /*The maximum line height required by the font*/
    .base_line = 1,                                 /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .static_bitmap = 0,
    .dsc = &otto_icon_font_dsc, /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};

#endif /*#if ENABLE_OTTO_ICON_FONT*/
