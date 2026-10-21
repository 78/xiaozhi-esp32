#include "settings_ui.h"

#include "application.h"
#include "audio/audio_codec.h"
#include "board.h"
#include "settings.h"
#include "display/lvgl_display/lvgl_theme.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <material_symbols.h>
#include <ssid_manager.h>
#include <wifi_manager.h>

#include <algorithm>
#include <cstdio>

#define TAG "SettingsUI"

namespace {

constexpr int kMaxScanResults = 20;
constexpr int kRotationChoices[] = {0, 90, 180, 270};

// Layout constants for the 800x1280 panel.
constexpr int kHeaderHeight = 104;
constexpr int kIconButtonSize = 64;
constexpr int kRowHeight = 104;
constexpr int kButtonHeight = 76;
constexpr int kCardRadius = 20;
constexpr int kButtonRadius = 16;
constexpr int kSliderHeight = 20;
constexpr int kKeyboardHeight = 380;  // four rows of ~90 px keys
constexpr int kGap = 20;
constexpr int kBottomBarHeight = 32;

// Shared motion tokens. One set of durations and curves keeps the overlay,
// the page swaps and the press feedback feeling like one system.
constexpr uint32_t kPanelInMs = 320;
constexpr uint32_t kPanelOutMs = 240;
constexpr uint32_t kPageInMs = 240;
constexpr int32_t kPressScale = 250;  // 1/256 units -> 0.977

// SenseCraft palette. The green is used as a surface, never as small text on a
// light background: filled green surfaces carry ink-colored content, while
// accent text/icons switch to a deeper green on the light theme.
constexpr uint32_t kSenseCraftGreen = 0x8CC020;
constexpr uint32_t kSenseCraftInk = 0x172126;
constexpr uint32_t kDangerColor = 0xC02B38;
constexpr uint32_t kSuccessColor = 0x2E7D32;

// Settings overlay palettes. The overlay owns its background and surfaces, so
// both themes are spelled out instead of being derived from screen styles.
constexpr uint32_t kLightScreenBackground = 0xF2F4F7;
constexpr uint32_t kLightSurface = 0xFFFFFF;
constexpr uint32_t kLightSurfaceBorder = 0xD0D7DE;
constexpr uint32_t kLightText = 0x1F2328;
constexpr uint32_t kLightAccentInk = 0x1C3B14;
constexpr uint32_t kLightBadge = 0xEAEFF2;

constexpr uint32_t kDarkScreenBackground = 0x121517;
constexpr uint32_t kDarkSurface = 0x1C2128;
constexpr uint32_t kDarkSurfaceBorder = 0x30363D;
constexpr uint32_t kDarkText = 0xF0F6FC;
constexpr uint32_t kDarkAccentInk = 0x9ED13A;
constexpr uint32_t kDarkBadge = 0x283238;

lv_color_t AccentColor() { return lv_color_hex(kSenseCraftGreen); }

// Shared press motion. The pivot must be centered explicitly: LVGL's default
// is the object's top-left corner, which makes the shrink look like the card
// is sliding instead of being pushed in.
void ApplyPressMotion(lv_obj_t* obj) {
    lv_obj_set_style_transform_pivot_x(obj, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, LV_PCT(50), 0);
    lv_obj_set_style_translate_y(obj, 1, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale(obj, kPressScale, LV_STATE_PRESSED);
}

void ApplyPressFeedback(lv_obj_t* obj, lv_color_t base) {
    lv_obj_set_style_bg_color(obj, lv_color_mix(AccentColor(), base, 46), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(obj, AccentColor(), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(obj, 2, LV_STATE_PRESSED);
    ApplyPressMotion(obj);
}

void AnimRootY(void* var, int32_t value) { lv_obj_set_y(static_cast<lv_obj_t*>(var), value); }

void AnimOpa(void* var, int32_t value) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(var), (lv_opa_t)value, 0);
}

void AnimTranslateY(void* var, int32_t value) {
    lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(var), value, 0);
}

// --- Phone-style keyboard -------------------------------------------------
//
// LVGL's built-in keyboard handler dispatches on the exact button texts "abc",
// "ABC" and "1#" (lv_keyboard.c, lv_keyboard_def_event_cb), so the mode keys
// have to carry those labels; a "?123" or a shift arrow would be typed into
// the text area instead of switching the layout. The maps are static because
// lv_keyboard_set_map() stores the pointers.
//
// Widths are relative per row: letters 2, so a row of ten letters is 20. The
// second row is nine letters between two hidden half-width spacers, which is
// what gives it the phone-like indent on both ends.
#define KB_W2 (lv_buttonmatrix_ctrl_t)2
#define KB_FN(w) (lv_buttonmatrix_ctrl_t)((w) | LV_BUTTONMATRIX_CTRL_CHECKED)
#define KB_GAP \
    (lv_buttonmatrix_ctrl_t)(1 | LV_BUTTONMATRIX_CTRL_HIDDEN | LV_BUTTONMATRIX_CTRL_DISABLED)

const char* const kKeyboardLowerMap[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    " ", "a", "s", "d", "f", "g", "h", "j", "k", "l", " ", "\n",
    "ABC", "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
    "1#", ",", " ", ".", LV_SYMBOL_OK, ""};

const lv_buttonmatrix_ctrl_t kKeyboardLowerCtrl[] = {
    KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2,
    KB_GAP, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_GAP,
    KB_FN(3), KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_FN(3),
    KB_FN(3), KB_W2, (lv_buttonmatrix_ctrl_t)10, KB_W2, KB_FN(3)};

const char* const kKeyboardUpperMap[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    " ", "A", "S", "D", "F", "G", "H", "J", "K", "L", " ", "\n",
    "abc", "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "1#", ",", " ", ".", LV_SYMBOL_OK, ""};

const char* const kKeyboardSpecialMap[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
    "abc", "-", "_", "+", "=", "/", "\\", ":", ";", LV_SYMBOL_BACKSPACE, "\n",
    "~", "<", ">", "[", "]", "{", "}", "\"", "'", "?", "\n",
    ",", " ", ".", LV_SYMBOL_OK, ""};

const lv_buttonmatrix_ctrl_t kKeyboardSpecialCtrl[] = {
    KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2,
    KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2,
    KB_FN(3), KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_FN(3),
    KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2, KB_W2,
    KB_W2, (lv_buttonmatrix_ctrl_t)10, KB_W2, KB_FN(3)};

struct ScanTaskArgs {
    SettingsUi* ui;
    uint32_t generation;
};

const char* SignalIcon(int8_t rssi) {
    if (rssi >= -65) {
        return MATERIAL_SYMBOLS_WIFI;
    }
    if (rssi >= -75) {
        return MATERIAL_SYMBOLS_WIFI_2_BAR;
    }
    if (rssi >= -85) {
        return MATERIAL_SYMBOLS_WIFI_1_BAR;
    }
    return MATERIAL_SYMBOLS_WIFI_OFF;
}

const char* VolumeGlyph(int value) {
    if (value <= 0) {
        return MATERIAL_SYMBOLS_VOLUME_OFF;
    }
    if (value < 45) {
        return MATERIAL_SYMBOLS_VOLUME_DOWN;
    }
    return MATERIAL_SYMBOLS_VOLUME_UP;
}

}  // namespace

SettingsUi::SettingsUi(LcdDisplay* display, ConnectCallback connect_cb, CloseCallback close_cb)
    : display_(display), connect_cb_(std::move(connect_cb)), close_cb_(std::move(close_cb)) {}

SettingsUi::~SettingsUi() {
    lv_async_call_cancel(AsyncThunk, this);
    if (root_ != nullptr) {
        lv_anim_delete(root_, nullptr);
        lv_obj_delete(root_);
        root_ = nullptr;
    }
    retired_pages_.clear();
}

// ---------------------------------------------------------------------------
// Event plumbing
// ---------------------------------------------------------------------------

void SettingsUi::EventThunk(lv_event_t* event) {
    auto* ctx = static_cast<EventCtx*>(lv_event_get_user_data(event));
    if (ctx == nullptr || ctx->ui == nullptr) {
        return;
    }
    SettingsUi* ui = ctx->ui;
    // Page transitions delete the object that is currently dispatching this
    // event, so run them from the async queue instead of inside the callback.
    ui->pending_actions_.push_back({ctx->action, ctx->index});
    if (!ui->async_scheduled_) {
        ui->async_scheduled_ = true;
        lv_async_call(AsyncThunk, ui);
    }
}

void SettingsUi::AsyncThunk(void* arg) {
    static_cast<SettingsUi*>(arg)->DrainPendingActions();
}

void SettingsUi::DrainPendingActions() {
    async_scheduled_ = false;
    std::vector<std::pair<Action, int>> actions;
    actions.swap(pending_actions_);
    for (const auto& action : actions) {
        HandleAction(action.first, action.second);
        if (root_ == nullptr) {
            break;
        }
    }
}

void SettingsUi::Bind(lv_obj_t* obj, Action action, int index, lv_event_code_t code) {
    event_ctx_.push_back(std::unique_ptr<EventCtx>(new EventCtx{this, action, index}));
    lv_obj_add_event_cb(obj, EventThunk, code, event_ctx_.back().get());
}

// The slider writes NVS through AudioCodec::SetOutputVolume(), which persists on
// every call, so dragging only previews the value and the codec is written once
// when the finger leaves the knob.
void SettingsUi::VolumeSliderThunk(lv_event_t* event) {
    auto* ui = static_cast<SettingsUi*>(lv_event_get_user_data(event));
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (ui == nullptr || slider == nullptr) {
        return;
    }
    int value = (int)lv_slider_get_value(slider);
    if (ui->volume_value_ != nullptr) {
        lv_label_set_text_fmt(ui->volume_value_, "%d%%", value);
    }
    ui->UpdateVolumeIcon(value);
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
        return;
    }
    auto* codec = Board::GetInstance().GetAudioCodec();
    if (codec == nullptr) {
        return;
    }
    if (value > 0) {
        ui->volume_restore_ = value;
    }
    codec->SetOutputVolume(value);
}

