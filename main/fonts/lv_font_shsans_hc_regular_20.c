/* main/fonts/lv_font_shsans_hc_regular_20.c */
#include "lvgl.h" // 或者 "lvgl/lvgl.h" 取決於包含路徑配置
// 實際的LVGL字型C文件會非常長，包含字形數據、查找表等。
// 這裡只是一個示意。

// 假設這是由 LVGL 字體轉換工具生成的字體數據結構
// 實際內容會由工具自動產生

/* Example of how a font might be structured (highly simplified) */
static const uint8_t lv_font_shsans_hc_regular_20_glyph_bitmap[] = {
    /* Pixel data for glyphs */
    /* For example, a small dot */
    0xFF, 0xFF,
    0xFF, 0xFF,
};

static const lv_font_glyph_dsc_t lv_font_shsans_hc_regular_20_glyph_dsc[] = {
    /* Descriptors for glyphs */
    /* Example for one glyph */
    {
        .adv_w = 8, // Advance width
        .box_w = 2, // Bounding box width
        .box_h = 2, // Bounding box height
        .ofs_x = 0, // X offset of the bounding box
        .ofs_y = 0, // Y offset of the bounding box
        .bitmap_index = 0, // Index of the glyph in the bitmap
        .is_placeholder = false,
    },
};

// Note: The actual structure and content of lv_font_fmt_txt_dsc_t might vary
// This is a simplified representation.
// For LVGL v8 and later, the font structure is simpler.

typedef struct {
    lv_font_t font;
    // Additional fields if any for specific format
} lv_font_fmt_txt_dsc_t_custom;


lv_font_t lv_font_shsans_hc_regular_20 = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph dsc*/
    .get_glyph_bitmap = lv_font_get_glyph_bitmap_fmt_txt, /*Function pointer to get glyph bitmap*/
    .line_height = 20,          /*The line_height of the font*/
    .base_line = 3,             /*Base SCRIPT_NAME of the font*/
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -8,
    .underline_thickness = 1,
    .dsc = &(lv_font_fmt_txt_dsc_t_custom){ // This part is tricky and depends on LVGL version & format.
                                           // For fmt_txt, the dsc field itself contains glyph_dsc, glyph_bitmap etc.
                                           // For simplicity, I'm putting dummy pointers here.
                                           // In a real generated file, this would be correctly populated.
        .font.glyph_dsc = lv_font_shsans_hc_regular_20_glyph_dsc, /*Actual glyph dsc array*/
        .font.glyph_bitmap = lv_font_shsans_hc_regular_20_glyph_bitmap, /*Actual glyph bitmap array*/
        // .cmaps = ... , // Character maps
        // .kern_dsc = ... , // Kerning data
        // .unicode_list = ... , // List of unicode characters
        // .unicode_range_start = ...,
        // .unicode_range_end = ...,
    },
};
