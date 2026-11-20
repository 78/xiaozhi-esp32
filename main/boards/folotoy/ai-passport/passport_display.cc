#include "passport_display.h"

#if CONFIG_USE_READING_MESSAGE_STYLE
#include "application.h"
#include "assets/lang_config.h"
#include "lvgl_theme.h"
#include "system_info.h"

#include <esp_log.h>
#include <material_symbols.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#define TAG "PassportDisplay"

// Where the keys meet the screen edge, in screen pixels from the top, measured on the
// FoloToy product drawings: the power key (left) and UP face the top of the screen,
// DOWN its middle and OK its lower part.
static constexpr int kPowerKeyCenterY = 47;
static constexpr int kUpKeyCenterY = 42;
static constexpr int kDownKeyCenterY = 157;
static constexpr int kOkKeyCenterY = 274;
// Gap between a label and the screen edge or the status row.
static constexpr int kKeyHintMargin = 2;
static constexpr uint32_t kKeyHintShowMs = 3000;
static constexpr uint32_t kKeyHintGreyMs = 1000;
// Flags on the label objects: the arrow, a label on the left edge, a two-line label.
static constexpr lv_obj_flag_t kKeyHintArrowFlag = LV_OBJ_FLAG_USER_1;
static constexpr lv_obj_flag_t kKeyHintLeftEdgeFlag = LV_OBJ_FLAG_USER_2;
static constexpr lv_obj_flag_t kKeyHintTwoLinesFlag = LV_OBJ_FLAG_USER_3;

PassportDisplay::~PassportDisplay() {
    if (key_hint_timer_ != nullptr) {
        lv_timer_delete(key_hint_timer_);
    }
    if (key_hints_ != nullptr) {
        lv_obj_delete(key_hints_);
    }
}

void PassportDisplay::SetupUI() {
    SpiLcdDisplay::SetupUI();
    ShowKeyHints();
}