// ---------------------------------------------------------------------------
// Styling helpers
// ---------------------------------------------------------------------------

const lv_font_t* SettingsUi::IconFont(bool large) const {
    if (icon_font_provider_) {
        return icon_font_provider_(large);
    }
    return nullptr;
}

lv_color_t SettingsUi::CardColor() const { return SurfaceColor(); }

bool SettingsUi::IsLightTheme() const {
    // LcdDisplay::SetTheme() updates the active screen's text colour but not
    // its background, so reading the screen style reports the theme that was
    // active at SetupUI() time. Display::GetTheme() is the live source of
    // truth and is what survives a runtime theme switch.
    Theme* theme = display_ != nullptr ? display_->GetTheme() : nullptr;
    if (theme != nullptr) {
        return theme->name() == "light";
    }
    Settings settings("display", false);
    return settings.GetString("theme", "light") == "light";
}

lv_color_t SettingsUi::ScreenBackground() const {
    return lv_color_hex(IsLightTheme() ? kLightScreenBackground : kDarkScreenBackground);
}

lv_color_t SettingsUi::ThemeTextColor() const {
    return lv_color_hex(IsLightTheme() ? kLightText : kDarkText);
}

lv_color_t SettingsUi::AccentInkColor() const {
    return lv_color_hex(IsLightTheme() ? kLightAccentInk : kDarkAccentInk);
}

lv_color_t SettingsUi::SurfaceColor() const {
    return lv_color_hex(IsLightTheme() ? kLightSurface : kDarkSurface);
}

lv_color_t SettingsUi::SurfaceBorderColor() const {
    return lv_color_hex(IsLightTheme() ? kLightSurfaceBorder : kDarkSurfaceBorder);
}

lv_color_t SettingsUi::RaisedSurfaceColor() const {
    lv_color_t surface = SurfaceColor();
    return IsLightTheme() ? lv_color_darken(surface, 10) : lv_color_lighten(surface, 18);
}

lv_color_t SettingsUi::AccentWashStrongColor() const {
    return lv_color_mix(AccentColor(), SurfaceColor(), 54);
}

lv_color_t SettingsUi::PressedSurfaceColor() const {
    return lv_color_mix(AccentColor(), SurfaceColor(), 46);
}

lv_color_t SettingsUi::TrackColor() const {
    lv_color_t surface = SurfaceColor();
    return IsLightTheme() ? lv_color_darken(surface, 22) : lv_color_lighten(surface, 18);
}

lv_color_t SettingsUi::IconBadgeNeutralColor() const {
    return lv_color_hex(IsLightTheme() ? kLightBadge : kDarkBadge);
}

lv_obj_t* SettingsUi::MakeIconLabel(lv_obj_t* parent, const char* glyph, bool large) {
    lv_obj_t* label = lv_label_create(parent);
    const lv_font_t* font = IconFont(large);
    if (font != nullptr) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    lv_label_set_text(label, glyph);
    icon_labels_.push_back({label, large});
    return label;
}

void SettingsUi::RefreshIconFonts() {
    auto refresh = [this](const std::vector<std::pair<lv_obj_t*, bool>>& labels) {
        for (const auto& entry : labels) {
            const lv_font_t* font = IconFont(entry.second);
            if (font != nullptr) {
                lv_obj_set_style_text_font(entry.first, font, 0);
            }
        }
    };
    refresh(icon_labels_);
    // Retired pages are still visible during the cross-fade, so their icon
    // labels must also move to the new font before the old one is freed.
    for (const auto& page : retired_pages_) {
        refresh(page->icon_labels);
    }
}

lv_obj_t* SettingsUi::MakeIconBadge(lv_obj_t* parent, const char* glyph, bool large) {
    const int size = large ? 52 : 42;
    lv_obj_t* badge = lv_obj_create(parent);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, size, size);
    lv_obj_set_style_radius(badge, large ? 16 : 13, 0);
    lv_obj_set_style_bg_color(badge, AccentWashStrongColor(), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(badge, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* label = MakeIconLabel(badge, glyph, large);
    lv_obj_set_style_text_color(label, AccentInkColor(), 0);
    lv_obj_center(label);
    return badge;
}

lv_obj_t* SettingsUi::MakeIconButton(lv_obj_t* parent, const char* glyph, Action action) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, kIconButtonSize, kIconButtonSize);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, CardColor(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_90, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, SurfaceBorderColor(), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_text_color(button, ThemeTextColor(), 0);
    lv_obj_set_style_text_color(button, AccentInkColor(), LV_STATE_PRESSED);
    ApplyPressFeedback(button, CardColor());
    lv_obj_t* icon = MakeIconLabel(button, glyph, false);
    lv_obj_set_style_text_color(icon, ThemeTextColor(), 0);
    lv_obj_center(icon);
    Bind(button, action);
    return button;
}

lv_obj_t* SettingsUi::MakeSectionLabel(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, AccentInkColor(), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_80, 0);
    lv_obj_set_style_pad_left(label, 4, 0);
    lv_obj_set_style_pad_top(label, 8, 0);
    return label;
}

void SettingsUi::RetireCurrentPage() {
    if (header_ == nullptr && body_ == nullptr) {
        return;
    }
    auto page = std::unique_ptr<RetiredPage>(new RetiredPage{
        this, header_, body_, std::move(event_ctx_), std::move(icon_labels_)});
    header_ = nullptr;
    body_ = nullptr;
    textarea_ = nullptr;
    keyboard_ = nullptr;
    volume_slider_ = nullptr;
    volume_value_ = nullptr;
    volume_icon_ = nullptr;
    AnimateRetired(page.get());
    retired_pages_.push_back(std::move(page));
}

void SettingsUi::AnimateRetired(RetiredPage* page) {
    if (page == nullptr) {
        return;
    }
    lv_obj_t* targets[] = {page->header, page->body};
    lv_obj_t* completion_target = page->body != nullptr ? page->body : page->header;
    for (lv_obj_t* target : targets) {
        if (target == nullptr) {
            continue;
        }
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, target);
        lv_anim_set_duration(&anim, 160);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
        lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_exec_cb(&anim, AnimOpa);
        if (target == completion_target) {
            lv_anim_set_completed_cb(&anim, RetiredDoneThunk);
            lv_anim_set_user_data(&anim, page);
        }
        lv_anim_start(&anim);
    }
}

void SettingsUi::RetiredDoneThunk(lv_anim_t* anim) {
    auto* page = static_cast<RetiredPage*>(anim->user_data);
    if (page == nullptr || page->ui == nullptr) {
        return;
    }
    SettingsUi* ui = page->ui;
    if (page->body != nullptr) {
        lv_obj_delete(page->body);
        page->body = nullptr;
    }
    if (page->header != nullptr) {
        lv_obj_delete(page->header);
        page->header = nullptr;
    }
    for (auto it = ui->retired_pages_.begin(); it != ui->retired_pages_.end(); ++it) {
        if (it->get() == page) {
            ui->retired_pages_.erase(it);
            break;
        }
    }
}

