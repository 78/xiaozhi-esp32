#include "dashboard_ui.h"

#include "board.h"
#include "dashboard_mappings.h"
#include "lvgl_display/lvgl_theme.h"

#include <cstdio>
#include <cstring>
#include <ctime>

// Font declarations must stay at global scope: the corresponding definitions
// live in C font files (dashboard/fonts/*.c and the xiaozhi-fonts component)
// with external C linkage. Putting them in an anonymous namespace produces
// internal-linkage references that fail to link.
LV_FONT_DECLARE(lv_font_digits_72);
LV_FONT_DECLARE(font_weather_symbols_26_4);
LV_FONT_DECLARE(font_weather_symbols_36_4);
LV_FONT_DECLARE(font_noto_sans_basic_30_4);
LV_FONT_DECLARE(font_noto_sans_basic_16_4);

namespace {
extern const uint8_t neutral_gif_start[] asm("_binary_neutral_gif_start");

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

    // (1) Top status bar
    network_label_ = MakeLabel(container_, icon_font, 0x424242, 8, 6, 40, 24);
    top_clock_label_ = MakeLabel(container_, nullptr, 0x424242, 180, 8, 52, 22);
    lv_obj_set_style_text_align(top_clock_label_, LV_TEXT_ALIGN_RIGHT, 0);

