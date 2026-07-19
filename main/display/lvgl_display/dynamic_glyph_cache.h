#ifndef DYNAMIC_GLYPH_CACHE_H
#define DYNAMIC_GLYPH_CACHE_H

#include "display.h"

#include <lvgl.h>
#include <cstddef>
#include <cstdint>
#include <vector>

class DynamicGlyphCache {
public:
    static constexpr size_t kMaxGlyphs = 256;
    static constexpr size_t kMaxBitmapBytes = 64 * 1024;

    DynamicGlyphCache();
    lv_font_t* EnsureFont(const lv_font_t* base_font, uint8_t bpp);
    bool AddGlyphs(const std::vector<TextGlyph>& glyphs);
    void Clear();

private:
    struct Entry {
        uint32_t codepoint = 0;
        uint32_t adv_w = 0;
        uint16_t box_w = 0;
        uint16_t box_h = 0;
        int16_t ofs_x = 0;
        int16_t ofs_y = 0;
        TextGlyphVector<uint8_t> bitmap;
        uint64_t last_use = 0;
    };

    void Rebuild();
    size_t BitmapBytes() const;

    bool initialized_ = false;
    bool retain_between_batches_ = false;
    uint8_t bpp_ = 0;
    uint64_t use_counter_ = 0;
    lv_font_t font_{};
    lv_font_fmt_txt_dsc_t dsc_{};
    TextGlyphVector<Entry> entries_;
    TextGlyphVector<uint8_t> bitmap_blob_;
    TextGlyphVector<uint16_t> unicode_list_;
    TextGlyphVector<lv_font_fmt_txt_glyph_dsc_t> glyph_dsc_;
    TextGlyphVector<lv_font_fmt_txt_cmap_t> cmaps_;
};

#endif
