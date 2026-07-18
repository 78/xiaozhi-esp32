#pragma once

#include <lvgl.h>

class LvglFont {
public:
    virtual const lv_font_t* font() const = 0;
    virtual void SetFallback(const lv_font_t* fallback) = 0;
    virtual ~LvglFont() = default;
};

// Built-in font
class LvglBuiltInFont : public LvglFont {
public:
    LvglBuiltInFont(const lv_font_t* font) : font_(*font) {}
    virtual const lv_font_t* font() const override { return &font_; }
    virtual void SetFallback(const lv_font_t* fallback) override { font_.fallback = fallback; }

private:
    lv_font_t font_{};
};

class LvglCBinFont : public LvglFont {
public:
    LvglCBinFont(void* data);
    virtual ~LvglCBinFont();
    virtual const lv_font_t* font() const override { return font_; }
    uint8_t bpp() const {
        if (font_ == nullptr || font_->dsc == nullptr) {
            return 0;
        }
        return static_cast<const lv_font_fmt_txt_dsc_t*>(font_->dsc)->bpp;
    }
    virtual void SetFallback(const lv_font_t* fallback) override {
        if (font_ != nullptr) {
            font_->fallback = fallback;
        }
    }

private:
    lv_font_t* font_;
};
