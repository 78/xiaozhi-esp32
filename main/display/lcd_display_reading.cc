// Reading message style for LcdDisplay (CONFIG_USE_READING_MESSAGE_STYLE).
//
// The emotion sits in a status row at the top; the rest of the screen is a scrollable
// transcript. Each turn (a question and its answer) is one box. Answer sentences are
// appended to the box instead of replacing each other, so the whole answer can be
// read while it is spoken. The newest box is at least one screen tall, which makes
// "scroll to the end" open a new question on a fresh page and then follow the answer
// as it grows. Lines that trail the conversation (the transient system line) are kept
// inside the newest box so they never push its page up. ScrollChat() lets a board page
// back through the transcript; following resumes once the view is back at the end.
//
// OnDeviceStateChanged() follows the conversation: a "listening" line while the device
// waits for speech, a waiting line and a seconds counter between the question and the
// first sentence, a status row tinted by state, and at the end of a conversation the
// last turn stays on screen, faded, until the next conversation starts.
#include "lcd_display.h"

#if CONFIG_USE_READING_MESSAGE_STYLE

#include "assets/lang_config.h"
#include "lvgl_theme.h"

#include <esp_log.h>
#include <material_symbols.h>

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <string>

#define TAG "LcdDisplay"

namespace {

// Status row accents. They are mixed with the theme colors so they work on light and
// dark backgrounds alike.
constexpr uint32_t kListeningAccent = 0x1F5FD1;
constexpr uint32_t kThinkingAccent = 0xC98A00;
constexpr uint32_t kSpeakingAccent = 0x1E8E4E;

lv_color_t AccentText(LvglTheme* theme, uint32_t accent) {
    return lv_color_mix(lv_color_hex(accent), theme->text_color(), 200);
}

// Question text and dividers are derived from the theme so both light and dark work.
lv_color_t QuestionColor(LvglTheme* theme) {
    return lv_color_mix(theme->text_color(), theme->background_color(), 150);
}

lv_color_t DividerColor(LvglTheme* theme) {
    return lv_color_mix(theme->text_color(), theme->background_color(), 40);
}

lv_obj_t* CreatePlainBox(lv_obj_t* parent) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_set_style_radius(box, 0, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

lv_obj_t* CreateWrappedLabel(lv_obj_t* parent) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, "");
    return label;
}

}  // namespace

