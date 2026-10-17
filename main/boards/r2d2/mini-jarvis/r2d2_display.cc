#include "r2d2_display.h"

#include "application.h"
#include "board.h"
#include "device_state.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <cstdio>
#include <cstring>
#include <ctime>

LV_FONT_DECLARE(font_noto_sans_basic_16_4);
LV_FONT_DECLARE(font_noto_sans_basic_30_4);
LV_FONT_DECLARE(font_material_symbols_20_4);

#define TAG "R2d2Display"

namespace r2d2 {

namespace {

constexpr int kFaceW = 220, kFaceH = 140;
constexpr float kFaceScale = 0.58f;
constexpr int kWeatherIcon = 44;
constexpr uint32_t kTickMs = 100;
constexpr int64_t kTalkPeriodUs = 300 * 1000;   // speaking mouth changes
constexpr int64_t kIdlePeriodUs = 1200 * 1000;  // listening/thinking glances
constexpr int64_t kErrorHoldUs = 4 * 1000 * 1000;

const char* kWeekdays[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
const char* kMonths[] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};

lv_obj_t* Plain(lv_obj_t* parent) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

lv_obj_t* Label(lv_obj_t* parent, const lv_font_t* font, uint32_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, Rgb(color), 0);
    lv_label_set_text(label, "");
    return label;
}

bool ShowsOwnScreen(DeviceState state) {
    // Starting, Wi-Fi config, activation, upgrade, audio test, fatal error:
    // the stock screen carries instructions the user needs.
    return state == kDeviceStateIdle || state == kDeviceStateConnecting ||
           state == kDeviceStateListening || state == kDeviceStateSpeaking ||
           state == kDeviceStateNotifying;
}

}  // namespace