    // (2) Weather area
    city_label_ = MakeLabel(container_, nullptr, 0xF57C00, 10, 40, 88, 26);
    aqi_badge_ = MakeLabel(container_, nullptr, 0xFFFFFF, 104, 40, 64, 28);
    lv_obj_set_style_bg_color(aqi_badge_, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_bg_opa(aqi_badge_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(aqi_badge_, 8, 0);
    lv_obj_set_style_pad_all(aqi_badge_, 0, 0);
    lv_obj_set_style_text_align(aqi_badge_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(aqi_badge_, "--");

    weather_icon_label_ =
        MakeLabel(container_, &font_weather_symbols_36_4, 0x607D8B, 172, 34, 60, 40);
    lv_obj_set_style_text_align(weather_icon_label_, LV_TEXT_ALIGN_CENTER, 0);

    aqi_line_label_ = MakeLabel(container_, nullptr, 0x424242, 10, 82, 150, 26);
    weather_text_badge_ =
        MakeLabel(container_, nullptr, 0xFFFFFF, 168, 84, 62, 24);
    lv_obj_set_style_bg_color(weather_text_badge_, lv_color_hex(0x9E9E9E), 0);
    lv_obj_set_style_bg_opa(weather_text_badge_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_align(weather_text_badge_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(weather_text_badge_, "--");

    // (3) Clock area
    hour_label_ = MakeLabel(container_, &lv_font_digits_72, 0x212121, 8, 116, 84, 78);
    colon_label_ = MakeLabel(container_, &lv_font_digits_72, 0x212121, 86, 116, 22, 78);
    minute_label_ =
        MakeLabel(container_, &lv_font_digits_72, 0xFB8C00, 106, 116, 84, 78);
    second_label_ =
        MakeLabel(container_, &font_noto_sans_basic_30_4, 0xE53935, 190, 160, 44, 34);

    // Date and weekday
    date_label_ = MakeLabel(container_, nullptr, 0x616161, 10, 202, 100, 26);
    weekday_label_ = MakeLabel(container_, nullptr, 0x616161, 150, 202, 80, 26);
    lv_obj_set_style_text_align(weekday_label_, LV_TEXT_ALIGN_RIGHT, 0);

    // (4) Environment area: thermometer row
    temp_icon_ = MakeLabel(container_, &font_weather_symbols_26_4, 0xE53935, 8, 234, 30, 28);
    char icon_utf8[5];
    CodepointToUtf8(0x1F321, icon_utf8);
    lv_label_set_text(temp_icon_, icon_utf8);

    temp_bar_ = lv_bar_create(container_);
    lv_obj_set_pos(temp_bar_, 40, 244);
    lv_obj_set_size(temp_bar_, 84, 10);
    lv_bar_set_range(temp_bar_, 0, 100);
    lv_obj_set_style_bg_color(temp_bar_, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(temp_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(temp_bar_, lv_color_hex(0x1E88E5), LV_PART_INDICATOR);
    lv_bar_set_value(temp_bar_, 0, LV_ANIM_OFF);

    // 16px so "-10°C"/"100%" (50px at 20px) fit the 46px box without GIF overlap.
    temp_value_ = MakeLabel(container_, &font_noto_sans_basic_16_4, 0x424242, 126, 238, 46, 22);

    // Humidity row
    humid_icon_ =
        MakeLabel(container_, &font_weather_symbols_26_4, 0x1E88E5, 8, 268, 30, 28);
    CodepointToUtf8(0x1F4A7, icon_utf8);
    lv_label_set_text(humid_icon_, icon_utf8);

    humid_bar_ = lv_bar_create(container_);
    lv_obj_set_pos(humid_bar_, 40, 278);
    lv_obj_set_size(humid_bar_, 84, 10);
    lv_bar_set_range(humid_bar_, 0, 100);
    lv_obj_set_style_bg_color(humid_bar_, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(humid_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(humid_bar_, lv_color_hex(0x43A047), LV_PART_INDICATOR);
    lv_bar_set_value(humid_bar_, 0, LV_ANIM_OFF);

    humid_value_ = MakeLabel(container_, &font_noto_sans_basic_16_4, 0x424242, 126, 272, 46, 22);

    // Idle robot GIF
    static lv_img_dsc_t gif_raw;
    memset(&gif_raw, 0, sizeof(gif_raw));
    gif_raw.data = const_cast<uint8_t*>(neutral_gif_start);
    gif_ = std::make_unique<LvglGif>(&gif_raw);
    if (gif_->IsLoaded()) {
        gif_image_ = lv_image_create(container_);
        lv_obj_set_pos(gif_image_, 174, 252);
        gif_->SetFrameCallback([this]() {
            lv_image_set_src(gif_image_, gif_->image_dsc());
        });
        lv_image_set_src(gif_image_, gif_->image_dsc());
        // Create the playback timer, then pause it: the container is hidden at
        // construction; Show() -> Resume() starts it without offscreen decoding.
        gif_->Start();
        gif_->Pause();
    }

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
    gif_->Resume();
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
    gif_->Pause();
}

void DashboardUI::UpdateClock() {
    time_t now = time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    if (tm_now.tm_year < 2025 - 1900) {
        lv_label_set_text(top_clock_label_, "--:--");
        lv_label_set_text(hour_label_, "--");
        lv_label_set_text(colon_label_, ":");
        lv_label_set_text(minute_label_, "--");
        lv_label_set_text(second_label_, "");
        lv_label_set_text(date_label_, "--");
        lv_label_set_text(weekday_label_, "");
        return;
    }

    char buffer[16];
    strftime(buffer, sizeof(buffer), "%H:%M", &tm_now);
    lv_label_set_text(top_clock_label_, buffer);
    strftime(buffer, sizeof(buffer), "%H", &tm_now);
    lv_label_set_text(hour_label_, buffer);
    lv_label_set_text(colon_label_, ":");
    strftime(buffer, sizeof(buffer), "%M", &tm_now);
    lv_label_set_text(minute_label_, buffer);
    strftime(buffer, sizeof(buffer), "%S", &tm_now);
    lv_label_set_text(second_label_, buffer);
    strftime(buffer, sizeof(buffer), "%m-%d", &tm_now);
    lv_label_set_text(date_label_, buffer);
    lv_label_set_text(weekday_label_, WeekdayZh(tm_now.tm_wday));
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

    char icon_utf8[5];
    uint32_t codepoint = WeatherIconToCodepoint(snapshot.icon_code);
    CodepointToUtf8(codepoint, icon_utf8);
    lv_label_set_text(weather_icon_label_, icon_utf8);
    lv_obj_set_style_text_color(weather_icon_label_,
                                lv_color_hex(WeatherIconColor(codepoint)), 0);

    if (!snapshot.valid) {
        const char* status_text = snapshot.key_invalid
                                      ? "密钥无效"
                                      : (snapshot.no_key ? "天气未配置" : "天气获取中...");
        lv_label_set_text(aqi_line_label_, status_text);
        lv_label_set_text(weather_text_badge_, "--");
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
