#include "dashboard_ui.h"

#include "assets.h"
#include "board.h"
#include "dashboard_mappings.h"
#include "solar_terms.h"
#include "lvgl_display/lvgl_image.h"
#include "lvgl_display/lvgl_theme.h"
#include "misc/cache/instance/lv_image_cache.h"

#include <cstdio>
#include <ctime>

// Font declarations must stay at global scope: the corresponding definitions
// live in C font files (dashboard/fonts/*.c) with external C linkage.
// Putting them in an anonymous namespace produces internal-linkage references
// that fail to link.
LV_FONT_DECLARE(lv_font_impact_64);
LV_FONT_DECLARE(lv_font_impact_36);
LV_FONT_DECLARE(font_weather_symbols_26_4);

namespace {

constexpr uint32_t kFadeMs = 300;

lv_obj_t* MakeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t color_hex,
                    lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t* label = lv_label_create(parent);
    // A null font means "inherit the screen text font". Do not cache a font
    // pointer here: LcdDisplay::SetTextFont frees the previous theme font, so
    // a locally-stored pointer would dangle (see activation crash, Task 9).
    if (font != nullptr) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    lv_obj_set_style_text_color(label, lv_color_hex(color_hex), 0);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, h);
    return label;
}

// A pill/rounded badge. The badge height hugs the text plus vertical padding
// (a fixed-height label always draws from the top with no vertical centering).
// The vertical padding is asymmetric on purpose: the Noto glyphs sit a touch
// high in their em box, so one extra pixel on top nudges the text down to the
// visual vertical center requested for every badge.
lv_obj_t* MakeBadge(lv_obj_t* parent, uint32_t bg_color_hex, uint32_t text_color_hex,
                    lv_coord_t x, lv_coord_t y, lv_coord_t radius,
                    lv_coord_t pad_hor, lv_coord_t pad_ver) {
    lv_obj_t* badge = lv_label_create(parent);
    lv_obj_set_style_text_color(badge, lv_color_hex(text_color_hex), 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(bg_color_hex), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, radius, 0);
    lv_obj_set_style_pad_hor(badge, pad_hor, 0);
    lv_obj_set_style_pad_top(badge, pad_ver + 1, 0);
    lv_obj_set_style_pad_bottom(badge, pad_ver, 0);
    lv_obj_set_pos(badge, x, y);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_label_set_text(badge, "");
    return badge;
}

// Pin a content-sized badge to the right margin (8px). Must run after the
// label text is set; forces layout so the measured width is current.
void AlignBadgeRight(lv_obj_t* badge) {
    lv_obj_update_layout(badge);
    lv_obj_set_x(badge, 240 - 8 - lv_obj_get_width(badge));
}

void SetHidden(lv_obj_t* obj, bool hidden) {
    if (obj == nullptr) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

}  // namespace