R2d2Display::R2d2Display(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t panel_handle,
                         int width, int height, int offset_x, int offset_y, bool mirror_x,
                         bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {}

R2d2Display::~R2d2Display() {
    if (timer_) lv_timer_delete(timer_);
}

void R2d2Display::SetupUI() {
    SpiLcdDisplay::SetupUI();  // the stock UI stays underneath (see header)
    DisplayLockGuard lock(this);

    root_ = Plain(lv_screen_active());
    lv_obj_set_style_bg_color(root_, Rgb(kBg), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    // ── idle: Wi-Fi, clock, date, weather ──
    idle_ = Plain(root_);
    wifi_label_ = Label(idle_, &font_material_symbols_20_4, kTextDim);
    lv_obj_align(wifi_label_, LV_ALIGN_TOP_MID, 0, 16);
    clock_label_ = Label(idle_, &lv_font_montserrat_48, kText);
    lv_obj_align(clock_label_, LV_ALIGN_CENTER, 0, -44);
    date_label_ = Label(idle_, &font_noto_sans_basic_16_4, kTextDim);
    lv_obj_align(date_label_, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t* row = lv_obj_create(idle_);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 36);
    weather_icon_ = std::make_unique<WeatherIcon>(row, kWeatherIcon);
    temp_label_ = Label(row, &font_noto_sans_basic_30_4, kText);
    condition_label_ = Label(idle_, &font_noto_sans_basic_16_4, kTextDim);
    lv_obj_align(condition_label_, LV_ALIGN_CENTER, 0, 76);

    // ── conversation: the face and its state label ──
    talk_ = Plain(root_);
    face_ = std::make_unique<Face>(talk_, kFaceW, kFaceH, kFaceScale);
    lv_obj_align(face_->obj(), LV_ALIGN_CENTER, 0, -12);
    state_label_ = Label(talk_, &font_noto_sans_basic_16_4, kTextDim);
    lv_obj_align(state_label_, LV_ALIGN_CENTER, 0, 76);
    lv_obj_add_flag(talk_, LV_OBJ_FLAG_HIDDEN);

    ShowWeather();
    timer_ = lv_timer_create(OnTick, kTickMs, this);
}

void R2d2Display::OnTick(lv_timer_t* timer) {
    auto* self = static_cast<R2d2Display*>(lv_timer_get_user_data(timer));
    self->Refresh();
}

void R2d2Display::SetStatus(const char* status) {
    SpiLcdDisplay::SetStatus(status);  // stock screen (shown during setup states)
    DisplayLockGuard lock(this);
    Refresh();
}

void R2d2Display::SetEmotion(const char* emotion) {
    SpiLcdDisplay::SetEmotion(emotion);
    DisplayLockGuard lock(this);
    emotion_ = emotion ? emotion : "";
    if (emotion_ == "error" || emotion_ == "confused") {  // Core alerts: failure / busy
        error_variant_ = emotion_ == "error" ? "service" : "confused";
        error_until_us_ = esp_timer_get_time() + kErrorHoldUs;
    }
    next_variant_us_ = 0;  // show the new expression now
    Refresh();
}

void R2d2Display::SetChatMessage(const char* role, const char* content) {
    // No subtitles: like the HUD, the face carries the conversation. Custom
    // Core messages (charts, menus) arrive here as role "system" JSON.
    if (role && strcmp(role, "system") == 0 && content && content[0] == '{') {
        ESP_LOGI(TAG, "Core message: %.120s", content);
        return;
    }
    SpiLcdDisplay::SetChatMessage(role, content);  // stock screen, for setup states
}

void R2d2Display::UpdateStatusBar(bool update_all) {
    SpiLcdDisplay::UpdateStatusBar(update_all);
    DisplayLockGuard lock(this);
    if (update_all) last_clock_min_ = -1;
    UpdateClock();
}

void R2d2Display::SetWeather(const Weather& weather) {
    DisplayLockGuard lock(this);
    weather_ = weather;
    weather_dirty_ = true;
    ShowWeather();
}

void R2d2Display::UpdateClock() {
    if (!clock_label_) return;
    const char* wifi = Board::GetInstance().GetNetworkStateIcon();
    if (wifi != wifi_icon_) {
        wifi_icon_ = wifi;
        lv_label_set_text(wifi_label_, wifi ? wifi : "");
    }
    time_t now = time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);  // the Core sends local time as UTC
    if (tm_now.tm_year < 2025 - 1900) return;  // clock not set yet
    int minute = tm_now.tm_hour * 60 + tm_now.tm_min;
    if (minute == last_clock_min_) return;
    last_clock_min_ = minute;
    char text[24];
    strftime(text, sizeof(text), "%H:%M", &tm_now);
    lv_label_set_text(clock_label_, text);
    snprintf(text, sizeof(text), "%s, %d %s", kWeekdays[tm_now.tm_wday], tm_now.tm_mday,
             kMonths[tm_now.tm_mon]);
    lv_label_set_text(date_label_, text);
    if (tm_now.tm_min == 0) weather_dirty_ = true;  // sun ↔ moon at the hour
    if (weather_dirty_) ShowWeather();
}

void R2d2Display::ShowWeather() {
    if (!temp_label_) return;
    weather_dirty_ = false;
    if (!weather_.up) {
        lv_label_set_text(temp_label_, "--°");
        lv_label_set_text(condition_label_, "");
        weather_icon_->Draw("cloud", false);
        return;
    }
    char text[48];
    snprintf(text, sizeof(text), "%.0f°", weather_.temp_c);
    lv_label_set_text(temp_label_, text);
    if (weather_.rain_pct >= 0) {
        snprintf(text, sizeof(text), "%s · %d%%", weather_.condition.c_str(), weather_.rain_pct);
    } else {
        snprintf(text, sizeof(text), "%s", weather_.condition.c_str());
    }
    lv_label_set_text(condition_label_, text);
    time_t now = time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    bool night = tm_now.tm_year >= 2025 - 1900 && (tm_now.tm_hour < 6 || tm_now.tm_hour >= 18);
    weather_icon_->Draw(weather_.icon, night);
}

void R2d2Display::Refresh() {
    if (!root_) return;
    auto state = Application::GetInstance().GetDeviceState();
    if (!ShowsOwnScreen(state)) {
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
        face_state_.clear();
        return;
    }
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Which face, from the device state and the last emotion (face.py states).
    std::string face_state, hint;
    int64_t now = esp_timer_get_time();
    if (state == kDeviceStateIdle) {
        face_state = "";
    } else if (now < error_until_us_) {
        face_state = "error";
        hint = error_variant_;
    } else if (state == kDeviceStateConnecting) {
        face_state = "extra";
        hint = "greeting";
    } else if (state == kDeviceStateListening) {
        if (emotion_ == "thinking") {
            face_state = "thinking";
        } else if (emotion_ == "happy") {  // goodbye from the Core
            face_state = "extra";
            hint = "happy";
        } else {
            face_state = "listening";
        }
    } else {  // speaking / notifying
        face_state = "speaking";
        hint = IsEmotion(emotion_) ? emotion_ : "";
    }

    if (face_state.empty()) {
        lv_obj_add_flag(talk_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(idle_, LV_OBJ_FLAG_HIDDEN);
        face_state_.clear();
        UpdateClock();
        return;
    }
    lv_obj_add_flag(idle_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(talk_, LV_OBJ_FLAG_HIDDEN);

    bool changed = face_state != face_state_ || hint != face_hint_;
    if (!changed && now < next_variant_us_) return;
    face_variant_ = PickVariant(face_state, hint, changed ? "" : face_variant_);
    face_state_ = face_state;
    face_hint_ = hint;
    next_variant_us_ = now + (face_state == "speaking" ? kTalkPeriodUs : kIdlePeriodUs);
    face_->Draw(face_state_, face_variant_);
    lv_label_set_text(state_label_, FaceLabel(face_state_, face_variant_));
    lv_obj_set_style_text_color(state_label_, FaceColor(face_state_), 0);
}

}  // namespace r2d2
