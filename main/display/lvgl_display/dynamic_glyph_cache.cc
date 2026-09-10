#include "dynamic_glyph_cache.h"

#include <esp_log.h>
#include <algorithm>
#include <iterator>

#define TAG "DynamicGlyphCache"

DynamicGlyphCache::DynamicGlyphCache() : retain_between_batches_(TextGlyphStorageUsesPsram()) {}

lv_font_t* DynamicGlyphCache::EnsureFont(const lv_font_t* base_font, uint8_t bpp) {
    if (base_font == nullptr || (bpp != 1 && bpp != 4)) {
        return nullptr;
    }
    bool needs_rebuild = !initialized_;
    if (initialized_ && bpp_ != bpp) {
        entries_.clear();
        use_counter_ = 0;
        needs_rebuild = true;
    }
    bpp_ = bpp;
    font_.get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt;
    font_.get_glyph_bitmap = lv_font_get_bitmap_fmt_txt;
    font_.release_glyph = nullptr;
    font_.line_height = base_font->line_height;
    font_.base_line = base_font->base_line;
    font_.subpx = LV_FONT_SUBPX_NONE;
    font_.kerning = LV_FONT_KERNING_NONE;
    font_.static_bitmap = 0;
    font_.underline_position = base_font->underline_position;
    font_.underline_thickness = base_font->underline_thickness;
    font_.fallback = nullptr;
    font_.user_data = nullptr;
    font_.dsc = &dsc_;
    initialized_ = true;
    if (needs_rebuild) {
        Rebuild();
    }
    return &font_;
}

size_t DynamicGlyphCache::BitmapBytes() const {
    size_t total = 0;
    for (const auto& entry : entries_) {
        total += entry.bitmap.size();
    }
    return total;
}

bool DynamicGlyphCache::AddGlyphs(const std::vector<TextGlyph>& glyphs) {
    if (!initialized_ || glyphs.empty()) {
        return false;
    }

    bool changed = false;
    if (!retain_between_batches_) {
        changed = !entries_.empty();
        entries_.clear();
        use_counter_ = 0;
    }
    size_t bitmap_bytes = BitmapBytes();
    for (const auto& glyph : glyphs) {
        const size_t expected = (static_cast<size_t>(glyph.box_w) * glyph.box_h * bpp_ + 7) / 8;
        if (glyph.codepoint == 0 || glyph.codepoint > 0x10FFFF || glyph.bitmap.size() != expected) {
            ESP_LOGW(TAG, "Rejected glyph U+%04lX", static_cast<unsigned long>(glyph.codepoint));
            continue;
        }

        auto existing = std::find_if(
            entries_.begin(), entries_.end(),
            [&glyph](const Entry& entry) { return entry.codepoint == glyph.codepoint; });
        if (existing != entries_.end()) {
            bitmap_bytes -= existing->bitmap.size();
        } else {
            entries_.emplace_back();
            existing = std::prev(entries_.end());
        }
        existing->codepoint = glyph.codepoint;
        existing->adv_w = glyph.adv_w;
        existing->box_w = glyph.box_w;
        existing->box_h = glyph.box_h;
        existing->ofs_x = glyph.ofs_x;
        existing->ofs_y = glyph.ofs_y;
        existing->bitmap.assign(glyph.bitmap.begin(), glyph.bitmap.end());
        existing->last_use = ++use_counter_;
        bitmap_bytes += glyph.bitmap.size();
        changed = true;
    }

    while (entries_.size() > kMaxGlyphs || bitmap_bytes > kMaxBitmapBytes) {
        auto oldest = std::min_element(
            entries_.begin(), entries_.end(),
            [](const Entry& a, const Entry& b) { return a.last_use < b.last_use; });
        if (oldest == entries_.end()) {
            break;
        }
        bitmap_bytes -= oldest->bitmap.size();
        entries_.erase(oldest);
    }
    if (changed) {
        Rebuild();
    }
    return changed;
}

void DynamicGlyphCache::Clear() {
    if (retain_between_batches_) {
        entries_.clear();
    } else {
        TextGlyphVector<Entry>().swap(entries_);
        TextGlyphVector<uint8_t>().swap(bitmap_blob_);
        TextGlyphVector<uint16_t>().swap(unicode_list_);
        TextGlyphVector<lv_font_fmt_txt_glyph_dsc_t>().swap(glyph_dsc_);
        TextGlyphVector<lv_font_fmt_txt_cmap_t>().swap(cmaps_);
    }
    use_counter_ = 0;
    Rebuild();
}

void DynamicGlyphCache::Rebuild() {
    std::sort(entries_.begin(), entries_.end(),
              [](const Entry& a, const Entry& b) { return a.codepoint < b.codepoint; });

    bitmap_blob_.clear();
    unicode_list_.clear();
    glyph_dsc_.clear();
    cmaps_.clear();
    glyph_dsc_.push_back(lv_font_fmt_txt_glyph_dsc_t{});

    struct Range {
        uint32_t start;
        uint32_t list_begin;
        uint32_t last;
        uint16_t count;
    };
    TextGlyphVector<Range> ranges;
    for (const auto& entry : entries_) {
        if (ranges.empty() || entry.codepoint - ranges.back().start > 0xFFFE) {
            ranges.push_back(Range{entry.codepoint, static_cast<uint32_t>(unicode_list_.size()),
                                   entry.codepoint, 0});
        }
        auto& range = ranges.back();
        unicode_list_.push_back(static_cast<uint16_t>(entry.codepoint - range.start));
        range.last = entry.codepoint;
        ++range.count;

        lv_font_fmt_txt_glyph_dsc_t glyph_dsc{};
        glyph_dsc.bitmap_index = bitmap_blob_.size();
        glyph_dsc.adv_w = entry.adv_w;
        glyph_dsc.box_w = entry.box_w;
        glyph_dsc.box_h = entry.box_h;
        glyph_dsc.ofs_x = entry.ofs_x;
        glyph_dsc.ofs_y = entry.ofs_y;
        glyph_dsc_.push_back(glyph_dsc);
        bitmap_blob_.insert(bitmap_blob_.end(), entry.bitmap.begin(), entry.bitmap.end());
    }

    for (const auto& range : ranges) {
        lv_font_fmt_txt_cmap_t cmap{};
        cmap.range_start = range.start;
        cmap.range_length = static_cast<uint16_t>(range.last - range.start + 1);
        cmap.glyph_id_start = static_cast<uint16_t>(range.list_begin + 1);
        cmap.unicode_list = unicode_list_.data() + range.list_begin;
        cmap.glyph_id_ofs_list = nullptr;
        cmap.list_length = range.count;
        cmap.type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY;
        cmaps_.push_back(cmap);
    }

    dsc_.glyph_bitmap = bitmap_blob_.empty() ? nullptr : bitmap_blob_.data();
    dsc_.glyph_dsc = glyph_dsc_.data();
    dsc_.cmaps = cmaps_.empty() ? nullptr : cmaps_.data();
    dsc_.kern_dsc = nullptr;
    dsc_.kern_scale = 0;
    dsc_.cmap_num = cmaps_.size();
    dsc_.bpp = bpp_;
    dsc_.kern_classes = 0;
    dsc_.bitmap_format = LV_FONT_FMT_TXT_PLAIN;
    dsc_.stride = 0;
    font_.dsc = &dsc_;
}
