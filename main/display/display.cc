#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <font_awesome.h>

#include "display.h"
#include "board.h"
#include "application.h"
#include "audio_codec.h"
#include "settings.h"
#include "assets/lang_config.h"

#define TAG "Display"

#ifdef HAVE_LVGL
#include "lvgl_theme.h"
#endif

Display::Display() {
}

Display::~Display() {
}

void Display::SetStatus(const char* status) {
    ESP_LOGW(TAG, "SetStatus: %s", status);
}

void Display::ShowNotification(const std::string &notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void Display::ShowNotification(const char* notification, int duration_ms) {
    ESP_LOGW(TAG, "ShowNotification: %s", notification);
}

void Display::UpdateStatusBar(bool update_all) {
}


void Display::SetEmotion(const char* emotion) {
    ESP_LOGW(TAG, "SetEmotion: %s", emotion);
}

void Display::SetChatMessage(const char* role, const char* content) {
    ESP_LOGW(TAG, "Role:%s", role);
    ESP_LOGW(TAG, "     %s", content);
}

void Display::SetTheme(Theme* theme) {
    current_theme_ = theme;
    Settings settings("display", true);
    settings.SetString("theme", theme->name());
}

void Display::SetPowerSaveMode(bool on) {
    ESP_LOGW(TAG, "SetPowerSaveMode: %d", on);
}

void Display::ShowQRCode(const char* url, const char* hint) {
#ifdef HAVE_LVGL
    DisplayLockGuard lock(this);

    LvglTheme* lvgl_theme = nullptr;
    if (current_theme_ != nullptr) {
        lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    }
    const lv_font_t* text_font = nullptr;
    if (lvgl_theme != nullptr && lvgl_theme->text_font() != nullptr) {
        text_font = lvgl_theme->text_font()->font();
    }

    // Delete old QR code container if exists
    if (qrcode_container_ != nullptr) {
        lv_obj_del(qrcode_container_);
        qrcode_container_ = nullptr;
        qrcode_ = nullptr;
        qrcode_hint_label_ = nullptr;
    }

    // Create full screen container
    qrcode_container_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(qrcode_container_, width_, height_);
    lv_obj_center(qrcode_container_);
    lv_obj_clear_flag(qrcode_container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(qrcode_container_, 0, 0);
    lv_obj_set_style_pad_all(qrcode_container_, 20, 0);
    if (text_font != nullptr) {
        lv_obj_set_style_text_font(qrcode_container_, text_font, 0);
    }

    // Set flex layout (vertical, center)
    lv_obj_set_flex_flow(qrcode_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(qrcode_container_,
                         LV_FLEX_ALIGN_CENTER,
                         LV_FLEX_ALIGN_CENTER,
                         LV_FLEX_ALIGN_CENTER);

#if LV_USE_QRCODE
    // Calculate QR code size
    int qr_size = (width_ < height_ ? width_ : height_) * 0.6;
    if (qr_size > 300) qr_size = 300;
    if (qr_size < 100) qr_size = 100;

    // Create QR code object
    qrcode_ = lv_qrcode_create(qrcode_container_);
    if (qrcode_ != nullptr) {
        lv_qrcode_set_size(qrcode_, qr_size);
        lv_qrcode_set_dark_color(qrcode_, lv_color_black());
        lv_qrcode_set_light_color(qrcode_, lv_color_white());

        // Update QR code data
        lv_result_t res = lv_qrcode_update(qrcode_, url, strlen(url));
        if (res == LV_RESULT_OK) {
            lv_obj_center(qrcode_);
            ESP_LOGI(TAG, "QR code created successfully");
        } else {
            ESP_LOGE(TAG, "Failed to update QR code data");
            // Fallback to label
            lv_obj_del(qrcode_);
            qrcode_ = lv_label_create(qrcode_container_);
            lv_label_set_text(qrcode_, "[QR Code Creation Failed]");
        }
    } else {
        ESP_LOGE(TAG, "Failed to create QR code object");
        // Fallback to label
        qrcode_ = lv_label_create(qrcode_container_);
        lv_label_set_text(qrcode_, "[QR Code Creation Failed]");
    }
#else
    // Fallback: display URL as text
    qrcode_ = lv_label_create(qrcode_container_);
    lv_label_set_text(qrcode_, url);
    lv_label_set_long_mode(qrcode_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(qrcode_, width_ - 40);
    ESP_LOGW(TAG, "LV_USE_QRCODE not enabled, displaying URL as text");
#endif

    if (text_font != nullptr && qrcode_ != nullptr) {
        lv_obj_set_style_text_font(qrcode_, text_font, 0);
    }

    // Create hint label
    qrcode_hint_label_ = lv_label_create(qrcode_container_);
    lv_label_set_text(qrcode_hint_label_, hint);
    lv_obj_set_style_text_align(qrcode_hint_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(qrcode_hint_label_, 20, 0);
    lv_obj_set_width(qrcode_hint_label_, width_ - 40);
    lv_label_set_long_mode(qrcode_hint_label_, LV_LABEL_LONG_WRAP);
    if (text_font != nullptr) {
        lv_obj_set_style_text_font(qrcode_hint_label_, text_font, 0);
    }

    ESP_LOGI(TAG, "QR Code displayed: %s", url);
#else
    ESP_LOGW(TAG, "ShowQRCode called but HAVE_LVGL not defined");
#endif
}

void Display::HideQRCode() {
#ifdef HAVE_LVGL
    DisplayLockGuard lock(this);

    if (qrcode_container_ != nullptr) {
        lv_obj_del(qrcode_container_);
        qrcode_container_ = nullptr;
        qrcode_ = nullptr;
        qrcode_hint_label_ = nullptr;
        ESP_LOGI(TAG, "QR code hidden");
    }
#else
    ESP_LOGW(TAG, "HideQRCode called but HAVE_LVGL not defined");
#endif
}
