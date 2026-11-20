#ifndef PETOI_EXPRESSION_DISPLAY_H_
#define PETOI_EXPRESSION_DISPLAY_H_

#include <cstring>

#include "application.h"
#include "display/lcd_display.h"
#include "face_expression.h"
#include "lvgl_font.h"

// Product-only display. Dialogue text is never retained or rendered. Network,
// provisioning and error messages remain available on a small system screen.
class PetoiExpressionDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

    ~PetoiExpressionDisplay() override {
        DisplayLockGuard lock(this);
        if (poll_timer_) {
            lv_timer_delete(poll_timer_);
        }
        face_.reset();
        if (status_label_) {
            lv_obj_delete(status_label_);
            status_label_ = nullptr;
        }
        if (chat_message_label_) {
            lv_obj_delete(chat_message_label_);
            chat_message_label_ = nullptr;
        }
    }

    void SetupUI() override {
        DisplayLockGuard lock(this);
        if (!lock || IsSetupUICalled()) {
            return;
        }
        Display::SetupUI();
        auto screen = lv_display_get_screen_active(display_);
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
        lv_obj_set_style_text_color(screen, lv_color_white(), 0);
        if (current_theme_ && current_theme_->GetTextFont()) {
            lv_obj_set_style_text_font(screen, current_theme_->GetTextFont()->font(), 0);
        }
        status_label_ = lv_label_create(screen);
        lv_obj_set_width(status_label_, width_ - 16);
        lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 12);
        chat_message_label_ = lv_label_create(screen);
        lv_obj_set_width(chat_message_label_, width_ - 16);
        lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(chat_message_label_, LV_ALIGN_CENTER, 0, 12);
        lv_label_set_text(status_label_, "");
        lv_label_set_text(chat_message_label_, "");
        face_ = std::make_unique<FaceExpression>(screen);
        face_->Show(FaceExpression::MakeDefaultConfig(width_, height_));
        poll_timer_ = lv_timer_create(
            [](lv_timer_t* timer) {
                static_cast<PetoiExpressionDisplay*>(lv_timer_get_user_data(timer))->Tick();
            },
            50, this);
        Tick();
    }

    void SetStatus(const char* status) override {
        DisplayLockGuard lock(this);
        if (lock && status_label_) {
            lv_label_set_text(status_label_, status ? status : "");
        }
    }

    void SetChatMessage(const char* role, const char* content) override {
        if (!role || std::strcmp(role, "system") != 0) {
            return;
        }
        DisplayLockGuard lock(this);
        if (!lock || !chat_message_label_) {
            return;
        }
        system_message_ = content && *content;
        notification_deadline_us_ = 0;
        lv_label_set_text(chat_message_label_, content ? content : "");
        Tick();
    }

    void ClearChatMessages() override { SetChatMessage("system", ""); }
    void RemoveChatMessage(const char* role, const char* content) override {
        DisplayLockGuard lock(this);
        if (lock && role && content && std::strcmp(role, "system") == 0 && chat_message_label_ &&
            std::strcmp(lv_label_get_text(chat_message_label_), content) == 0) {
            system_message_ = false;
            lv_label_set_text(chat_message_label_, "");
        }
    }
    void ShowNotification(const char* text, int duration_ms = 3000) override {
        SetChatMessage("system", text);
        DisplayLockGuard lock(this);
        if (lock) {
            notification_deadline_us_ =
                esp_timer_get_time() + static_cast<int64_t>(std::max(0, duration_ms)) * 1000;
        }
    }
    void ShowNotification(const std::string& text, int duration_ms = 3000) override {
        ShowNotification(text.c_str(), duration_ms);
    }
    void UpdateStatusBar(bool = false) override {}
    bool SetEmotion(const char*) override { return false; }
    bool AddTextGlyphs(const std::vector<TextGlyph>&, uint8_t) override { return false; }
    bool SupportsGuiOperations() const override { return false; }
    void SetPreviewImage(std::unique_ptr<LvglImage>) override {}
    void SetTheme(Theme* theme) override {
        DisplayLockGuard lock(this);
        if (lock && theme) {
            current_theme_ = theme;
            if (IsSetupUICalled() && theme->GetTextFont()) {
                lv_obj_set_style_text_font(lv_display_get_screen_active(display_),
                                           theme->GetTextFont()->font(), 0);
            }
        }
    }

    void Look(const std::string& direction) {
        DisplayLockGuard lock(this);
        if (lock && face_)
            face_->Look(direction);
    }
    void Blink(int times) {
        DisplayLockGuard lock(this);
        if (lock && face_)
            face_->Blink(times);
    }
    void SetMouthWidth(int width) {
        DisplayLockGuard lock(this);
        if (lock && face_) {
            manual_mouth_width_ = std::clamp(width, 0, 240);
            temporary_mouth_width_ = 0;
            face_->SetManualMouthWidth(manual_mouth_width_);
        }
    }
    void SetTemporaryMouthWidth(int width) {
        DisplayLockGuard lock(this);
        if (lock && face_) {
            quiet_after_speech_ = false;
            temporary_mouth_width_ = std::clamp(width, 2, 240);
            mouth_restore_us_ = esp_timer_get_time() + 2000000;
            face_->SetManualMouthWidth(temporary_mouth_width_);
        }
    }
    void SetQuietAfterSpeech(bool quiet) {
        DisplayLockGuard lock(this);
        if (lock) {
            quiet_after_speech_ = quiet;
            temporary_mouth_width_ = 0;
            manual_mouth_width_ = 0;
            if (!quiet && face_)
                face_->SetManualMouthWidth(0);
        }
    }