void PassportDisplay::ShowKeyHints() {
    DisplayLockGuard lock(this);
    if (top_bar_ == nullptr) {
        return;
    }
    if (key_hints_ != nullptr) {
        HideKeyHints("shown again");
    }

    key_hints_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(key_hints_);
    lv_obj_set_size(key_hints_, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(key_hints_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(key_hints_, LV_OBJ_FLAG_SCROLLABLE);
    key_hints_grey_ = false;

    CreateKeyHint(true, kPowerKeyCenterY, Lang::Strings::KEY_HINT_POWER);
    CreateKeyHint(false, kUpKeyCenterY,
                  std::string(Lang::Strings::KEY_HINT_HOLD_VOLUME_UP) + "\n" +
                      Lang::Strings::KEY_HINT_TAP_PAGE_UP);
    CreateKeyHint(false, kDownKeyCenterY,
                  std::string(Lang::Strings::KEY_HINT_HOLD_VOLUME_DOWN) + "\n" +
                      Lang::Strings::KEY_HINT_TAP_PAGE_DOWN);
    // Until the device is online, a tap on OK enters Wi-Fi setup (see ToggleChat() in the
    // board), so the label says so and changes once start-up is over.
    key_hint_ok_setup_ = Application::GetInstance().GetDeviceState() == kDeviceStateStarting;
    lv_obj_t* ok = CreateKeyHint(false, kOkKeyCenterY,
                                 key_hint_ok_setup_ ? Lang::Strings::KEY_HINT_TAP_WIFI_SETUP
                                                    : Lang::Strings::KEY_HINT_TAP_CHAT);
    const uint32_t count = lv_obj_get_child_count(ok);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* child = lv_obj_get_child(ok, i);
        if (!lv_obj_has_flag(child, kKeyHintArrowFlag)) {
            key_hint_ok_label_ = child;
        }
    }
    PlaceKeyHints();

    key_hint_timer_ = lv_timer_create(OnKeyHintTimer, kKeyHintShowMs, this);
    ESP_LOGI(TAG, "Key hints shown (OK: %s)", key_hint_ok_setup_ ? "Wi-Fi setup" : "conversation");
}

// One label on a screen edge, text only: an arrow pointing at the key, then one or two
// lines.
lv_obj_t* PassportDisplay::CreateKeyHint(bool left_edge, int key_center_y,
                                         const std::string& text) {
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    lv_obj_t* hint = lv_obj_create(key_hints_);
    lv_obj_remove_style_all(hint);
    lv_obj_set_size(hint, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_remove_flag(hint, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(hint, LV_OBJ_FLAG_SCROLLABLE);
    if (left_edge) {
        lv_obj_add_flag(hint, kKeyHintLeftEdgeFlag);
    }
    if (text.find('\n') != std::string::npos) {
        lv_obj_add_flag(hint, kKeyHintTwoLinesFlag);
    }
    lv_obj_set_user_data(hint, reinterpret_cast<void*>(static_cast<intptr_t>(key_center_y)));
    lv_obj_set_style_pad_hor(hint, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_ver(hint, lvgl_theme->spacing(1), 0);
    lv_obj_set_style_pad_column(hint, lvgl_theme->spacing(1), 0);
    lv_obj_set_flex_flow(hint, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hint, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* label = lv_label_create(hint);
    lv_obj_set_style_text_align(label, left_edge ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(label, text.c_str());

    lv_obj_t* arrow = lv_label_create(hint);
    lv_obj_add_flag(arrow, kKeyHintArrowFlag);
    lv_label_set_text(arrow, left_edge ? MATERIAL_SYMBOLS_KEYBOARD_ARROW_LEFT
                                       : MATERIAL_SYMBOLS_KEYBOARD_ARROW_RIGHT);
    if (left_edge) {
        lv_obj_move_to_index(arrow, 0);
    }
    StyleKeyHint(hint);
    return hint;
}

// Everything that comes from the theme. Styles keep raw font pointers and a font change
// frees the old font right after SetTheme(), so SetTheme() runs this again.
void PassportDisplay::StyleKeyHint(lv_obj_t* hint) {
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    const lv_color_t text = lvgl_theme->text_color();
    const lv_color_t background = lvgl_theme->background_color();
    // Grey is the colour of the faded earlier turns in the transcript.
    lv_obj_set_style_text_color(hint, key_hints_grey_ ? lv_color_mix(text, background, 150) : text,
                                0);
    const uint32_t count = lv_obj_get_child_count(hint);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* child = lv_obj_get_child(hint, i);
        const bool arrow = lv_obj_has_flag(child, kKeyHintArrowFlag);
        lv_obj_set_style_text_font(
            child, arrow ? lvgl_theme->icon_font()->font() : lvgl_theme->text_font()->font(), 0);
    }
}

// Centres each label on its key, but keeps the two top ones under the status row, which
// shows the start-up progress meanwhile.
void PassportDisplay::PlaceKeyHints() {
    lv_obj_update_layout(top_bar_);
    lv_area_t status_row;
    lv_obj_get_coords(top_bar_, &status_row);
    const int min_top = status_row.y2 + 1 + kKeyHintMargin;
    lv_obj_update_layout(key_hints_);
    const uint32_t count = lv_obj_get_child_count(key_hints_);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* hint = lv_obj_get_child(key_hints_, i);
        const bool left_edge = lv_obj_has_flag(hint, kKeyHintLeftEdgeFlag);
        const int key_center_y =
            static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(hint)));
        const int height = lv_obj_get_height(hint);
        const int centred_top = key_center_y - height / 2;
        const int top = std::max(centred_top, min_top);
        // A two-line label pushed down by the status row keeps its arrow on the first
        // line, which is still next to the key.
        const bool first_line = top > centred_top && lv_obj_has_flag(hint, kKeyHintTwoLinesFlag);
        lv_obj_set_style_flex_cross_place(
            hint, first_line ? LV_FLEX_ALIGN_START : LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_align(hint, left_edge ? LV_ALIGN_TOP_LEFT : LV_ALIGN_TOP_RIGHT,
                     left_edge ? kKeyHintMargin : -kKeyHintMargin, top);
    }
}

// Runs on the LVGL task, which already holds the display lock.
void PassportDisplay::OnKeyHintTimer(lv_timer_t* timer) {
    auto self = static_cast<PassportDisplay*>(lv_timer_get_user_data(timer));
    if (!self->key_hints_grey_) {
        self->GreyKeyHints();
        lv_timer_set_period(timer, kKeyHintGreyMs);
    } else {
        self->HideKeyHints("timeout");
    }
}

void PassportDisplay::GreyKeyHints() {
    if (key_hints_ == nullptr) {
        return;
    }
    key_hints_grey_ = true;
    const uint32_t count = lv_obj_get_child_count(key_hints_);
    for (uint32_t i = 0; i < count; ++i) {
        StyleKeyHint(lv_obj_get_child(key_hints_, i));
    }
    ESP_LOGI(TAG, "Key hints greyed");
}

// Callers hold the display lock.
void PassportDisplay::HideKeyHints(const char* reason) {
    if (key_hint_timer_ != nullptr) {
        lv_timer_delete(key_hint_timer_);
        key_hint_timer_ = nullptr;
    }
    if (key_hints_ != nullptr) {
        lv_obj_delete(key_hints_);
        key_hints_ = nullptr;
        key_hint_ok_label_ = nullptr;
        ESP_LOGI(TAG, "Key hints hidden (%s)", reason);
    }
}

bool PassportDisplay::KeyHintsVisible() {
    DisplayLockGuard lock(this);
    return key_hints_ != nullptr;
}

void PassportDisplay::SetTheme(Theme* theme) {
    SpiLcdDisplay::SetTheme(theme);
    // Loading the assets at start-up swaps the text font while the labels are up.
    DisplayLockGuard lock(this);
    if (key_hints_ == nullptr) {
        return;
    }
    const uint32_t count = lv_obj_get_child_count(key_hints_);
    for (uint32_t i = 0; i < count; ++i) {
        StyleKeyHint(lv_obj_get_child(key_hints_, i));
    }
    PlaceKeyHints();
    UpdateOkHint();
}

// Start-up is over once the device is online: the assets load (SetTheme()) and the
// version check sets the status right after it.
void PassportDisplay::UpdateOkHint() {
    if (key_hint_ok_label_ == nullptr || !key_hint_ok_setup_) {
        return;
    }
    const auto state = Application::GetInstance().GetDeviceState();
    if (state == kDeviceStateStarting || state == kDeviceStateWifiConfiguring) {
        return;  // still a tap for Wi-Fi setup; the setup text removes the labels anyway
    }
    key_hint_ok_setup_ = false;
    lv_label_set_text(key_hint_ok_label_, Lang::Strings::KEY_HINT_TAP_CHAT);
    PlaceKeyHints();
    ESP_LOGI(TAG, "Key hint for OK: conversation");
}

void PassportDisplay::SetStatus(const char* status) {
    SpiLcdDisplay::SetStatus(status);
    // The application sets the status on every state change. Listening and speaking
    // put text in the transcript (the listening line, the question, the answer), so
    // the labels give way; the status text itself does not count.
    const auto state = Application::GetInstance().GetDeviceState();
    DisplayLockGuard lock(this);
    if (state == kDeviceStateListening || state == kDeviceStateSpeaking) {
        HideKeyHints("conversation");
    } else {
        UpdateOkHint();
    }
}

void PassportDisplay::SetChatMessage(const char* role, const char* content) {
    if (role != nullptr && content != nullptr && content[0] != '\0') {
        DisplayLockGuard lock(this);
        if (key_hints_ != nullptr) {
            // The firmware version line printed at start-up is not news; the labels
            // stand in for it while they are up.
            if (strcmp(role, "system") == 0 && SystemInfo::GetUserAgent() == content) {
                return;
            }
            HideKeyHints("text");
        }
    }
    SpiLcdDisplay::SetChatMessage(role, content);
}
#endif  // CONFIG_USE_READING_MESSAGE_STYLE