void LcdDisplay::SetupUI() {
    // Prevent duplicate calls - if already called, return early
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }

    Display::SetupUI();  // Mark SetupUI as called
    DisplayLockGuard lock(this);
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lvgl_theme->text_color(), 0);
    lv_obj_set_style_bg_color(screen, lvgl_theme->background_color(), 0);

    /* Container: status row on top, transcript below */
    container_ = CreatePlainBox(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(container_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_border_color(container_, lvgl_theme->border_color(), 0);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status row: emotion, status or notification text, then the status icons.
     * status_bar_ points at the same object so boards that restyle it after SetupUI()
     * still find a valid bar. */
    top_bar_ = CreatePlainBox(container_);
    status_bar_ = top_bar_;
    lv_obj_set_size(top_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(top_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_bottom(top_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_left(top_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_pad_right(top_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_pad_column(top_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_border_side(top_bar_, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(top_bar_, 1, 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    emoji_box_ = CreatePlainBox(top_bar_);
    lv_obj_set_size(emoji_box_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    emoji_label_ = lv_label_create(emoji_box_);
    lv_obj_set_style_text_font(emoji_label_, large_icon_font, 0);
    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(emoji_label_, MATERIAL_SYMBOLS_ROBOT_2);

    emoji_image_ = lv_image_create(emoji_box_);
    lv_obj_center(emoji_image_);
    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(top_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(status_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(top_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_label_set_long_mode(notification_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(notification_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lvgl_theme->text_color(), 0);

    mute_label_ = lv_label_create(top_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, lvgl_theme->text_color(), 0);

    battery_label_ = lv_label_create(top_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, lvgl_theme->text_color(), 0);

    /* Transcript */
    content_ = CreatePlainBox(container_);
    lv_obj_add_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(content_, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(content_, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scroll_dir(content_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_pad_left(content_, lvgl_theme->spacing(5), 0);
    lv_obj_set_style_pad_right(content_, lvgl_theme->spacing(5), 0);
    lv_obj_set_style_pad_top(content_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_bottom(content_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_row(content_, lvgl_theme->spacing(3), 0);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_width(content_, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(content_, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(content_, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(content_, LV_OPA_COVER, LV_PART_SCROLLBAR);

    // Listening / waiting line, then the transient system line (activation hints,
    // errors); both trail the conversation, see AttachTrailingLabels().
    pending_label_ = CreateWrappedLabel(content_);
    lv_obj_add_flag(pending_label_, LV_OBJ_FLAG_HIDDEN);

    system_label_ = CreateWrappedLabel(content_);
    lv_obj_set_style_radius(system_label_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_all(system_label_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_bg_opa(system_label_, LV_OPA_50, 0);
    lv_obj_add_flag(system_label_, LV_OBJ_FLAG_HIDDEN);

    /* Overlays */
    preview_image_ = lv_image_create(screen);
    lv_obj_set_size(preview_image_, width_ / 2, height_ / 2);
    lv_obj_align(preview_image_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);

    // Shown while the reader has paged back and newer text is below the view.
    more_below_label_ = lv_label_create(screen);
    lv_obj_set_style_text_font(more_below_label_, icon_font, 0);
    lv_label_set_text(more_below_label_, MATERIAL_SYMBOLS_KEYBOARD_ARROW_DOWN);
    lv_obj_set_style_radius(more_below_label_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(more_below_label_, lvgl_theme->spacing(1), 0);
    lv_obj_set_style_bg_opa(more_below_label_, LV_OPA_COVER, 0);
    lv_obj_align(more_below_label_, LV_ALIGN_BOTTOM_RIGHT, -lvgl_theme->spacing(4),
                 -lvgl_theme->spacing(2));
    lv_obj_add_flag(more_below_label_, LV_OBJ_FLAG_HIDDEN);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -lvgl_theme->spacing(4));
    lv_obj_set_style_bg_color(low_battery_popup_, lvgl_theme->low_battery_color(), 0);
    lv_obj_set_style_radius(low_battery_popup_, lvgl_theme->spacing(4), 0);

    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    // Follows the spoken answer; resumed when a spoken sentence arrives.
    speech_timer_ = lv_timer_create(
        [](lv_timer_t* timer) {
            static_cast<LcdDisplay*>(lv_timer_get_user_data(timer))->FollowSpeech();
        },
        100, this);
    lv_timer_pause(speech_timer_);
#if CONFIG_READING_MESSAGE_STYLE_FOLLOW_WORDS
    speech_.set_word_level(true);
#else
    speech_.set_word_level(false);
#endif

    ApplyReadingColors(lvgl_theme);
}

void LcdDisplay::SetChatMessage(const char* role, const char* content) {
    if (!setup_ui_called_) {
        ESP_LOGW(TAG, "SetChatMessage('%s', '%s') called before SetupUI() - message will be lost!",
                 role, content);
    }
    DisplayLockGuard lock(this);
    if (content_ == nullptr || role == nullptr || content == nullptr) {
        return;
    }

    if (strcmp(role, "system") == 0) {
        // System messages are transient, as in the other styles: an empty one clears it.
        if (content[0] == '\0') {
            lv_obj_add_flag(system_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(system_label_, content);
            lv_obj_remove_flag(system_label_, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (strcmp(role, "user") == 0) {
        if (conversation_ended_ && content[0] != '\0') {
            ClearTranscript();  // the faded turn of a finished conversation gives way
        }
        ApplyTranscriptUpdate(transcript_.AddUser(content));
        if (content[0] != '\0' && device_state_ == kDeviceStateListening) {
            StartThinking();  // the question is in; the answer has not started yet
        }
    } else {
        AppendAssistant(content, 0);
    }

    if (follow_latest_) {
        ScrollToLatest(true);
    }
    UpdateMoreBelow();
}

void LcdDisplay::AddSpokenSentence(uint32_t speech_id, const char* text) {
    DisplayLockGuard lock(this);
    if (content_ == nullptr || text == nullptr) {
        return;
    }
    AppendAssistant(text, speech_id);
    if (follow_latest_) {
        ScrollToLatest(true);
    }
    UpdateMoreBelow();
}

void LcdDisplay::SetSpeechProgress(uint32_t speech_id, uint32_t position_ms) {
    speech_progress_.store((static_cast<uint64_t>(speech_id) << 32) | position_ms);
}

void LcdDisplay::OnSpeechInterrupted() { speech_interrupted_.store(true); }

void LcdDisplay::AppendAssistant(const char* text, uint32_t speech_id) {
    if (conversation_ended_ && text[0] != '\0') {
        ClearTranscript();
    }
    // An empty assistant message clears the subtitle in the other styles; here the
    // answer stays readable until the transcript is cleared, so it is ignored.
    auto update = transcript_.AddAssistant(text, speech_id);
    ApplyTranscriptUpdate(update);
    if (text[0] != '\0' && pending_ == ReadingPending::kThinking) {
        StopThinking();
        // The counter wrote over the status text; put back the one for the state.
        if (status_label_ != nullptr && device_state_ == kDeviceStateSpeaking) {
            lv_label_set_text(status_label_, Lang::Strings::SPEAKING);
        } else if (status_label_ != nullptr && device_state_ == kDeviceStateListening) {
            lv_label_set_text(status_label_, Lang::Strings::LISTENING);
        }
    }
    if (speech_id != 0 && update.turn_id != 0 && speech_timer_ != nullptr) {
        // Start following; the grace period covers the audio that is still buffering.
        last_progress_us_ = esp_timer_get_time();
        lv_timer_resume(speech_timer_);
    }
}

void LcdDisplay::RemoveChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (content_ == nullptr || role == nullptr || content == nullptr) {
        return;
    }
    if (strcmp(role, "system") == 0) {
        if (!lv_obj_has_flag(system_label_, LV_OBJ_FLAG_HIDDEN) &&
            strcmp(lv_label_get_text(system_label_), content) == 0) {
            lv_obj_add_flag(system_label_, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (strcmp(role, "user") != 0) {
        ApplyTranscriptUpdate(transcript_.RemoveAssistant(content));
    }
    if (follow_latest_) {
        ScrollToLatest(false);
    }
    UpdateMoreBelow();
}

// The application calls this when the device returns to idle. Unlike the other styles it
// does not wipe the screen: the last turn stays readable, faded, and is cleared when the
// next conversation starts (see OnDeviceStateChanged()).
void LcdDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (system_label_ != nullptr) {
        lv_obj_add_flag(system_label_, LV_OBJ_FLAG_HIDDEN);
    }
    EndConversation();
    UpdateMoreBelow();
}

void LcdDisplay::ClearTranscript() {
    transcript_.Clear();
    ParkTrailingLabels();
    for (auto& view : turn_views_) {
        lv_obj_delete(view.box);
    }
    turn_views_.clear();
    follow_latest_ = true;
    conversation_ended_ = false;
    speech_.Reset();
    if (content_ != nullptr) {
        lv_obj_scroll_to_y(content_, 0, LV_ANIM_OFF);
    }
}

void LcdDisplay::OnDeviceStateChanged(DeviceState state) {
    DisplayLockGuard lock(this);
    if (content_ == nullptr) {
        return;
    }
    device_state_ = state;
    switch (state) {
        case kDeviceStateConnecting:
            // A new conversation starts on an empty screen.
            ClearTranscript();
            StopThinking();
            SetPending(ReadingPending::kNone);
            break;
        case kDeviceStateListening:
            // Boards that keep the audio channel open go from idle straight to listening.
            if (conversation_ended_) {
                ClearTranscript();
            }
            StopThinking();
            SetPending(ReadingPending::kListening);
            break;
        case kDeviceStateIdle:
            // Keeps a system line that is still showing, such as a network error.
            EndConversation();
            break;
        case kDeviceStateSpeaking:
            // Servers start speaking (tts start) before the first sentence of the answer
            // arrives, sometimes seconds before; keep the waiting line and its counter
            // until there is text to show.
            if (pending_ == ReadingPending::kThinking) {
                UpdateThinkingStatus();  // the state change has just written "speaking" there
            } else {
                SetPending(ReadingPending::kNone);
            }
            break;
        default:
            StopThinking();
            SetPending(ReadingPending::kNone);
            break;
    }
    ApplyStatusTint(static_cast<LvglTheme*>(current_theme_));
    if (follow_latest_) {
        ScrollToLatest(true);
    }
    UpdateMoreBelow();
}

void LcdDisplay::EndConversation() {
    StopThinking();
    SetPending(ReadingPending::kNone);
    if (turn_views_.empty()) {
        return;
    }
    conversation_ended_ = true;
    ApplyTurnColors(static_cast<LvglTheme*>(current_theme_));
    UpdateSpokenSelection();
}

void LcdDisplay::SetPending(ReadingPending pending) {
    pending_ = pending;
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    StylePending(lvgl_theme);
    ApplyStatusTint(lvgl_theme);
}

void LcdDisplay::StylePending(LvglTheme* lvgl_theme) {
    if (pending_label_ == nullptr) {
        return;
    }
    switch (pending_) {
        case ReadingPending::kListening:
            lv_label_set_text(pending_label_, Lang::Strings::LISTENING);
            lv_obj_set_style_text_color(pending_label_, AccentText(lvgl_theme, kListeningAccent),
                                        0);
            break;
        case ReadingPending::kThinking:
            lv_label_set_text(pending_label_, "...");
            lv_obj_set_style_text_color(pending_label_, QuestionColor(lvgl_theme), 0);
            break;
        case ReadingPending::kNone:
            lv_obj_add_flag(pending_label_, LV_OBJ_FLAG_HIDDEN);
            return;
    }
    lv_obj_remove_flag(pending_label_, LV_OBJ_FLAG_HIDDEN);
}

void LcdDisplay::StartThinking() {
    thinking_since_us_ = esp_timer_get_time();
    SetPending(ReadingPending::kThinking);
    if (thinking_timer_ == nullptr) {
        thinking_timer_ = lv_timer_create(
            [](lv_timer_t* timer) {
                static_cast<LcdDisplay*>(lv_timer_get_user_data(timer))->UpdateThinkingStatus();
            },
            1000, this);
    }
    UpdateThinkingStatus();
}

void LcdDisplay::StopThinking() {
    if (thinking_timer_ != nullptr) {
        lv_timer_delete(thinking_timer_);
        thinking_timer_ = nullptr;
    }
    if (pending_ == ReadingPending::kThinking) {
        SetPending(ReadingPending::kNone);
    }
}

// Counts the seconds spent waiting for the answer in the status row, so a slow reply
// does not look like a hang. Runs from an LVGL timer, which already holds the lock.
void LcdDisplay::UpdateThinkingStatus() {
    if (status_label_ == nullptr || pending_ != ReadingPending::kThinking) {
        return;
    }
    const int64_t seconds = (esp_timer_get_time() - thinking_since_us_) / 1000000;
    std::string text = Lang::Strings::PLEASE_WAIT;
    if (seconds >= 2) {
        text += " " + std::to_string(seconds) + "s";
    }
    lv_label_set_text(status_label_, text.c_str());
}

void LcdDisplay::ApplyStatusTint(LvglTheme* lvgl_theme) {
    if (top_bar_ == nullptr) {
        return;
    }
    uint32_t accent = 0;
    if (pending_ == ReadingPending::kThinking) {
        accent = kThinkingAccent;
    } else if (device_state_ == kDeviceStateListening) {
        accent = kListeningAccent;
    } else if (device_state_ == kDeviceStateSpeaking || device_state_ == kDeviceStateNotifying) {
        accent = kSpeakingAccent;
    }
    // The border keeps one width in every state so the page height never changes.
    lv_obj_set_style_border_width(top_bar_, 2, 0);
    if (accent == 0) {
        lv_obj_set_style_bg_opa(top_bar_, LV_OPA_50, 0);
        lv_obj_set_style_bg_color(top_bar_, lvgl_theme->chat_background_color(), 0);
        lv_obj_set_style_border_color(top_bar_, DividerColor(lvgl_theme), 0);
        return;
    }
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(
        top_bar_, lv_color_mix(lv_color_hex(accent), lvgl_theme->background_color(), 56), 0);
    lv_obj_set_style_border_color(top_bar_, lv_color_hex(accent), 0);
}

bool LcdDisplay::ScrollChat(int direction) {
    DisplayLockGuard lock(this);
    if (content_ == nullptr || direction == 0 || lv_obj_has_flag(content_, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }
    lv_obj_update_layout(content_);
    // Start from where a running scroll animation will end, so quick presses add up.
    lv_point_t end;
    lv_obj_get_scroll_end(content_, &end);
    const int32_t current = end.y;
    const int32_t max_scroll = lv_obj_get_scroll_y(content_) + lv_obj_get_scroll_bottom(content_);
    // Two lines per press keeps some context from the previous view on screen.
    const int32_t step =
        2 * lv_font_get_line_height(lv_obj_get_style_text_font(content_, LV_PART_MAIN));
    auto result = ReadingTranscript::Step(current, max_scroll, direction < 0 ? -step : step);
    if (!result.moved) {
        return false;
    }
    follow_latest_ = result.follow;
    lv_obj_scroll_to_y(content_, result.target, LV_ANIM_ON);
    UpdateMoreBelow();
    return true;
}

void LcdDisplay::ApplyTranscriptUpdate(const ReadingTranscript::Update& update) {
    for (uint32_t id : update.dropped) {
        DeleteTurnView(id);
    }
    if (update.turn_id == 0) {
        return;
    }
    if (update.new_turn) {
        CreateTurnView(update.turn_id);
        // A new question always opens on a fresh page, even if the reader had paged back.
        follow_latest_ = true;
        speech_.Reset();
        speech_interrupted_.store(false);
    }
    if (auto view = FindTurnView(update.turn_id)) {
        SyncTurnView(*view);
    }
}

LcdDisplay::ReadingTurnView* LcdDisplay::FindTurnView(uint32_t id) {
    for (auto& view : turn_views_) {
        if (view.id == id) {
            return &view;
        }
    }
    return nullptr;
}

void LcdDisplay::CreateTurnView(uint32_t id) {
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    ReadingTurnView view;
    view.id = id;
    view.box = CreatePlainBox(content_);
    lv_obj_set_width(view.box, lv_pct(100));
    lv_obj_set_height(view.box, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(view.box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(view.box, lvgl_theme->spacing(2), 0);
    if (!turn_views_.empty()) {
        // A thin rule separates this turn from the previous one.
        lv_obj_set_style_border_side(view.box, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(view.box, 1, 0);
        lv_obj_set_style_pad_top(view.box, lvgl_theme->spacing(3), 0);
    }

    // Colors are set by ApplyTurnColors() below, together with the older turns.
    view.user = CreateWrappedLabel(view.box);
    lv_obj_add_flag(view.user, LV_OBJ_FLAG_HIDDEN);

    view.assistant = CreateWrappedLabel(view.box);
    lv_obj_add_flag(view.assistant, LV_OBJ_FLAG_HIDDEN);

    // Only the newest turn is a full page tall; the older ones shrink to their text and no
    // longer show what was left unspoken.
    if (!turn_views_.empty()) {
        lv_obj_t* previous = turn_views_.back().assistant;
        lv_obj_set_style_min_height(turn_views_.back().box, 0, 0);
        lv_label_set_text_selection_start(previous, LV_LABEL_TEXT_SELECTION_OFF);
        lv_label_set_text_selection_end(previous, LV_LABEL_TEXT_SELECTION_OFF);
    }
    turn_views_.push_back(view);
    AttachTrailingLabels();
    ApplyTurnColors(lvgl_theme);
    ResizeNewestTurn();
}

void LcdDisplay::DeleteTurnView(uint32_t id) {
    auto it = std::find_if(turn_views_.begin(), turn_views_.end(),
                           [id](const ReadingTurnView& view) { return view.id == id; });
    if (it == turn_views_.end()) {
        return;
    }
    const bool was_first = it == turn_views_.begin();
    const bool was_last = std::next(it) == turn_views_.end();
    int32_t removed_height =
        lv_obj_get_height(it->box) + lv_obj_get_style_pad_row(content_, LV_PART_MAIN);
    const int32_t current = lv_obj_get_scroll_y(content_);
    ParkTrailingLabels();  // the newest box holds them; keep them out of the deletion
    lv_obj_delete(it->box);
    turn_views_.erase(it);
    AttachTrailingLabels();
    if (turn_views_.empty()) {
        return;
    }
    ApplyTurnColors(static_cast<LvglTheme*>(current_theme_));

    if (was_first) {
        // The oldest remaining turn no longer needs the rule above it.
        lv_obj_t* first = turn_views_.front().box;
        removed_height += lv_obj_get_style_pad_top(first, LV_PART_MAIN) +
                          lv_obj_get_style_border_width(first, LV_PART_MAIN);
        lv_obj_set_style_border_width(first, 0, 0);
        lv_obj_set_style_pad_top(first, 0, 0);
    }
    if (was_last) {
        ResizeNewestTurn();
    } else if (!follow_latest_) {
        // Old turns sit above the view; keep the text being read in place.
        lv_obj_update_layout(content_);
        lv_obj_scroll_to_y(content_, std::max<int32_t>(0, current - removed_height), LV_ANIM_OFF);
    }
}

void LcdDisplay::SyncTurnView(ReadingTurnView& view) {
    const ReadingTranscript::Turn* turn = transcript_.Find(view.id);
    if (turn == nullptr) {
        return;
    }
    if (turn->user.empty()) {
        lv_obj_add_flag(view.user, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(view.user, turn->user.c_str());
        lv_obj_remove_flag(view.user, LV_OBJ_FLAG_HIDDEN);
    }

    std::string answer = turn->AssistantText();
    if (answer.empty()) {
        lv_obj_add_flag(view.assistant, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (turn->trimmed) {
        answer.insert(0, "...");
    }
    lv_label_set_text(view.assistant, answer.c_str());
    lv_obj_remove_flag(view.assistant, LV_OBJ_FLAG_HIDDEN);
    if (!turn_views_.empty() && &view == &turn_views_.back()) {
        UpdateSpokenSelection();
    }
}

// The part of the newest answer that has not been spoken yet is drawn faded, through the
// label's text selection. Nothing is selected once everything has been spoken, when the
// conversation has ended, or in older turns.
void LcdDisplay::UpdateSpokenSelection() {
    if (turn_views_.empty() || transcript_.turns().empty()) {
        return;
    }
    const ReadingTurnView& view = turn_views_.back();
    const ReadingTranscript::Turn& turn = transcript_.turns().back();
    if (view.id != turn.id) {
        return;
    }
    const std::vector<size_t> ends = turn.SentenceEnds();
    const uint32_t prefix = turn.trimmed ? 3 : 0;  // the "..." marker
    const uint32_t total = prefix + (ends.empty() ? 0 : ends.back());
    const uint32_t lit = prefix + speech_.LitChars(turn);
    if (conversation_ended_ || lit >= total) {
        lv_label_set_text_selection_start(view.assistant, LV_LABEL_TEXT_SELECTION_OFF);
        lv_label_set_text_selection_end(view.assistant, LV_LABEL_TEXT_SELECTION_OFF);
        return;
    }
    lv_label_set_text_selection_start(view.assistant, lit);
    lv_label_set_text_selection_end(view.assistant, total);
}

// Runs every 100 ms while an answer is being spoken, from the LVGL timer (lock held).
// Frames are reported about every 60 ms; when none has played for a while the speech has
// paused between sentences or ended.
void LcdDisplay::FollowSpeech() {
    if (transcript_.turns().empty()) {
        lv_timer_pause(speech_timer_);
        return;
    }
    const ReadingTranscript::Turn& turn = transcript_.turns().back();
    if (speech_interrupted_.exchange(false)) {
        speech_.OnInterrupted();
    }
    bool changed = false;
    const uint64_t progress = speech_progress_.load();
    const int64_t now = esp_timer_get_time();
    if (progress != applied_progress_) {
        applied_progress_ = progress;
        last_progress_us_ = now;
        changed = speech_.OnProgress(static_cast<uint32_t>(progress >> 32),
                                     static_cast<uint32_t>(progress), turn);
    } else if (now - last_progress_us_ > 300 * 1000) {
        const bool speaking = device_state_ == kDeviceStateSpeaking;
        changed = speech_.OnPlaybackPaused(speaking, turn);
        if (!speaking) {
            lv_timer_pause(speech_timer_);  // nothing more is coming for this answer
        }
    }
    if (changed) {
        UpdateSpokenSelection();
    }
}

// The newest turn is exactly one transcript page tall (see the top of this file). The
// page height follows the status row, so it is recomputed whenever fonts can change.
void LcdDisplay::ResizeNewestTurn() {
    if (content_ == nullptr || turn_views_.empty()) {
        return;
    }
    lv_obj_update_layout(content_);
    lv_obj_set_style_min_height(turn_views_.back().box, lv_obj_get_content_height(content_), 0);
}

void LcdDisplay::ApplyReadingColors(LvglTheme* lvgl_theme) {
    ApplyStatusTint(lvgl_theme);
    StylePending(lvgl_theme);
    if (content_ != nullptr) {
        lv_obj_set_style_bg_color(content_, QuestionColor(lvgl_theme), LV_PART_SCROLLBAR);
    }
    if (system_label_ != nullptr) {
        lv_obj_set_style_text_color(system_label_, lvgl_theme->system_text_color(), 0);
        lv_obj_set_style_bg_color(system_label_, lvgl_theme->chat_background_color(), 0);
    }
    if (more_below_label_ != nullptr) {
        lv_obj_set_style_bg_color(more_below_label_, lvgl_theme->text_color(), 0);
        lv_obj_set_style_text_color(more_below_label_, lvgl_theme->background_color(), 0);
    }
    ApplyTurnColors(lvgl_theme);
}

// The answer being read is drawn in the text color; earlier turns are faded, and so is
// the last one once the conversation has ended.
void LcdDisplay::ApplyTurnColors(LvglTheme* lvgl_theme) {
    for (size_t i = 0; i < turn_views_.size(); ++i) {
        const auto& view = turn_views_[i];
        const bool current = i + 1 == turn_views_.size() && !conversation_ended_;
        lv_obj_set_style_border_color(view.box, DividerColor(lvgl_theme), 0);
        lv_obj_set_style_text_color(view.user, QuestionColor(lvgl_theme), 0);
        lv_obj_set_style_text_color(
            view.assistant, current ? lvgl_theme->text_color() : QuestionColor(lvgl_theme), 0);
        // The part not spoken yet (see UpdateSpokenSelection()); the selection background
        // is the screen background so only the text color changes.
        lv_obj_set_style_text_color(view.assistant, QuestionColor(lvgl_theme), LV_PART_SELECTED);
        lv_obj_set_style_bg_color(view.assistant, lvgl_theme->background_color(), LV_PART_SELECTED);
    }
}

// Trailing lines live at the end of the newest box, inside its one-page minimum height,
// so showing one never scrolls the start of the newest turn off the screen.
void LcdDisplay::AttachTrailingLabels() {
    if (content_ == nullptr) {
        return;
    }
    lv_obj_t* parent = turn_views_.empty() ? content_ : turn_views_.back().box;
    for (lv_obj_t* label : {pending_label_, system_label_}) {
        if (label == nullptr) {
            continue;
        }
        if (lv_obj_get_parent(label) != parent) {
            lv_obj_set_parent(label, parent);
        }
        lv_obj_move_to_index(label, -1);
    }
}

// Moves the trailing lines back to the transcript before turn boxes are deleted.
void LcdDisplay::ParkTrailingLabels() {
    if (content_ == nullptr) {
        return;
    }
    for (lv_obj_t* label : {pending_label_, system_label_}) {
        if (label != nullptr && lv_obj_get_parent(label) != content_) {
            lv_obj_set_parent(label, content_);
        }
    }
}

void LcdDisplay::ScrollToLatest(bool animate) {
    if (content_ == nullptr) {
        return;
    }
    lv_obj_update_layout(content_);
    const int32_t below = lv_obj_get_scroll_bottom(content_);
    if (below == 0) {
        return;
    }
    const int32_t target = std::max<int32_t>(0, lv_obj_get_scroll_y(content_) + below);
    lv_obj_scroll_to_y(content_, target, animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

void LcdDisplay::UpdateMoreBelow() {
    if (more_below_label_ == nullptr || content_ == nullptr) {
        return;
    }
    // Not following means the reader paged back, so newer text is below the view.
    const bool show = !follow_latest_ && !lv_obj_has_flag(content_, LV_OBJ_FLAG_HIDDEN);
    if (show) {
        lv_obj_remove_flag(more_below_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(more_below_label_, LV_OBJ_FLAG_HIDDEN);
    }
}

#endif  // CONFIG_USE_READING_MESSAGE_STYLE