private:
    std::unique_ptr<FaceExpression> face_;
    lv_timer_t* poll_timer_ = nullptr;
    DeviceState last_state_ = kDeviceStateUnknown;
    bool system_message_ = false;
    bool quiet_after_speech_ = false;
    int64_t notification_deadline_us_ = 0;
    int manual_mouth_width_ = 0;
    int temporary_mouth_width_ = 0;
    int64_t mouth_restore_us_ = 0;

    // LVGL timer context already owns the display lock. Only read application
    // atomics here; never schedule application state changes from the renderer.
    void Tick() {
        if (!face_)
            return;
        auto& app = Application::GetInstance();
        const auto state = app.GetDeviceState();
        if (notification_deadline_us_ && esp_timer_get_time() >= notification_deadline_us_) {
            notification_deadline_us_ = 0;
            system_message_ = false;
            lv_label_set_text(chat_message_label_, "");
        }
        if (state != last_state_) {
            if (last_state_ == kDeviceStateSpeaking && temporary_mouth_width_ > 0) {
                mouth_restore_us_ = esp_timer_get_time() + 2000000;
            }
            if (last_state_ == kDeviceStateSpeaking && quiet_after_speech_) {
                face_->SetManualMouthWidth(5);
            }
            if (state == kDeviceStateSpeaking && quiet_after_speech_) {
                face_->SetManualMouthWidth(0);
            }
            face_->SetState(state == kDeviceStateListening, state == kDeviceStateSpeaking);
            last_state_ = state;
        }
        face_->SetVoiceActive(app.IsVoiceDetected());
        if (temporary_mouth_width_ > 0) {
            if (state == kDeviceStateSpeaking)
                mouth_restore_us_ = 0;
            else if (mouth_restore_us_ && esp_timer_get_time() >= mouth_restore_us_)
                temporary_mouth_width_ = 0;
        }
        const int mouth_width = temporary_mouth_width_ > 0 ? temporary_mouth_width_
                                : manual_mouth_width_ > 0  ? manual_mouth_width_
                                : quiet_after_speech_ && state != kDeviceStateSpeaking ? 5
                                                                                       : 0;
        face_->SetManualMouthWidth(mouth_width);
        const auto level = state == kDeviceStateSpeaking && mouth_width == 0
                               ? app.GetAudioService().GetPlaybackLevel()
                               : 0;
        face_->SetSpeechMotion(level > 0.0f, level);
        const bool operational = state == kDeviceStateIdle || state == kDeviceStateConnecting ||
                                 state == kDeviceStateListening || state == kDeviceStateSpeaking;
        const bool show_face = operational && !system_message_;
        face_->SetVisible(show_face);
        if (show_face) {
            lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
};

#endif  // PETOI_EXPRESSION_DISPLAY_H_