DashboardUI::DashboardUI(lv_obj_t* parent) {
    auto* theme = static_cast<LvglTheme*>(
        Board::GetInstance().GetDisplay()->GetTheme());
    const lv_font_t* icon_font = theme->icon_font()->font();

    container_ = lv_obj_create(parent);
    lv_obj_remove_style_all(container_);
    lv_obj_set_size(container_, 240, 320);
    lv_obj_set_pos(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(container_, LV_OPA_TRANSP, 0);

    // (1) Top status bar: WiFi + holiday notice (shown on holiday windows).
    network_label_ = MakeLabel(container_, icon_font, 0x424242, 8, 6, 40, 24);

    // Fixed 25px height (y5-30) with 2px top padding; the box is exactly the
    // label's line height so the glyph stays centered on the notice text.
    holiday_badge_label_ = MakeBadge(container_, 0xFDD835, 0x000000,
                                     44, 5, 10, 5, 0);
    lv_obj_set_height(holiday_badge_label_, 23);
    lv_obj_set_style_pad_top(holiday_badge_label_, 1, 0);
    lv_label_set_text(holiday_badge_label_, "节假日");

    // Long notices scroll circularly (marquee) instead of clipping statically.
    holiday_notice_label_ =
        MakeLabel(container_, nullptr, 0x424242, 108, 6, 126, 22);
    lv_label_set_long_mode(holiday_notice_label_,
                           LV_LABEL_LONG_SCROLL_CIRCULAR);
    SetHidden(holiday_badge_label_, true);
    SetHidden(holiday_notice_label_, true);

    // (2) Weather area. The colored QWeather icon is a 64x64 image pinned to
    // the top-right corner (right margin 8); the weather-text pill sits
    // directly below it, left/right edges aligned with the icon.
    weather_icon_image_ = lv_image_create(container_);
    // Pinned to the very top edge; the pill below starts where the icon ends.
    lv_obj_set_pos(weather_icon_image_, 168, 0);

    // The AQI badge shares this row (right side), so the city label is
    // clipped at x104 instead of running underneath it.
    city_label_ = MakeLabel(container_, nullptr, 0x424242, 10, 35, 94, 22);
    lv_label_set_long_mode(city_label_, LV_LABEL_LONG_CLIP);
    // The AQI level badge sits on this row, starting at x106 (aligned with
    // the minute tens label). 3/0 vertical padding gives height 28 (y35-63),
    // clearing the 空气指数 row at y63 and centering the glyph visually.
    aqi_badge_ = MakeBadge(container_, 0x9E9E9E, 0xFFFFFF,
                           106, 35, 10, 6, 0);
    lv_obj_set_style_pad_top(aqi_badge_, 3, 0);
    lv_label_set_text(aqi_badge_, "--");

    aqi_line_label_ = MakeLabel(container_, nullptr, 0x424242,
                                10, 63, 88, 22);

    // The pill hugs the bottom edge of the 64px icon (which ends at y64).
    // Fixed width keeps it aligned with the icon. 3/0 padding makes a 28px
    // strip (y64-92) whose bottom edge meets the clock digits starting y92.
    weather_text_badge_ = MakeBadge(container_, 0x9E9E9E, 0xFFFFFF,
                                    168, 64, 10, 0, 0);
    lv_obj_set_width(weather_text_badge_, 64);
    lv_obj_set_style_pad_top(weather_text_badge_, 3, 0);
    lv_obj_set_style_text_align(weather_text_badge_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(weather_text_badge_, "--");

    // (3) Clock area: Impact digits at y92; glyph bottoms end around y142.
    hour_label_ = MakeLabel(container_, &lv_font_impact_64, 0x424242,
                            8, 92, 70, 56);
    // The colon glyph box is 33px but the font line_height is 52: LVGL places
    // it on the font baseline, so the label must be a full line tall or the
    // lower dot is clipped. y83/h52 draws the glyph at y102-135, centered on
    // the digits (y91-143). Hour glyphs end around x75 and minutes start at
    // x108; the glyph's 6px ofs_x puts label x80 -> glyph x86-96, centered.
    colon_label_ = MakeLabel(container_, &lv_font_impact_64, 0x424242,
                             80, 83, 16, 52);
    minute_label_ = MakeLabel(container_, &lv_font_impact_64, 0xFB8C00,
                              106, 92, 70, 56);
    // Seconds in Impact 36, bottom-aligned with the HH:MM glyph bottoms (143),
    // tucked right next to the minutes. Width 44 fits both digits (~39px); a
    // narrower 36px label clipped the units digit.
    second_label_ = MakeLabel(container_, &lv_font_impact_36, 0xE53935,
                              178, 112, 44, 31);

    // Date and weekday: 9月24日 / 星期四.
    date_label_ = MakeLabel(container_, nullptr, 0x616161, 10, 152, 104, 22);
    weekday_label_ = MakeLabel(container_, nullptr, 0x616161, 138, 152, 94, 22);
    lv_obj_set_style_text_align(weekday_label_, LV_TEXT_ALIGN_RIGHT, 0);

    // Lunar date: black pill with white text; width hugs the content.
    lunar_label_ = MakeBadge(container_, 0x000000, 0xFFFFFF,
                             8, 180, 6, 8, 3);
    lv_obj_set_style_pad_top(lunar_label_, 6, 0);

    // Pill on the right (right edge at the 8px margin): the solar term when
    // today has one, otherwise the traditional festival (e.g. 中秋节);
    // hidden on ordinary days. Its color follows which kind it shows.
    solar_term_label_ = MakeBadge(container_, 0xFFC107, 0x000000,
                                  156, 180, 10, 6, 3);
    lv_obj_set_style_pad_top(solar_term_label_, 6, 0);
    SetHidden(solar_term_label_, true);

    // Suitable/avoid activities (宜忌), each on its own line. The badges are
    // fixed 28x28 perfect circles; the first starts at y218, clear of the
    // lunar pill above, the second at y248.
    yi_badge_ = MakeBadge(container_, 0x43A047, 0xFFFFFF,
                          10, 218, LV_RADIUS_CIRCLE, 6, 2);
    lv_obj_set_size(yi_badge_, 28, 28);
    lv_label_set_text(yi_badge_, "宜");
    // The badge's top padding (3px) pushes its glyph down; offset the content
    // label by the same amount so both texts share one vertical center.
    yi_label_ = MakeLabel(container_, nullptr, 0x424242, 40, 221, 192, 22);
    // Overflow scrolls circularly (marquee) instead of being clipped.
    lv_label_set_long_mode(yi_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

    ji_badge_ = MakeBadge(container_, 0xF4511E, 0xFFFFFF,
                          10, 248, LV_RADIUS_CIRCLE, 6, 2);
    lv_obj_set_size(ji_badge_, 28, 28);
    lv_label_set_text(ji_badge_, "忌");
    ji_label_ = MakeLabel(container_, nullptr, 0x424242, 40, 251, 192, 22);
    lv_label_set_long_mode(ji_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // (4) Environment area: temperature and humidity share one row, shifted
    // down to y286 (more room below than before). Icon glyphs are 28px tall
    // with ofs_y=-5, so the label box starts at the glyph top or it clips.
    temp_icon_ = MakeLabel(container_, &font_weather_symbols_26_4, 0xE53935,
                           4, 286, 28, 28);
    char icon_utf8[5];
    CodepointToUtf8(0x1F321, icon_utf8);
    lv_label_set_text(temp_icon_, icon_utf8);

    temp_bar_ = lv_bar_create(container_);
    lv_obj_set_pos(temp_bar_, 32, 295);
    lv_obj_set_size(temp_bar_, 46, 10);
    lv_bar_set_range(temp_bar_, 0, 100);
    lv_obj_set_style_bg_color(temp_bar_, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(temp_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(temp_bar_, lv_color_hex(0x9E9E9E), LV_PART_INDICATOR);
    lv_bar_set_value(temp_bar_, 0, LV_ANIM_OFF);

    // 16px so "-10°C"/"100%" fit the narrow half-row; clip instead of wrap.
    temp_value_ = MakeLabel(container_, nullptr, 0x424242, 80, 289, 42, 22);
    lv_label_set_long_mode(temp_value_, LV_LABEL_LONG_CLIP);

    humid_icon_ = MakeLabel(container_, &font_weather_symbols_26_4, 0x1E88E5,
                            122, 286, 28, 28);
    CodepointToUtf8(0x1F4A7, icon_utf8);
    lv_label_set_text(humid_icon_, icon_utf8);

    humid_bar_ = lv_bar_create(container_);
    lv_obj_set_pos(humid_bar_, 150, 295);
    lv_obj_set_size(humid_bar_, 46, 10);
    lv_bar_set_range(humid_bar_, 0, 100);
    lv_obj_set_style_bg_color(humid_bar_, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(humid_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(humid_bar_, lv_color_hex(0x9E9E9E), LV_PART_INDICATOR);
    lv_bar_set_value(humid_bar_, 0, LV_ANIM_OFF);

    humid_value_ = MakeLabel(container_, nullptr, 0x424242, 198, 289, 40, 22);
    lv_label_set_long_mode(humid_value_, LV_LABEL_LONG_CLIP);

    // Self-contained timers; callbacks no-op while the dashboard is hidden.
    // DashboardUI is a process-lifetime singleton with no destruction path,
    // so passing the bare `this` pointer as timer user data is safe.
    lv_timer_create(
        [](lv_timer_t* timer) {
            auto* self = static_cast<DashboardUI*>(lv_timer_get_user_data(timer));
            if (self->IsVisible()) {
                self->UpdateClock();
            }
        },
        1000, this);
    lv_timer_create(
        [](lv_timer_t* timer) {
            auto* self = static_cast<DashboardUI*>(lv_timer_get_user_data(timer));
            if (self->IsVisible()) {
                self->UpdateNetwork();
            }
        },
        10000, this);
    // Colon blink: 500ms visible, 500ms hidden -- one full blink per second.
    lv_timer_create(
        [](lv_timer_t* timer) {
            auto* self = static_cast<DashboardUI*>(lv_timer_get_user_data(timer));
            if (!self->IsVisible()) {
                return;
            }
            if (lv_obj_has_flag(self->colon_label_, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(self->colon_label_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(self->colon_label_, LV_OBJ_FLAG_HIDDEN);
            }
        },
        500, this);

}

bool DashboardUI::IsVisible() const {
    return !lv_obj_has_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

void DashboardUI::Show() {
    // Cancel any in-flight hide animation first (its deleted_cb would re-hide us);
    // lv_anim_delete invokes deleted_cb, so clear HIDDEN afterwards.
    lv_anim_delete(container_, nullptr);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* parent = lv_obj_get_parent(container_);
    lv_obj_move_to_index(container_,
                         static_cast<int32_t>(lv_obj_get_child_count(parent)) - 1);
    // Skip the fade-in when already fully opaque (e.g. network recovers while
    // the idle dashboard is on screen) to avoid a full-screen re-flash.
    // Mid fade-out (opa < COVER) still fades in, which correctly cancels it.
    if (lv_obj_get_style_opa(container_, LV_PART_MAIN) != LV_OPA_COVER) {
        lv_obj_fade_in(container_, kFadeMs, 0);
    }
    UpdateClock();
    UpdateNetwork();
    UpdateWeather(WeatherService::GetInstance().GetSnapshot());
    UpdateAlmanac(AlmanacService::GetInstance().GetSnapshot());
}

void DashboardUI::Hide() {
    if (!IsVisible()) {
        return;  // Already hidden: no redundant TRANSP->TRANSP animation
    }
    // Cancel any running fade-in/fade-out first: otherwise this fade-out and
    // an in-flight fade-in both write opa concurrently. A previous fade-out's
    // deleted_cb checks opa==TRANSP, so mid-fade deletion will not mis-hide.
    lv_anim_delete(container_, nullptr);

    // LVGL 9's lv_obj_fade_out() returns void; build the fade manually so a
    // deleted_cb can hide the container when the animation finishes/is replaced.
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, container_);
    lv_anim_set_values(&anim, lv_obj_get_style_opa(container_, LV_PART_MAIN),
                       LV_OPA_TRANSP);
    lv_anim_set_duration(&anim, kFadeMs);
    lv_anim_set_exec_cb(&anim, [](void* var, int32_t value) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(var), value, 0);
    });
    lv_anim_set_deleted_cb(&anim, [](lv_anim_t* anim) {
        lv_obj_t* obj = static_cast<lv_obj_t*>(anim->var);
        if (lv_obj_get_style_opa(obj, LV_PART_MAIN) == LV_OPA_TRANSP) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    });
    lv_anim_start(&anim);
}

void DashboardUI::UpdateClock() {
    time_t now = time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    if (tm_now.tm_year < 2025 - 1900) {
        lv_label_set_text(hour_label_, "--");
        lv_label_set_text(colon_label_, ":");
        lv_obj_clear_flag(colon_label_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(minute_label_, "--");
        lv_label_set_text(second_label_, "");
        lv_label_set_text(date_label_, "--");
        lv_label_set_text(weekday_label_, "");
        return;
    }

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%H", &tm_now);
    lv_label_set_text(hour_label_, buffer);
    // The colon text is constant; a 500ms timer toggles its visibility,
    // giving one blink per second. Clear HIDDEN here so the phase restarts
    // visible every time the dashboard is shown.
    lv_label_set_text(colon_label_, ":");
    lv_obj_clear_flag(colon_label_, LV_OBJ_FLAG_HIDDEN);
    strftime(buffer, sizeof(buffer), "%M", &tm_now);
    lv_label_set_text(minute_label_, buffer);
    strftime(buffer, sizeof(buffer), "%S", &tm_now);
    lv_label_set_text(second_label_, buffer);
    snprintf(buffer, sizeof(buffer), "%d月%d日", tm_now.tm_mon + 1,
             tm_now.tm_mday);
    lv_label_set_text(date_label_, buffer);
    lv_label_set_text(weekday_label_, WeekdayZhFull(tm_now.tm_wday));
}

void DashboardUI::UpdateNetwork() {
    const char* icon = Board::GetInstance().GetNetworkStateIcon();
    if (icon != nullptr) {
        lv_label_set_text(network_label_, icon);
    }
}

void DashboardUI::UpdateWeather(const WeatherSnapshot& snapshot) {
    lv_label_set_text(city_label_,
                      (!snapshot.city.empty()) ? snapshot.city.c_str() : "--");

    AqiLevelInfo level = AqiToLevel(snapshot.aqi);
    lv_label_set_text(aqi_badge_, snapshot.aqi >= 0 ? level.category : "--");
    lv_obj_set_style_bg_color(aqi_badge_, lv_color_hex(level.color_hex), 0);
    lv_obj_set_style_text_color(aqi_badge_, lv_color_hex(level.text_color_hex), 0);

    // Load the colored icon <icon_code>.png from the assets image; fall back
    // to the generic 999 icon if the code is missing from the pack. Skip the
    // whole swap when the displayed icon already matches the snapshot.
    if (!snapshot.icon_code.empty() &&
        snapshot.icon_code != loaded_icon_code_) {
        void* ptr = nullptr;
        size_t size = 0;
        auto& assets = Assets::GetInstance();
        if (!assets.GetAssetData(snapshot.icon_code + ".png", ptr, size)) {
            assets.GetAssetData("999.png", ptr, size);
        }
        if (ptr != nullptr) {
            // Evict the old descriptor's decoded-image/header cache entries
            // BEFORE freeing it. The cache key is the descriptor pointer, and
            // the freshly new-ed wrapper often reuses that exact heap address;
            // without this the stale bitmap (e.g. fog after switching to a
            // sunny city) would be served from cache.
            if (weather_raw_image_ != nullptr) {
                lv_image_cache_drop(weather_raw_image_->image_dsc());
            }
            delete weather_raw_image_;
            weather_raw_image_ = new LvglRawImage(ptr, size);
            lv_image_set_src(weather_icon_image_, weather_raw_image_->image_dsc());
            loaded_icon_code_ = snapshot.icon_code;
        }
    }

    // Weather-text pill color follows the condition; its text is centered.
    lv_obj_set_style_bg_color(weather_text_badge_,
                              lv_color_hex(WeatherBadgeColor(snapshot.icon_code)), 0);

    if (!snapshot.valid) {
        const char* status_text = snapshot.key_invalid
                                      ? "密钥无效"
                                      : (snapshot.no_key ? "天气未配置" : "天气获取中...");
        lv_label_set_text(aqi_line_label_, status_text);
        lv_label_set_text(weather_text_badge_, "--");
        lv_obj_set_style_bg_color(weather_text_badge_,
                                  lv_color_hex(0x9E9E9E), 0);
        lv_bar_set_value(temp_bar_, 0, LV_ANIM_OFF);
        lv_bar_set_value(humid_bar_, 0, LV_ANIM_OFF);
        lv_label_set_text(temp_value_, "--°C");
        lv_label_set_text(humid_value_, "--%");
        return;
    }

    char line[32];
    if (snapshot.aqi >= 0) {
        snprintf(line, sizeof(line), "空气指数 %d", snapshot.aqi);
        lv_label_set_text(aqi_line_label_, line);
    } else {
        lv_label_set_text(aqi_line_label_, "空气指数 --");
    }
    lv_label_set_text(weather_text_badge_, snapshot.weather_text.c_str());

    lv_bar_set_value(temp_bar_, TempToPercent(snapshot.temperature), LV_ANIM_OFF);
    lv_bar_set_value(humid_bar_, snapshot.humidity, LV_ANIM_OFF);
    snprintf(line, sizeof(line), "%d°C", snapshot.temperature);
    lv_label_set_text(temp_value_, line);
    snprintf(line, sizeof(line), "%d%%", snapshot.humidity);
    lv_label_set_text(humid_value_, line);
}

void DashboardUI::UpdateAlmanac(const AlmanacSnapshot& snapshot) {
    almanac_snapshot_ = snapshot;
    RenderAlmanac();
    // A label's own invalidation when its text is set from a background
    // service can be merged away with the surrounding band left unpainted
    // (only a full-area refresh painted it). Invalidate the whole container
    // so the new content is flushed; updates happen at most a few times/day.
    if (IsVisible()) {
        lv_obj_invalidate(container_);
    }
}

void DashboardUI::RenderAlmanac() {
    const auto& s = almanac_snapshot_;
    if (!s.valid) {
        // No almanac data (daily quota spent, service unreachable...): keep
        // the whole layout visible with placeholders instead of collapsing
        // the block. Only the holiday arrangement (no source for it) is hidden.
        SetHidden(holiday_badge_label_, true);
        SetHidden(holiday_notice_label_, true);

        SetHidden(lunar_label_, false);
        lv_label_set_text(lunar_label_, "农历暂缺");

        // The solar term is derived from a local table, so it is still known
        // without any network request.
        time_t now = time(nullptr);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        const char* term = GetSolarTerm(tm_now.tm_year + 1900,
                                        tm_now.tm_mon + 1, tm_now.tm_mday);
        if (term == nullptr || term[0] == '\0') {
            SetHidden(solar_term_label_, true);
        } else {
            SetHidden(solar_term_label_, false);
            lv_obj_set_style_bg_color(solar_term_label_,
                                      lv_color_hex(0xFFC107), 0);
            lv_obj_set_style_text_color(solar_term_label_,
                                        lv_color_hex(0x000000), 0);
            lv_label_set_text(solar_term_label_, term);
            AlignBadgeRight(solar_term_label_);
        }

        SetHidden(yi_badge_, false);
        SetHidden(yi_label_, false);
        lv_label_set_text(yi_label_, "暂无数据");
        SetHidden(ji_badge_, false);
        SetHidden(ji_label_, false);
        lv_label_set_text(ji_label_, "暂无数据");
        return;
    }

    // Festival arrangement present (today is inside a holiday window):
    // badge + marquee notice are shown at the top.
    bool has_notice = !s.holiday_desc.empty();
    SetHidden(holiday_badge_label_, !has_notice);
    SetHidden(holiday_notice_label_, !has_notice);
    if (has_notice) {
        lv_label_set_text(holiday_notice_label_, s.holiday_desc.c_str());
    }

    SetHidden(lunar_label_, false);
    std::string lunar = "农历" + s.lunar_date;
    if (lunar.size() >= 3 && lunar.compare(lunar.size() - 3, 3, "日") != 0) {
        lunar += "日";
    }
    lv_label_set_text(lunar_label_, lunar.c_str());

    // Prefer the solar term; fall back to the festival (e.g. 中秋节). The
    // pill keeps amber for terms and turns purple for festivals so the two
    // read differently and the palette gains a color.
    const bool has_term = !s.solar_term.empty();
    const std::string& term_or_festival =
        has_term ? s.solar_term : s.festival;
    if (term_or_festival.empty()) {
        SetHidden(solar_term_label_, true);
    } else {
        SetHidden(solar_term_label_, false);
        if (has_term) {
            lv_obj_set_style_bg_color(solar_term_label_,
                                      lv_color_hex(0xFFC107), 0);
            lv_obj_set_style_text_color(solar_term_label_,
                                        lv_color_hex(0x000000), 0);
        } else {
            lv_obj_set_style_bg_color(solar_term_label_,
                                      lv_color_hex(0x7E57C2), 0);
            lv_obj_set_style_text_color(solar_term_label_,
                                        lv_color_hex(0xFFFFFF), 0);
        }
        lv_label_set_text(solar_term_label_, term_or_festival.c_str());
        AlignBadgeRight(solar_term_label_);
    }

    // Join every item; labels scroll circularly when the text overflows.
    auto join = [](const std::vector<std::string>& items) {
        std::string text;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) {
                text += "·";
            }
            text += items[i];
        }
        return text;
    };

    SetHidden(yi_badge_, s.yi.empty());
    SetHidden(yi_label_, s.yi.empty());
    lv_label_set_text(yi_label_, join(s.yi).c_str());

    SetHidden(ji_badge_, s.ji.empty());
    SetHidden(ji_label_, s.ji.empty());
    lv_label_set_text(ji_label_, join(s.ji).c_str());
}