lv_obj_t* SettingsUi::BuildPage(const char* title, Action back_action, bool with_refresh) {
    RetireCurrentPage();

    header_ = lv_obj_create(root_);
    lv_obj_remove_style_all(header_);
    lv_obj_set_size(header_, LV_PCT(100), kHeaderHeight);
    lv_obj_align(header_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_hor(header_, kGap + 4, 0);
    lv_obj_set_style_pad_column(header_, kGap, 0);
    lv_obj_set_flex_flow(header_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(header_, LV_SCROLLBAR_MODE_OFF);

    MakeIconButton(header_, MATERIAL_SYMBOLS_ARROW_BACK, back_action);

    lv_obj_t* title_label = lv_label_create(header_);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, ThemeTextColor(), 0);
    lv_obj_set_flex_grow(title_label, 1);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);

    if (with_refresh) {
        MakeIconButton(header_, MATERIAL_SYMBOLS_REFRESH, Action::WifiRescan);
    }

    body_ = lv_obj_create(root_);
    lv_obj_remove_style_all(body_);
    lv_obj_set_size(body_, LV_PCT(100), LV_VER_RES - kHeaderHeight - kBottomBarHeight);
    lv_obj_align(body_, LV_ALIGN_TOP_MID, 0, kHeaderHeight);
    lv_obj_set_style_pad_all(body_, kGap + 4, 0);
    lv_obj_set_style_pad_row(body_, kGap, 0);
    lv_obj_set_flex_flow(body_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(body_, LV_SCROLLBAR_MODE_OFF);

    lv_obj_add_flag(header_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(header_, OnSwipeUpGesture, LV_EVENT_ALL, this);

    AnimatePageIn(header_, body_);
    return body_;
}

void SettingsUi::AnimatePageIn(lv_obj_t* header, lv_obj_t* body) {
    lv_obj_t* targets[] = {header, body};
    for (int i = 0; i < 2; i++) {
        lv_obj_t* target = targets[i];
        if (target == nullptr) {
            continue;
        }
        lv_obj_set_style_opa(target, LV_OPA_TRANSP, 0);
        lv_obj_set_style_translate_y(target, 14, 0);

        lv_anim_t opa;
        lv_anim_init(&opa);
        lv_anim_set_var(&opa, target);
        lv_anim_set_delay(&opa, i == 0 ? 0 : 20);
        lv_anim_set_duration(&opa, kPageInMs);
        lv_anim_set_path_cb(&opa, lv_anim_path_ease_out);
        lv_anim_set_values(&opa, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&opa, AnimOpa);
        lv_anim_start(&opa);

        lv_anim_t slide;
        lv_anim_init(&slide);
        lv_anim_set_var(&slide, target);
        lv_anim_set_delay(&slide, i == 0 ? 0 : 20);
        lv_anim_set_duration(&slide, kPageInMs);
        lv_anim_set_path_cb(&slide, lv_anim_path_ease_out);
        lv_anim_set_values(&slide, 14, 0);
        lv_anim_set_exec_cb(&slide, AnimTranslateY);
        lv_anim_start(&slide);
    }
}

lv_obj_t* SettingsUi::MakeScrollArea(lv_obj_t* parent) {
    lv_obj_t* area = lv_obj_create(parent);
    lv_obj_remove_style_all(area);
    lv_obj_set_width(area, LV_PCT(100));
    lv_obj_set_flex_grow(area, 1);
    lv_obj_set_flex_flow(area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(area, 8, 0);
    lv_obj_set_style_pad_right(area, 4, 0);
    lv_obj_set_scroll_dir(area, LV_DIR_VER);
    lv_obj_add_flag(area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(area, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(area, 6, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(area, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(area, AccentColor(), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(area, LV_OPA_40, LV_PART_SCROLLBAR);
    return area;
}

lv_obj_t* SettingsUi::MakeKeyboard(lv_obj_t* parent, lv_obj_t* textarea, Action ready_action,
                                   bool push_to_bottom) {
    // Push the keyboard to the bottom of the flex column instead of letting it
    // stretch over the whole remaining height (the default stretched every key
    // to ~200 px with the 14 px default font).
    if (push_to_bottom) {
        lv_obj_t* spacer = lv_obj_create(parent);
        lv_obj_remove_style_all(spacer);
        lv_obj_set_width(spacer, LV_PCT(100));
        lv_obj_set_flex_grow(spacer, 1);
        lv_obj_remove_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t* kb = lv_keyboard_create(parent);
    lv_obj_set_width(kb, LV_PCT(100));
    // Landscape (800 px tall) cannot afford the full portrait keyboard: keep
    // the form scrollable instead of pushing its fields off screen.
    int keyboard_height = LV_VER_RES / 3;
    if (keyboard_height > kKeyboardHeight) {
        keyboard_height = kKeyboardHeight;
    }
    lv_obj_set_height(kb, keyboard_height);
    lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(kb, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(kb, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(kb, 8, LV_PART_MAIN);

    // Key caps: theme text font (the keyboard otherwise uses the 14 px LVGL
    // default), rounded, card colour. Function keys are marked CHECKED in the
    // ctrl maps and get one shade more contrast instead of the accent, which
    // used to paint a block of blue over the whole left column and the bottom
    // row. Pressing any key tints it towards the accent.
    const lv_font_t* font = lv_obj_get_style_text_font(lv_screen_active(), LV_PART_MAIN);
    if (font != nullptr) {
        lv_obj_set_style_text_font(kb, font, LV_PART_ITEMS);
    }
    lv_obj_set_style_text_color(kb, ThemeTextColor(), LV_PART_ITEMS);
    const lv_color_t card = CardColor();
    const lv_color_t function_key = RaisedSurfaceColor();
    const lv_color_t pressed = lv_color_mix(AccentColor(), card, 64);
    lv_obj_set_style_radius(kb, 14, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, SurfaceBorderColor(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, card, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_90, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(
        kb, function_key,
        (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(
        kb, pressed, (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_PRESSED);
    lv_obj_set_style_border_color(
        kb, AccentColor(),
        (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(kb, pressed,
                              (lv_style_selector_t)LV_PART_ITEMS |
                                  (lv_style_selector_t)LV_STATE_PRESSED |
                                  (lv_style_selector_t)LV_STATE_CHECKED);

    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, kKeyboardLowerMap, kKeyboardLowerCtrl);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, kKeyboardUpperMap, kKeyboardLowerCtrl);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL, kKeyboardSpecialMap, kKeyboardSpecialCtrl);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);

    lv_keyboard_set_textarea(kb, textarea);
    // The OK key runs the page's primary action instead of only dismissing the
    // keyboard, so a password can be submitted without reaching for 连接.
    Bind(kb, ready_action, 0, LV_EVENT_READY);
    return kb;
}

lv_obj_t* SettingsUi::MakeCard(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, kRowHeight);
    lv_obj_set_style_bg_color(card, CardColor(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_90, 0);
    lv_obj_set_style_radius(card, kCardRadius, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, SurfaceBorderColor(), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_60, 0);
    lv_obj_set_style_pad_hor(card, 22, 0);
    lv_obj_set_style_pad_column(card, 18, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    ApplyPressFeedback(card, CardColor());
    return card;
}

lv_obj_t* SettingsUi::MakeListItem(lv_obj_t* parent, const char* icon, const char* title,
                                   const char* subtitle, Action action, int index,
                                   const char* trailing_icon) {
    lv_obj_t* card = MakeCard(parent);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    Bind(card, action, index);

    if (icon != nullptr) {
        MakeIconBadge(card, icon, true);
    }

    lv_obj_t* texts = lv_obj_create(card);
    lv_obj_remove_style_all(texts);
    // lv_obj is clickable by default and would swallow the tap meant for the
    // card, so the card's CLICKED callback never fired on the text area.
    lv_obj_remove_flag(texts, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(texts, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(texts, 1);
    lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(texts, 2, 0);
    lv_obj_set_scrollbar_mode(texts, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* title_label = lv_label_create(texts);
    lv_label_set_text(title_label, title);
    lv_obj_set_width(title_label, LV_PCT(100));
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(title_label, ThemeTextColor(), 0);

    if (subtitle != nullptr && subtitle[0] != '\0') {
        lv_obj_t* subtitle_label = lv_label_create(texts);
        lv_label_set_text(subtitle_label, subtitle);
        lv_obj_set_width(subtitle_label, LV_PCT(100));
        lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(subtitle_label, ThemeTextColor(), 0);
        lv_obj_set_style_text_opa(subtitle_label, LV_OPA_70, 0);
    }

    if (trailing_icon != nullptr) {
        lv_obj_t* trailing = MakeIconLabel(card, trailing_icon, false);
        lv_obj_set_style_text_opa(trailing, LV_OPA_40, 0);
    }
    return card;
}

lv_obj_t* SettingsUi::MakeTextButton(lv_obj_t* parent, const char* icon, const char* text,
                                     Action action, int index, bool primary) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_width(button, LV_PCT(100));
    lv_obj_set_height(button, kButtonHeight);
    lv_obj_set_style_radius(button, kButtonRadius, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, primary ? AccentColor() : SurfaceBorderColor(), 0);
    lv_obj_set_style_border_opa(button, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(button, primary ? AccentColor() : CardColor(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(button, primary ? lv_color_hex(kSenseCraftInk) : ThemeTextColor(),
                                0);
    if (primary) {
        lv_obj_set_style_bg_color(button, lv_color_darken(AccentColor(), 24), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(button, lv_color_darken(AccentColor(), 24), LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_bg_color(button, PressedSurfaceColor(), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(button, AccentColor(), LV_STATE_PRESSED);
    }
    ApplyPressMotion(button);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(button, 12, 0);
    if (icon != nullptr) {
        MakeIconLabel(button, icon, false);
    }
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    Bind(button, action, index);
    return button;
}

void SettingsUi::StyleTextArea(lv_obj_t* textarea) {
    lv_obj_set_style_bg_color(textarea, CardColor(), 0);
    lv_obj_set_style_bg_opa(textarea, LV_OPA_90, 0);
    lv_obj_set_style_radius(textarea, kButtonRadius, 0);
    lv_obj_set_style_border_width(textarea, 1, 0);
    lv_obj_set_style_border_color(textarea, SurfaceBorderColor(), 0);
    lv_obj_set_style_border_opa(textarea, LV_OPA_60, 0);
    lv_obj_set_style_border_width(textarea, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(textarea, AccentColor(), LV_STATE_FOCUSED);
    lv_obj_set_style_pad_all(textarea, 18, 0);
    lv_obj_set_style_text_color(textarea, ThemeTextColor(), 0);
    lv_obj_set_style_text_opa(textarea, LV_OPA_50, LV_PART_TEXTAREA_PLACEHOLDER);
}

lv_obj_t* SettingsUi::MakeTextArea(lv_obj_t* parent, const char* placeholder, int max_length) {
    lv_obj_t* textarea = lv_textarea_create(parent);
    lv_obj_set_width(textarea, LV_PCT(100));
    lv_obj_set_height(textarea, kButtonHeight);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_max_length(textarea, max_length);
    lv_textarea_set_placeholder_text(textarea, placeholder);
    StyleTextArea(textarea);
    return textarea;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void SettingsUi::Open(SettingsPage page) {
    if (root_ != nullptr) {
        return;
    }
    closing_ = false;
    pending_face_mode_ = -1;
    ESP_LOGI(TAG, "Opening settings overlay on page %d", static_cast<int>(page));

    lv_obj_t* screen = lv_screen_active();
    // Parented to the screen so every label inherits the theme text font. The
    // theme can be reloaded at runtime, which frees the previous font, so no
    // text font pointer is ever cached here.
    root_ = lv_obj_create(screen);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(root_, ScreenBackground(), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(root_, ThemeTextColor(), 0);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(root_, OnSwipeUpGesture, LV_EVENT_ALL, this);
    lv_obj_move_foreground(root_);

    wifi_state_ = WifiSettingsState::Idle;
    switch (page) {
        case SettingsPage::Display:
            ShowDisplaySettings();
            break;
        case SettingsPage::Volume:
            ShowVolumeSettings();
            break;
        case SettingsPage::Face:
            ShowFacePage();
            break;
        case SettingsPage::Wifi:
        default:
            ShowWifiPage();
            break;
    }

    // Bottom swipe handle bar indicator to guide user that the overlay can be dismissed upward
    lv_obj_t* bottom_area = lv_obj_create(root_);
    lv_obj_remove_style_all(bottom_area);
    lv_obj_set_size(bottom_area, LV_PCT(100), kBottomBarHeight);
    lv_obj_align(bottom_area, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(bottom_area, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_area, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(bottom_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bottom_area, OnSwipeUpGesture, LV_EVENT_ALL, this);

    lv_obj_t* handle_pill = lv_obj_create(bottom_area);
    lv_obj_remove_style_all(handle_pill);
    lv_obj_set_size(handle_pill, 100, 6);
    lv_obj_set_style_bg_color(handle_pill, ThemeTextColor(), 0);
    lv_obj_set_style_bg_opa(handle_pill, LV_OPA_50, 0);
    lv_obj_set_style_radius(handle_pill, 3, 0);
    lv_obj_remove_flag(handle_pill, LV_OBJ_FLAG_CLICKABLE);

    AnimateOpen();
}

void SettingsUi::AnimateOpen() {
    if (root_ == nullptr) {
        return;
    }
    lv_obj_update_layout(root_);
    int height = lv_obj_get_height(root_);
    if (height <= 0) {
        height = LV_VER_RES;
    }
    lv_anim_delete(root_, nullptr);
    lv_obj_set_y(root_, -height);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, root_);
    lv_anim_set_duration(&anim, kPanelInMs);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_values(&anim, -height, 0);
    lv_anim_set_exec_cb(&anim, AnimRootY);
    lv_anim_start(&anim);
}

void SettingsUi::Close() {
    if (root_ == nullptr || closing_) {
        return;
    }
    ESP_LOGI(TAG, "Closing settings overlay");
    lv_async_call_cancel(AsyncThunk, this);
    async_scheduled_ = false;
    scan_generation_++;
    pending_actions_.clear();
    closing_ = true;
    AnimateClose();
}

void SettingsUi::AnimateClose() {
    if (root_ == nullptr) {
        return;
    }
    lv_anim_delete(root_, nullptr);
    int height = lv_obj_get_height(root_);
    if (height <= 0) {
        height = LV_VER_RES;
    }
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, root_);
    lv_anim_set_duration(&anim, kPanelOutMs);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    lv_anim_set_values(&anim, lv_obj_get_y(root_), -height);
    lv_anim_set_exec_cb(&anim, AnimRootY);
    lv_anim_set_completed_cb(&anim, [](lv_anim_t* a) {
        auto* self = static_cast<SettingsUi*>(a->user_data);
        if (self != nullptr) {
            self->FinishClose();
        }
    });
    lv_anim_set_user_data(&anim, this);
    lv_anim_start(&anim);
}

void SettingsUi::OnSwipeUpGesture(lv_event_t* event) {
    auto* self = static_cast<SettingsUi*>(lv_event_get_user_data(event));
    if (self == nullptr || self->closing_) {
        return;
    }
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t* indev = lv_indev_active();

    if (code == LV_EVENT_GESTURE) {
        if (indev != nullptr && lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
            self->swipe_tracking_ = false;
            lv_indev_wait_release(indev);
            self->Close();
        }
        return;
    }
    if (code == LV_EVENT_PRESSED) {
        if (indev != nullptr) {
            lv_indev_get_point(indev, &self->swipe_start_);
            self->swipe_tracking_ = true;
        }
        return;
    }
    if (code == LV_EVENT_PRESSING) {
        if (!self->swipe_tracking_ || indev == nullptr) {
            return;
        }
        lv_point_t p = {};
        lv_indev_get_point(indev, &p);
        int dy = p.y - self->swipe_start_.y;
        // Dragging upwards by 60px closes the overlay smoothly
        if (dy <= -60) {
            self->swipe_tracking_ = false;
            lv_indev_wait_release(indev);
            self->Close();
        }
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        self->swipe_tracking_ = false;
    }
}

void SettingsUi::FinishClose() {
    if (root_ == nullptr) {
        return;
    }
    lv_anim_delete(root_, nullptr);
    lv_obj_delete(root_);
    root_ = nullptr;
    header_ = nullptr;
    body_ = nullptr;
    textarea_ = nullptr;
    keyboard_ = nullptr;
    volume_slider_ = nullptr;
    volume_value_ = nullptr;
    volume_icon_ = nullptr;
    icon_labels_.clear();
    event_ctx_.clear();
    retired_pages_.clear();
    closing_ = false;
    if (close_cb_) {
        close_cb_();
    }
}

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------

void SettingsUi::ShowWifiPage() {
    page_ = SettingsPage::Wifi;
    wifi_state_ = WifiSettingsState::Scanning;
    scan_error_.clear();
    lv_obj_t* body = BuildPage("Wi-Fi 网络", Action::Close, true);

    lv_obj_t* card = MakeCard(body);
    MakeIconBadge(card, MATERIAL_SYMBOLS_WIFI, true);
    lv_obj_t* label = lv_label_create(card);
    lv_label_set_text(label, "正在扫描附近的网络 ...");
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_text_opa(label, LV_OPA_70, 0);

    lv_obj_t* spinner = lv_spinner_create(card);
    lv_obj_set_size(spinner, 34, 34);
    lv_obj_set_style_arc_width(spinner, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, AccentColor(), LV_PART_INDICATOR);

    StartWifiScan();
}

void SettingsUi::ShowWifiList() {
    wifi_state_ = WifiSettingsState::SelectWifi;
    lv_obj_t* body = BuildPage("Wi-Fi 网络", Action::Close, true);

    lv_obj_t* area = MakeScrollArea(body);
    MakeSectionLabel(area, "附近的网络");
    if (scan_results_.empty()) {
        lv_obj_t* card = MakeCard(area);
        MakeIconBadge(card, MATERIAL_SYMBOLS_WIFI_OFF, true);
        lv_obj_t* label = lv_label_create(card);
        if (!scan_error_.empty()) {
            lv_label_set_text_fmt(label, "扫描未完成 (%s)，点击右上角重试", scan_error_.c_str());
        } else {
            lv_label_set_text(label, "没有扫描到网络，点击右上角重试");
        }
        lv_obj_set_flex_grow(label, 1);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    } else {
        for (size_t i = 0; i < scan_results_.size(); i++) {
            const auto& item = scan_results_[i];
            char subtitle[32];
            snprintf(subtitle, sizeof(subtitle), "%d dBm", item.rssi);
            MakeListItem(area, SignalIcon(item.rssi), item.ssid.c_str(), subtitle,
                         Action::WifiPickScanned, (int)i,
                         item.encrypted ? MATERIAL_SYMBOLS_LOCK
                                        : MATERIAL_SYMBOLS_KEYBOARD_ARROW_RIGHT);
        }
    }

    lv_obj_t* row = lv_obj_create(body);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), kButtonHeight);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* manual = MakeTextButton(row, MATERIAL_SYMBOLS_EDIT_SQUARE, "手动输入",
                                      Action::WifiManualSsid, 0, false);
    lv_obj_set_width(manual, LV_PCT(31));
    lv_obj_t* saved =
        MakeTextButton(row, MATERIAL_SYMBOLS_KEY, "已保存", Action::WifiSavedList, 0, false);
    lv_obj_set_width(saved, LV_PCT(31));
    lv_obj_t* static_ip = MakeTextButton(row, MATERIAL_SYMBOLS_EDIT_SQUARE, "静态 IP",
                                         Action::WifiStaticIp, 0, false);
    lv_obj_set_width(static_ip, LV_PCT(31));
}

void SettingsUi::StartWifiScan() {
    scan_generation_++;
    auto* args = new ScanTaskArgs{this, scan_generation_};
    ESP_LOGI(TAG, "Scanning (generation %u)", (unsigned)args->generation);

    BaseType_t ok = xTaskCreate(
        [](void* arg) {
            std::unique_ptr<ScanTaskArgs> args(static_cast<ScanTaskArgs*>(arg));
            std::vector<WifiScanItem> results;

            wifi_scan_config_t scan_config = {};
            scan_config.show_hidden = false;
            scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
            scan_config.scan_time.active.min = 100;
            scan_config.scan_time.active.max = 300;

            // Ensure WiFi station mode is started before scanning
            auto& wifi_manager = WifiManager::GetInstance();
            wifi_manager.StartStation();

            esp_err_t err = ESP_FAIL;
            for (int retry = 0; retry < 5; retry++) {
                err = esp_wifi_scan_start(&scan_config, true);
                if (err == ESP_ERR_WIFI_NOT_STARTED) {
                    vTaskDelay(pdMS_TO_TICKS(200));
                    continue;
                }
                if (err == ESP_ERR_WIFI_STATE) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    continue;
                }
                break;
            }
            if (err == ESP_OK) {
                uint16_t ap_count = 0;
                err = esp_wifi_scan_get_ap_num(&ap_count);
                if (err == ESP_OK && ap_count > 0) {
                    uint16_t wanted = std::min<uint16_t>(ap_count, kMaxScanResults);
                    auto* records = new wifi_ap_record_t[wanted];
                    err = esp_wifi_scan_get_ap_records(&wanted, records);
                    if (err == ESP_OK) {
                        for (uint16_t i = 0; i < wanted; i++) {
                            WifiScanItem item;
                            item.ssid = std::string(reinterpret_cast<char*>(records[i].ssid));
                            item.rssi = records[i].rssi;
                            item.encrypted = records[i].authmode != WIFI_AUTH_OPEN;
                            if (!item.ssid.empty()) {
                                results.push_back(item);
                            }
                        }
                    }
                    delete[] records;
                }
            }
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG, "Found %d networks", (int)results.size());
            }
            args->ui->OnScanComplete(std::move(results), err, args->generation);
            vTaskDelete(nullptr);
        },
        "d1001_scan", 4096, args, 3, nullptr);

    if (ok != pdPASS) {
        delete args;
        ESP_LOGE(TAG, "Failed to create the scan task");
    }
}

void SettingsUi::OnScanComplete(std::vector<WifiScanItem> results, esp_err_t error,
                                uint32_t generation) {
    DisplayLockGuard lock(display_);
    if (root_ == nullptr || closing_ || generation != scan_generation_ ||
        wifi_state_ != WifiSettingsState::Scanning) {
        return;
    }
    scan_results_ = std::move(results);
    scan_error_ = (error != ESP_OK) ? esp_err_to_name(error) : "";
    ShowWifiList();
}

void SettingsUi::ShowManualSsid() {
    wifi_state_ = WifiSettingsState::InputSsid;
    lv_obj_t* body = BuildPage("手动输入 SSID", Action::WifiRescan, false);

    textarea_ = MakeTextArea(body, "network name", 32);

    MakeTextButton(body, MATERIAL_SYMBOLS_ARROW_FORWARD, "下一步", Action::WifiManualNext, 0, true);

    keyboard_ = MakeKeyboard(body, textarea_, Action::WifiManualNext);
}

void SettingsUi::ShowPasswordInput(const std::string& ssid, bool encrypted) {
    wifi_state_ = WifiSettingsState::InputPassword;
    pending_ssid_ = ssid;
    pending_encrypted_ = encrypted;

    lv_obj_t* body = BuildPage(ssid.c_str(), Action::WifiPasswordCancel, false);

    textarea_ = MakeTextArea(body, encrypted ? "password" : "(open network)", 64);
    lv_textarea_set_password_mode(textarea_, true);

    lv_obj_t* row = lv_obj_create(body);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), kButtonHeight);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* toggle = MakeTextButton(row, MATERIAL_SYMBOLS_EYEGLASSES, "显示",
                                      Action::WifiTogglePassword, 0, false);
    lv_obj_set_width(toggle, LV_PCT(32));
    lv_obj_t* connect =
        MakeTextButton(row, MATERIAL_SYMBOLS_CHECK, "连接", Action::WifiConnectConfirm, 0, true);
    lv_obj_set_width(connect, LV_PCT(64));

    keyboard_ = MakeKeyboard(body, textarea_, Action::WifiConnectConfirm);
}

void SettingsUi::ShowSavedList() {
    wifi_state_ = WifiSettingsState::SavedList;
    saved_ssids_.clear();
    for (const auto& item : SsidManager::GetInstance().GetSsidList()) {
        saved_ssids_.push_back(item.ssid);
    }

    lv_obj_t* body = BuildPage("已保存的网络", Action::WifiRescan, false);
    lv_obj_t* area = MakeScrollArea(body);

    if (saved_ssids_.empty()) {
        lv_obj_t* card = MakeCard(area);
        MakeIconBadge(card, MATERIAL_SYMBOLS_INFO, true);
        lv_obj_t* label = lv_label_create(card);
        lv_label_set_text(label, "还没有保存任何网络");
        lv_obj_set_flex_grow(label, 1);
        return;
    }

    for (size_t i = 0; i < saved_ssids_.size(); i++) {
        lv_obj_t* card = MakeCard(area);
        MakeIconBadge(card, MATERIAL_SYMBOLS_KEY, true);

        lv_obj_t* name = lv_label_create(card);
        lv_label_set_text(name, saved_ssids_[i].c_str());
        lv_obj_set_flex_grow(name, 1);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

        lv_obj_t* connect = lv_button_create(card);
        lv_obj_set_size(connect, kIconButtonSize, kIconButtonSize);
        lv_obj_set_style_radius(connect, kButtonRadius, 0);
        lv_obj_set_style_shadow_width(connect, 0, 0);
        lv_obj_set_style_bg_color(connect, AccentColor(), 0);
        lv_obj_set_style_bg_color(connect, lv_color_darken(AccentColor(), 24), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(connect, lv_color_hex(kSenseCraftInk), 0);
        ApplyPressMotion(connect);
        lv_obj_center(MakeIconLabel(connect, MATERIAL_SYMBOLS_LINK, false));
        Bind(connect, Action::WifiConnectSaved, (int)i);

        lv_obj_t* remove = lv_button_create(card);
        lv_obj_set_size(remove, kIconButtonSize, kIconButtonSize);
        lv_obj_set_style_radius(remove, kButtonRadius, 0);
        lv_obj_set_style_shadow_width(remove, 0, 0);
        lv_obj_set_style_bg_color(remove, lv_color_hex(kDangerColor), 0);
        lv_obj_set_style_bg_color(remove, lv_color_darken(lv_color_hex(kDangerColor), 24),
                                  LV_STATE_PRESSED);
        lv_obj_set_style_text_color(remove, lv_color_white(), 0);
        ApplyPressMotion(remove);
        lv_obj_center(MakeIconLabel(remove, MATERIAL_SYMBOLS_DELETE, false));
        Bind(remove, Action::WifiDeleteSaved, (int)i);
    }
}

void SettingsUi::ShowConnecting(const std::string& ssid) {
    wifi_state_ = WifiSettingsState::Connecting;
    lv_obj_t* body = BuildPage("连接中", Action::WifiResultBack, false);

    lv_obj_t* box = lv_obj_create(body);
    lv_obj_remove_style_all(box);
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_flex_grow(box, 1);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 24, 0);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* spinner = lv_spinner_create(box);
    lv_obj_set_size(spinner, 96, 96);
    lv_obj_set_style_arc_color(spinner, AccentColor(), LV_PART_INDICATOR);

    lv_obj_t* label = lv_label_create(box);
    lv_label_set_text_fmt(label, "正在连接 %s", ssid.c_str());

    lv_obj_t* hint = lv_label_create(box);
    lv_label_set_text(hint, "最长约 20 秒");
    lv_obj_set_style_opa(hint, LV_OPA_60, 0);
}

void SettingsUi::ShowResult(bool success, const char* message) {
    wifi_state_ = success ? WifiSettingsState::Success : WifiSettingsState::Failed;
    lv_obj_t* body = BuildPage(success ? "连接成功" : "连接失败", Action::WifiResultBack, false);

    lv_obj_t* box = lv_obj_create(body);
    lv_obj_remove_style_all(box);
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_flex_grow(box, 1);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 24, 0);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* icon =
        MakeIconLabel(box, success ? MATERIAL_SYMBOLS_CHECK_CIRCLE : MATERIAL_SYMBOLS_CANCEL,
                      true);
    lv_obj_set_style_text_color(icon, lv_color_hex(success ? kSuccessColor : kDangerColor), 0);

    lv_obj_t* label = lv_label_create(box);
    lv_label_set_text(label, message);
    lv_obj_set_width(label, LV_PCT(90));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    MakeTextButton(body, success ? MATERIAL_SYMBOLS_CHECK : MATERIAL_SYMBOLS_REFRESH,
                   success ? "完成" : "返回列表", Action::WifiResultBack, 0, true);
    MakeTextButton(body, MATERIAL_SYMBOLS_CLOSE, "关闭设置", Action::Close, 0, false);
}

void SettingsUi::OnConnectResult(bool success, const std::string& message) {
    DisplayLockGuard lock(display_);
    operation_active_ = false;
    if (root_ == nullptr || closing_ || wifi_state_ != WifiSettingsState::Connecting) {
        return;
    }
    ESP_LOGI(TAG, "%s: %s", success ? "Success" : "Failed", message.c_str());
    ShowResult(success, message.c_str());
}

// ---------------------------------------------------------------------------
// Rotation
// ---------------------------------------------------------------------------

RotationProfile SettingsUi::MakeRotationProfile(int degrees) {
    // The panel and the touch controller always keep the 0-degree flags that
    // were validated on hardware; every other angle is applied with
    // lv_display_set_rotation(), and LVGL 9 rotates the pointer coordinates
    // itself (lv_indev.c: lv_display_rotate_point), so the touch flags must not
    // be changed per angle or the rotation is applied twice.
    if (degrees != 90 && degrees != 180 && degrees != 270) {
        degrees = 0;
    }
    return RotationProfile{degrees, false, false, false, false, true, true};
}

RotationProfile SettingsUi::LoadRotationProfile() {
    Settings settings("reterminal", false);
    int degrees = settings.GetInt("rotation", 0);
    return MakeRotationProfile(degrees);
}

bool SettingsUi::SaveRotation(int degrees) {
    if (degrees != 0 && degrees != 90 && degrees != 180 && degrees != 270) {
        return false;
    }
    Settings settings("reterminal", true);
    settings.SetInt("rotation", degrees);
    return true;  // committed by the Settings destructor
}

void SettingsUi::ShowStaticIpPage() {
    lv_obj_t* body = BuildPage("静态 IP", Action::WifiRescan, false);
    lv_obj_t* form = MakeScrollArea(body);

    Settings settings("wifi", false);
    bool enabled = settings.GetInt("static_en", 0) != 0;
    const char* keys[4] = {"static_ip", "static_gw", "static_mask", "static_dns"};
    const char* hints[4] = {"IP 地址 (192.168.1.50)", "网关", "子网掩码 (255.255.255.0)",
                            "DNS (可空, 默认网关)"};

    MakeSectionLabel(form, "网络参数");
    lv_obj_t* state = lv_label_create(form);
    lv_label_set_text(state, enabled ? "当前: 静态 IP" : "当前: DHCP 自动获取");
    lv_obj_set_style_text_opa(state, LV_OPA_60, 0);
    lv_obj_set_style_pad_left(state, 4, 0);

    for (int i = 0; i < 4; i++) {
        lv_obj_t* ta = MakeTextArea(form, hints[i], 15);
        std::string value = settings.GetString(keys[i], "");
        if (!value.empty()) {
            lv_textarea_set_text(ta, value.c_str());
        }
        // Clicking a field points the shared keyboard at it.
        lv_obj_add_event_cb(
            ta,
            [](lv_event_t* event) {
                auto* self = static_cast<SettingsUi*>(lv_event_get_user_data(event));
                if (self->keyboard_ != nullptr) {
                    lv_keyboard_set_textarea(
                        self->keyboard_,
                        static_cast<lv_obj_t*>(lv_event_get_current_target(event)));
                }
            },
            LV_EVENT_CLICKED, this);
        static_ta_[i] = ta;
    }

    lv_obj_t* row = lv_obj_create(form);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), kButtonHeight);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t* dhcp = MakeTextButton(row, MATERIAL_SYMBOLS_WIFI, "改用 DHCP",
                                    Action::WifiStaticUseDhcp, 0, false);
    lv_obj_set_width(dhcp, LV_PCT(38));
    lv_obj_t* save = MakeTextButton(row, MATERIAL_SYMBOLS_CHECK, "保存并重启",
                                    Action::WifiStaticSave, 0, true);
    lv_obj_set_width(save, LV_PCT(58));

    keyboard_ = MakeKeyboard(body, static_ta_[0], Action::WifiStaticSave, false);
}

void SettingsUi::ShowFacePage() {
    page_ = SettingsPage::Face;
    int mode = 0;
    std::string endpoint;
    if (face_get_) {
        face_get_(mode, endpoint);
    }
    // Only seed the selection from the saved config on first entry; a tile
    // tap rebuilds the page and must keep the user pick.
    if (pending_face_mode_ < 0) {
        pending_face_mode_ = mode;
    }

    lv_obj_t* body = BuildPage("人脸识别", Action::Close, false);
    lv_obj_t* content = MakeScrollArea(body);
    MakeSectionLabel(content, "视觉模式");

    static const char* kModeNames[4] = {"不感知", "人形感知", "智能问候", "哨兵模式"};
    static const char* kModeHints[4] = {"关闭视觉，不唤醒", "有人即唤醒，不认人脸",
                                        "熟人叫名字，主动问候", "熟人免打扰，陌生人警戒"};
    static const char* kModeIcons[4] = {
        MATERIAL_SYMBOLS_CANCEL,
        MATERIAL_SYMBOLS_PERSON,
        MATERIAL_SYMBOLS_ROBOT_2,
        MATERIAL_SYMBOLS_NOTIFICATIONS,
    };

    lv_obj_t* grid = lv_obj_create(content);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_height(grid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, 12, 0);
    lv_obj_set_style_pad_column(grid, 12, 0);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < 4; i++) {
        bool selected = (i == pending_face_mode_);
        lv_obj_t* tile = lv_button_create(grid);
        lv_obj_set_size(tile, LV_PCT(48), 116);
        lv_obj_set_style_radius(tile, kCardRadius, 0);
        lv_obj_set_style_shadow_width(tile, 0, 0);
        lv_obj_set_style_bg_color(tile, selected ? AccentWashStrongColor() : CardColor(), 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_90, 0);
        lv_obj_set_style_border_width(tile, selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(tile, selected ? AccentColor() : SurfaceBorderColor(), 0);
        lv_obj_set_style_border_opa(tile, selected ? LV_OPA_COVER : LV_OPA_60, 0);
        ApplyPressFeedback(tile, selected ? AccentWashStrongColor() : CardColor());
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(tile, 18, 0);
        lv_obj_set_style_pad_row(tile, 8, 0);

        lv_obj_t* hrow = lv_obj_create(tile);
        lv_obj_remove_style_all(hrow);
        lv_obj_set_size(hrow, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(hrow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(hrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(hrow, 8, 0);
        lv_obj_remove_flag(hrow, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* mode_icon = MakeIconLabel(hrow, kModeIcons[i], false);
        lv_obj_set_style_text_color(mode_icon, selected ? AccentInkColor() : ThemeTextColor(), 0);

        lv_obj_t* name = lv_label_create(hrow);
        lv_label_set_text(name, kModeNames[i]);
        lv_obj_set_style_text_color(name, selected ? AccentInkColor() : ThemeTextColor(), 0);
        lv_obj_set_flex_grow(name, 1);

        if (selected) {
            lv_obj_t* check = MakeIconLabel(hrow, MATERIAL_SYMBOLS_CHECK, false);
            lv_obj_set_style_text_color(check, AccentInkColor(), 0);
        }

        lv_obj_t* hint = lv_label_create(tile);
        lv_label_set_text(hint, kModeHints[i]);
        lv_obj_set_style_text_color(hint, selected ? AccentInkColor() : ThemeTextColor(), 0);
        lv_obj_set_style_text_opa(hint, selected ? LV_OPA_90 : LV_OPA_70, 0);

        Bind(tile, Action::FacePick, i);
    }

    MakeSectionLabel(content, "识别参数");
    {
        Settings settings("face", false);
        int interval = settings.GetInt("interval_s", 5);
        int duration = settings.GetInt("duration_s", 2);
        int cooldown = settings.GetInt("cooldown_s", 8);
        int threshold = settings.GetInt("threshold", 60);
        int known_only = settings.GetInt("known_only", 1);
        (void)interval;
        (void)threshold;
        (void)known_only;
        char text[24];
        snprintf(text, sizeof(text), "%d 秒", duration);
        MakeListItem(content, MATERIAL_SYMBOLS_CHECK, "持续确认", text, Action::FaceParamCycle, 1,
                     MATERIAL_SYMBOLS_KEYBOARD_ARROW_RIGHT);
        snprintf(text, sizeof(text), "%d 秒", cooldown);
        MakeListItem(content, MATERIAL_SYMBOLS_VOLUME_OFF, "唤醒冷却", text, Action::FaceParamCycle,
                     2, MATERIAL_SYMBOLS_KEYBOARD_ARROW_RIGHT);
    }

    MakeSectionLabel(content, "服务配置");
    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, "识别服务地址（智能问候/哨兵模式使用）");
    lv_obj_set_style_text_color(label, ThemeTextColor(), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_60, 0);
    lv_obj_set_style_pad_left(label, 4, 0);

    textarea_ = MakeTextArea(content, "http://192.168.x.x:8001/recognize", 96);
    if (!endpoint.empty()) {
        lv_textarea_set_text(textarea_, endpoint.c_str());
    }

    MakeTextButton(content, MATERIAL_SYMBOLS_CHECK, "保存设置", Action::FaceSave, 0, true);

    // Keyboard at bottom, hidden by default; focused on textarea_ reveals it
    keyboard_ = MakeKeyboard(body, textarea_, Action::FaceSave, false);
    lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(
        textarea_,
        [](lv_event_t* e) {
            auto* self = static_cast<SettingsUi*>(lv_event_get_user_data(e));
            if (self != nullptr && self->keyboard_ != nullptr) {
                lv_obj_clear_flag(self->keyboard_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_scroll_to_view(self->textarea_, LV_ANIM_ON);
            }
        },
        LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(
        textarea_,
        [](lv_event_t* e) {
            auto* self = static_cast<SettingsUi*>(lv_event_get_user_data(e));
            if (self != nullptr && self->keyboard_ != nullptr) {
                lv_obj_add_flag(self->keyboard_, LV_OBJ_FLAG_HIDDEN);
            }
        },
        LV_EVENT_DEFOCUSED, this);
}

void SettingsUi::ShowDisplaySettings() {
    page_ = SettingsPage::Display;
    int current = LoadRotationProfile().degrees;
    if (pending_rotation_ != 0 && pending_rotation_ != 90 && pending_rotation_ != 180 &&
        pending_rotation_ != 270) {
        pending_rotation_ = current;
    }

    lv_obj_t* body = BuildPage("显示与系统", Action::Close, false);
    lv_obj_t* content = MakeScrollArea(body);

    MakeSectionLabel(content, "外观风格");
    {
        lv_obj_t* theme_grid = lv_obj_create(content);
        lv_obj_remove_style_all(theme_grid);
        lv_obj_set_width(theme_grid, LV_PCT(100));
        lv_obj_set_height(theme_grid, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(theme_grid, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(theme_grid, 12, 0);
        lv_obj_set_scrollbar_mode(theme_grid, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(theme_grid, LV_OBJ_FLAG_SCROLLABLE);

        bool is_dark = !IsLightTheme();
        const char* theme_titles[2] = {"明亮模式", "暗黑模式"};
        const char* theme_glyphs[2] = {MATERIAL_SYMBOLS_BRIGHTNESS_6,
                                       MATERIAL_SYMBOLS_BRIGHTNESS_6};

        for (int i = 0; i < 2; i++) {
            bool selected = (i == 1) == is_dark;
            lv_obj_t* tile = lv_button_create(theme_grid);
            lv_obj_remove_style_all(tile);
            lv_obj_set_flex_grow(tile, 1);
            lv_obj_set_height(tile, 66);
            lv_obj_set_style_radius(tile, kCardRadius, 0);
            lv_obj_set_style_shadow_width(tile, 0, 0);
            lv_obj_set_style_bg_color(tile, selected ? AccentWashStrongColor() : CardColor(), 0);
            lv_obj_set_style_bg_opa(tile, LV_OPA_90, 0);
            lv_obj_set_style_border_width(tile, selected ? 2 : 1, 0);
            lv_obj_set_style_border_color(tile, selected ? AccentColor() : SurfaceBorderColor(), 0);
            lv_obj_set_style_border_opa(tile, selected ? LV_OPA_COVER : LV_OPA_60, 0);
            ApplyPressFeedback(tile, selected ? AccentWashStrongColor() : CardColor());
            lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_hor(tile, 16, 0);
            lv_obj_set_style_pad_ver(tile, 0, 0);
            lv_obj_set_style_pad_column(tile, 12, 0);
            lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(tile, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* badge = MakeIconBadge(tile, theme_glyphs[i], false);
            lv_obj_set_size(badge, 40, 40);
            lv_obj_set_style_bg_color(badge, selected ? AccentColor() : IconBadgeNeutralColor(), 0);
            lv_obj_t* badge_icon = lv_obj_get_child(badge, 0);
            if (badge_icon != nullptr) {
                lv_obj_set_style_text_color(
                    badge_icon, selected ? lv_color_hex(kSenseCraftInk) : ThemeTextColor(), 0);
            }

            // One line only: badge on the left, label in the middle, check on
            // the right. The previous stacked title/subtitle pair squeezed two
            // 30 px lines into a 76 px card.
            lv_obj_t* label = lv_label_create(tile);
            lv_label_set_text(label, theme_titles[i]);
            lv_obj_set_style_text_color(label, selected ? AccentInkColor() : ThemeTextColor(), 0);
            lv_obj_set_flex_grow(label, 1);
            lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);

            if (selected) {
                lv_obj_t* check = MakeIconLabel(tile, MATERIAL_SYMBOLS_CHECK, false);
                lv_obj_set_style_text_color(check, AccentInkColor(), 0);
            }

            Bind(tile, Action::ThemePick, i);
        }
    }

    MakeSectionLabel(content, "屏幕方向");

    lv_obj_t* grid = lv_obj_create(content);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_height(grid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(grid, 10, 0);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 4; i++) {
        int degrees = kRotationChoices[i];
        bool selected = degrees == pending_rotation_;
        bool is_current = degrees == current;

        lv_obj_t* tile = lv_button_create(grid);
        lv_obj_remove_style_all(tile);
        lv_obj_set_flex_grow(tile, 1);
        lv_obj_set_height(tile, 58);
        lv_obj_set_style_radius(tile, kCardRadius, 0);
        lv_obj_set_style_shadow_width(tile, 0, 0);
        lv_obj_set_style_bg_color(tile, selected ? AccentWashStrongColor() : CardColor(), 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_90, 0);
        lv_obj_set_style_border_width(tile, selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(tile, selected ? AccentColor() : SurfaceBorderColor(), 0);
        lv_obj_set_style_border_opa(tile, selected ? LV_OPA_COVER : LV_OPA_60, 0);
        ApplyPressFeedback(tile, selected ? AccentWashStrongColor() : CardColor());
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(tile, 6, 0);
        lv_obj_set_style_pad_row(tile, 0, 0);
        lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);

        // Degree and "current" share one label, so the 58 px tile cannot stack
        // them into two lines.
        lv_obj_t* label = lv_label_create(tile);
        char deg_text[24];
        snprintf(deg_text, sizeof(deg_text), "%d°%s", degrees, is_current ? " (当前)" : "");
        lv_label_set_text(label, deg_text);
        lv_obj_set_style_text_color(label, selected ? AccentInkColor() : ThemeTextColor(), 0);
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);

        Bind(tile, Action::RotationPick, i);
    }

    MakeSectionLabel(content, "电源管理");
    {
        Settings settings("power", false);
        int dim_s = settings.GetInt("dim_s", 60);
        int off_s = settings.GetInt("off_s", 0);
        char dim_text[24];
        snprintf(dim_text, sizeof(dim_text), dim_s == 0 ? "关" : "%d 分钟", dim_s / 60);
        char off_text[24];
        snprintf(off_text, sizeof(off_text), off_s == 0 ? "关" : "%d 分钟", off_s / 60);

        auto make_power_item = [&](const char* icon, const char* title, const char* value, Action action) {
            lv_obj_t* card = MakeCard(content);
            lv_obj_set_height(card, 68);
            lv_obj_set_style_pad_hor(card, 16, 0);
            lv_obj_set_style_pad_ver(card, 0, 0);
            lv_obj_set_style_pad_column(card, 12, 0);
            lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
            Bind(card, action, 0);

            lv_obj_t* badge = MakeIconBadge(card, icon, false);
            lv_obj_set_size(badge, 40, 40);
            lv_obj_set_style_bg_color(badge, IconBadgeNeutralColor(), 0);
            lv_obj_t* badge_icon = lv_obj_get_child(badge, 0);
            if (badge_icon != nullptr) {
                lv_obj_set_style_text_color(badge_icon, ThemeTextColor(), 0);
            }

            lv_obj_t* title_label = lv_label_create(card);
            lv_label_set_text(title_label, title);
            lv_obj_set_style_text_color(title_label, ThemeTextColor(), 0);
            lv_obj_set_flex_grow(title_label, 1);
            lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_LEFT, 0);

            lv_obj_t* val_label = lv_label_create(card);
            lv_label_set_text(val_label, value);
            lv_obj_set_style_text_color(val_label, ThemeTextColor(), 0);
            lv_obj_set_style_text_opa(val_label, LV_OPA_70, 0);

            lv_obj_t* trailing = MakeIconLabel(card, MATERIAL_SYMBOLS_KEYBOARD_ARROW_RIGHT, false);
            lv_obj_set_style_text_opa(trailing, LV_OPA_40, 0);
            return card;
        };

        make_power_item(MATERIAL_SYMBOLS_SCHEDULE, "自动息屏", dim_text, Action::SleepDimCycle);
        make_power_item(MATERIAL_SYMBOLS_POWER_SETTINGS_NEW, "无操作关机", off_text, Action::SleepOffCycle);
    }

    bool rotation_changed = (pending_rotation_ != current);
    lv_obj_t* save_btn = MakeTextButton(content, MATERIAL_SYMBOLS_POWER_SETTINGS_NEW,
                   rotation_changed ? "保存屏幕方向并重启" : "屏幕方向已是当前设置",
                   Action::RotationSave, 0, rotation_changed);
    lv_obj_set_height(save_btn, 64);
}

// ---------------------------------------------------------------------------
// Volume
// ---------------------------------------------------------------------------

void SettingsUi::ShowVolumeSettings() {
    page_ = SettingsPage::Volume;
    auto* codec = Board::GetInstance().GetAudioCodec();
    int volume = codec != nullptr ? codec->output_volume() : 0;
    if (volume > 0) {
        volume_restore_ = volume;
    }

    lv_obj_t* body = BuildPage("音量", Action::Close, false);
    MakeSectionLabel(body, "媒体音量");

    lv_obj_t* card = MakeCard(body);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 26, 0);

    lv_obj_t* head = lv_obj_create(card);
    lv_obj_remove_style_all(head);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head, 16, 0);
    lv_obj_remove_flag(head, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(head, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* badge = lv_obj_create(head);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, 52, 52);
    lv_obj_set_style_radius(badge, 16, 0);
    lv_obj_set_style_bg_color(badge, AccentWashStrongColor(), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    volume_icon_ = MakeIconLabel(badge, VolumeGlyph(volume), true);
    lv_obj_set_style_text_color(volume_icon_, AccentInkColor(), 0);
    lv_obj_center(volume_icon_);

    lv_obj_t* title = lv_label_create(head);
    lv_label_set_text(title, "输出音量");
    lv_obj_set_flex_grow(title, 1);

    volume_value_ = lv_label_create(head);
    lv_label_set_text_fmt(volume_value_, "%d%%", volume);
    lv_obj_set_style_text_color(volume_value_, AccentInkColor(), 0);

    volume_slider_ = lv_slider_create(card);
    lv_obj_set_width(volume_slider_, LV_PCT(100));
    lv_obj_set_height(volume_slider_, kSliderHeight);
    lv_slider_set_range(volume_slider_, 0, 100);
    lv_slider_set_value(volume_slider_, volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider_, TrackColor(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(volume_slider_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(volume_slider_, kSliderHeight / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume_slider_, AccentColor(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(volume_slider_, kSliderHeight / 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(volume_slider_, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(volume_slider_, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(volume_slider_, 12, LV_PART_KNOB);
    lv_obj_set_style_border_width(volume_slider_, 4, LV_PART_KNOB);
    lv_obj_set_style_border_color(volume_slider_, AccentColor(), LV_PART_KNOB);
    lv_obj_set_style_shadow_width(volume_slider_, 0, LV_PART_KNOB);
    lv_obj_set_style_transform_scale(
        volume_slider_, 264,
        (lv_style_selector_t)LV_PART_KNOB | (lv_style_selector_t)LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(volume_slider_, 22);
    lv_obj_add_event_cb(volume_slider_, VolumeSliderThunk, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(volume_slider_, VolumeSliderThunk, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(volume_slider_, VolumeSliderThunk, LV_EVENT_PRESS_LOST, this);

    MakeTextButton(body, volume == 0 ? MATERIAL_SYMBOLS_VOLUME_UP : MATERIAL_SYMBOLS_VOLUME_OFF,
                   volume == 0 ? "取消静音" : "静音", Action::VolumeMute, 0, false);

    lv_obj_t* hint = lv_label_create(body);
    lv_label_set_text(hint, "松手时写入设置，拖动过程只做预览。");
    lv_obj_set_width(hint, LV_PCT(100));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_opa(hint, LV_OPA_50, 0);
    lv_obj_set_style_pad_left(hint, 4, 0);
}

void SettingsUi::UpdateVolumeIcon(int volume) {
    if (volume_icon_ == nullptr) {
        return;
    }
    lv_label_set_text(volume_icon_, VolumeGlyph(volume));
}

void SettingsUi::ShowRotationConfirm() {
    lv_obj_t* body = BuildPage("确认", Action::ShowDisplay, false);

    lv_obj_t* box = lv_obj_create(body);
    lv_obj_remove_style_all(box);
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_flex_grow(box, 1);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 24, 0);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* icon = MakeIconLabel(box, MATERIAL_SYMBOLS_WARNING, true);
    lv_obj_set_style_text_color(icon, lv_color_hex(kDangerColor), 0);

    lv_obj_t* label = lv_label_create(box);
    lv_label_set_text_fmt(label, "将屏幕方向设为 %d°，设备会立即重启。", pending_rotation_);
    lv_obj_set_width(label, LV_PCT(90));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    MakeTextButton(body, MATERIAL_SYMBOLS_CHECK, "确认并重启", Action::RotationConfirm, 0, true);
    MakeTextButton(body, MATERIAL_SYMBOLS_CLOSE, "取消", Action::ShowDisplay, 0, false);
}

void SettingsUi::CommitRotation() {
    if (!SaveRotation(pending_rotation_)) {
        return;
    }
    ESP_LOGI(TAG, "Saved rotation=%d, rebooting", pending_rotation_);
    operation_active_ = true;

    lv_obj_t* body = BuildPage("屏幕方向", Action::ShowDisplay, false);
    lv_obj_t* box = lv_obj_create(body);
    lv_obj_remove_style_all(box);
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_flex_grow(box, 1);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 24, 0);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* spinner = lv_spinner_create(box);
    lv_obj_set_size(spinner, 96, 96);
    lv_obj_set_style_arc_color(spinner, AccentColor(), LV_PART_INDICATOR);

    lv_obj_t* label = lv_label_create(box);
    lv_label_set_text_fmt(label, "已保存 %d°，正在重启 ...", pending_rotation_);

    xTaskCreate(
        [](void*) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            Application::GetInstance().Reboot();
            vTaskDelete(nullptr);
        },
        "d1001_reboot", 4096, nullptr, 3, nullptr);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void SettingsUi::HandleAction(Action action, int index) {
    if (closing_) {
        return;
    }
    switch (action) {
        case Action::Close:
            if (operation_active_) {
                return;
            }
            Close();
            return;
        case Action::ShowDisplay:
            ShowDisplaySettings();
            return;
        case Action::ThemePick: {
            bool pick_dark = (index == 1);
            bool is_current_dark = !IsLightTheme();
            if (pick_dark == is_current_dark) {
                return;
            }
            const char* theme_name = pick_dark ? "dark" : "light";
            auto* next_theme = LvglThemeManager::GetInstance().GetTheme(theme_name);
            if (next_theme == nullptr || display_ == nullptr) {
                return;
            }
            display_->SetTheme(next_theme);
            {
                Settings settings("display", true);
                settings.SetString("theme", theme_name);
            }
            // LcdDisplay::SetTheme() repaints the display chrome but leaves the
            // active screen background at the value written by SetupUI(). Sync
            // it explicitly so nothing beneath the overlay keeps the old theme
            // while the page is rebuilt.
            lv_obj_t* screen = lv_screen_active();
            lv_obj_set_style_bg_color(screen, ScreenBackground(), 0);
            lv_obj_set_style_text_color(screen, ThemeTextColor(), 0);
            if (root_ != nullptr) {
                lv_obj_set_style_bg_color(root_, ScreenBackground(), 0);
                lv_obj_set_style_text_color(root_, ThemeTextColor(), 0);
            }
            ShowDisplaySettings();
            return;
        }
        case Action::WifiRescan:
            ShowWifiPage();
            return;
        case Action::WifiSavedList:
            ShowSavedList();
            return;
        case Action::WifiManualSsid:
            ShowManualSsid();
            return;
        case Action::WifiManualNext: {
            if (textarea_ == nullptr) {
                return;
            }
            std::string ssid = lv_textarea_get_text(textarea_);
            if (ssid.empty()) {
                return;
            }
            ShowPasswordInput(ssid, true);
            return;
        }
        case Action::WifiPickScanned:
            if (index >= 0 && index < (int)scan_results_.size()) {
                ShowPasswordInput(scan_results_[index].ssid, scan_results_[index].encrypted);
            }
            return;
        case Action::WifiTogglePassword:
            if (textarea_ != nullptr) {
                lv_textarea_set_password_mode(textarea_, !lv_textarea_get_password_mode(textarea_));
            }
            return;
        case Action::WifiPasswordCancel:
            ShowWifiList();
            return;
        case Action::WifiConnectConfirm: {
            if (operation_active_ || textarea_ == nullptr) {
                return;
            }
            std::string password = lv_textarea_get_text(textarea_);
            if (pending_encrypted_ && password.empty()) {
                return;
            }
            std::string ssid = pending_ssid_;
            operation_active_ = true;
            ShowConnecting(ssid);
            if (connect_cb_) {
                connect_cb_(ssid, password);
            }
            return;
        }
        case Action::WifiConnectSaved: {
            if (operation_active_ || index < 0) {
                return;
            }
            const auto& list = SsidManager::GetInstance().GetSsidList();
            if (index >= (int)list.size()) {
                return;
            }
            std::string ssid = list[index].ssid;
            std::string password = list[index].password;
            pending_ssid_ = ssid;
            pending_encrypted_ = !password.empty();
            operation_active_ = true;
            ShowConnecting(ssid);
            if (connect_cb_) {
                connect_cb_(ssid, password);
            }
            return;
        }
        case Action::WifiDeleteSaved:
            if (index >= 0 && index < (int)SsidManager::GetInstance().GetSsidList().size()) {
                ESP_LOGI(TAG, "Removing saved network at index %d", index);
                SsidManager::GetInstance().RemoveSsid(index);
            }
            ShowSavedList();
            return;
        case Action::WifiResultBack:
            if (operation_active_) {
                return;
            }
            ShowWifiList();
            return;
        case Action::RotationPick:
            if (index >= 0 && index < 4) {
                pending_rotation_ = kRotationChoices[index];
                ShowDisplaySettings();
            }
            return;
        case Action::RotationSave:
            if (operation_active_) {
                return;
            }
            ShowRotationConfirm();
            return;
        case Action::RotationConfirm:
            if (operation_active_) {
                return;
            }
            CommitRotation();
            return;
        case Action::SleepDimCycle: {
            static const int kDimChoices[] = {0, 60, 300, 600};
            Settings settings("power", true);
            int current = settings.GetInt("dim_s", 60);
            int next = kDimChoices[0];
            for (int i = 0; i < 4; i++) {
                if (kDimChoices[i] == current) {
                    next = kDimChoices[(i + 1) % 4];
                    break;
                }
            }
            settings.SetInt("dim_s", next);
            if (power_save_changed_) {
                power_save_changed_();
            }
            ShowDisplaySettings();
            break;
        }
        case Action::SleepOffCycle: {
            static const int kOffChoices[] = {0, 600, 1800};
            Settings settings("power", true);
            int current = settings.GetInt("off_s", 0);
            int next = kOffChoices[0];
            for (int i = 0; i < 3; i++) {
                if (kOffChoices[i] == current) {
                    next = kOffChoices[(i + 1) % 3];
                    break;
                }
            }
            settings.SetInt("off_s", next);
            if (power_save_changed_) {
                power_save_changed_();
            }
            ShowDisplaySettings();
            break;
        }
        case Action::FacePick:
            pending_face_mode_ = index;
            ShowFacePage();
            break;
        case Action::FaceSave: {
            std::string endpoint = textarea_ != nullptr ? lv_textarea_get_text(textarea_) : "";
            if (face_apply_) {
                face_apply_(pending_face_mode_, endpoint);
            }
            Close();
            break;
        }
        case Action::FaceParamCycle: {
            static const char* kKeys[5] = {"interval_s", "duration_s", "cooldown_s", "threshold",
                                           "known_only"};
            static const int kChoices[5][4] = {
                {1, 3, 5, 10}, {1, 2, 3, -1}, {3, 8, 15, 30}, {50, 60, 70, 80}, {1, 0, -1, -1}};
            static const int kDefaults[5] = {5, 2, 8, 60, 1};
            Settings settings("face", true);
            int current = settings.GetInt(kKeys[index], kDefaults[index]);
            int next = kChoices[index][0];
            for (int i = 0; i < 4 && kChoices[index][i] != -1; i++) {
                if (kChoices[index][i] == current) {
                    int j = i + 1;
                    if (j >= 4 || kChoices[index][j] == -1) {
                        j = 0;
                    }
                    next = kChoices[index][j];
                    break;
                }
            }
            settings.SetInt(kKeys[index], next);
            if (face_apply_) {
                face_apply_(pending_face_mode_, "");
            }
            ShowFacePage();
            break;
        }
        case Action::WifiStaticIp:
            ShowStaticIpPage();
            break;
        case Action::WifiStaticSave: {
            const char* keys[4] = {"static_ip", "static_gw", "static_mask", "static_dns"};
            std::string values[4];
            for (int i = 0; i < 4; i++) {
                values[i] = static_ta_[i] != nullptr ? lv_textarea_get_text(static_ta_[i]) : "";
            }
            if (values[0].empty() || values[1].empty() || values[2].empty()) {
                return;  // ip/gw/mask are required
            }
            {
                Settings settings("wifi", true);
                settings.SetInt("static_en", 1);
                for (int i = 0; i < 4; i++) {
                    settings.SetString(keys[i], values[i]);
                }
            }
            esp_restart();
            break;
        }
        case Action::WifiStaticUseDhcp: {
            {
                Settings settings("wifi", true);
                settings.SetInt("static_en", 0);
            }
            esp_restart();
            break;
        }
        case Action::VolumeMute: {
            auto* codec = Board::GetInstance().GetAudioCodec();
            if (codec == nullptr) {
                return;
            }
            int current = codec->output_volume();
            if (current > 0) {
                volume_restore_ = current;
                codec->SetOutputVolume(0);
            } else {
                codec->SetOutputVolume(volume_restore_ > 0 ? volume_restore_ : 60);
            }
            ShowVolumeSettings();
            return;
        }
    }
}
