#include "zhengchen_minicam_lcd_display.h"

#include "config.h"
#include "display/display.h"
#include "lvgl_theme.h"
#include "settings.h"

#include <string>
#include <algorithm>

namespace {
struct SettingsMenuMetrics {
    lv_coord_t menu_margin;
    lv_coord_t menu_pad_x;
    lv_coord_t menu_pad_y;
    lv_coord_t row_pad_x;
    lv_coord_t row_pad_y;
    lv_coord_t title_pad_bottom;
    lv_coord_t line_space;
    const char* title_text;
};

SettingsMenuMetrics GetSettingsMenuMetrics(bool is_landscape) {
    if (is_landscape) {
        return {
            .menu_margin = 8,
            .menu_pad_x = 8,
            .menu_pad_y = 4,
            .row_pad_x = 6,
            .row_pad_y = 2,
            .title_pad_bottom = 4,
            .line_space = 0,
            .title_text = "设置Settings",
        };
    }

    return {
        .menu_margin = 16,
        .menu_pad_x = 10,
        .menu_pad_y = 8,
        .row_pad_x = 8,
        .row_pad_y = 5,
        .title_pad_bottom = 8,
        .line_space = 2,
        .title_text = "设置\nSettings",
    };
}
}

#if !CONFIG_USE_WECHAT_MESSAGE_STYLE
namespace {
void ApplyFullscreenPreviewTransform(lv_obj_t* preview_image, const lv_image_dsc_t* img_dsc, bool is_landscape) {
    lv_image_set_rotation(preview_image, is_landscape ? 900 : 0);
    if (img_dsc->header.w <= 0 || img_dsc->header.h <= 0) {
        return;
    }

    const int32_t max_width = LV_HOR_RES * 9 / 10;
    const int32_t max_height = LV_VER_RES * 9 / 10;
    const int32_t display_width = is_landscape ? img_dsc->header.h : img_dsc->header.w;
    const int32_t display_height = is_landscape ? img_dsc->header.w : img_dsc->header.h;
    const uint32_t scale_x = (static_cast<uint32_t>(max_width) * 256) / display_width;
    const uint32_t scale_y = (static_cast<uint32_t>(max_height) * 256) / display_height;
    lv_image_set_scale(preview_image, std::min(scale_x, scale_y));
}
}
#endif

void ZhengchenMinicamLcdDisplay::SetupUI() {
    if (IsSetupUICalled()) {
        return;
    }

    Settings settings(kSettingsNamespace);
    is_landscape_ = settings.GetBool(kOrientationKey, false);
    lv_display_set_rotation(display_, is_landscape_ ? LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_0);

    SpiLcdDisplay::SetupUI();

    DisplayLockGuard lock(this);
    if (low_battery_popup_ != nullptr) {
        lv_obj_remove_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(low_battery_popup_, 1, 1);
        lv_obj_align(low_battery_popup_, LV_ALIGN_TOP_LEFT, -LV_HOR_RES, -LV_VER_RES);
    }
    CreateSettingsMenuLocked();
    ApplyMenuThemeLocked();
    RefreshSettingsMenuLocked();
}

void ZhengchenMinicamLcdDisplay::SetTheme(Theme* theme) {
    SpiLcdDisplay::SetTheme(theme);

    if (low_battery_popup_ != nullptr) {
        DisplayLockGuard lock(this);
        lv_obj_remove_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(low_battery_popup_, 1, 1);
        lv_obj_align(low_battery_popup_, LV_ALIGN_TOP_LEFT, -LV_HOR_RES, -LV_VER_RES);
    }

    if (settings_menu_ == nullptr) {
        return;
    }

    DisplayLockGuard lock(this);
    ApplyMenuThemeLocked();
    RefreshSettingsMenuLocked();
}

void ZhengchenMinicamLcdDisplay::UpdateStatusBar(bool update_all) {
    auto* saved_low_battery_popup = low_battery_popup_;
    auto* saved_low_battery_label = low_battery_label_;
    low_battery_popup_ = nullptr;
    low_battery_label_ = nullptr;

    SpiLcdDisplay::UpdateStatusBar(update_all);

    low_battery_popup_ = saved_low_battery_popup;
    low_battery_label_ = saved_low_battery_label;

    DisplayLockGuard lock(this);
    if (low_battery_popup_ != nullptr) {
        lv_obj_remove_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(low_battery_popup_, 1, 1);
        lv_obj_align(low_battery_popup_, LV_ALIGN_TOP_LEFT, -LV_HOR_RES, -LV_VER_RES);
    }
}

void ZhengchenMinicamLcdDisplay::ShowSettingsMenu() {
    DisplayLockGuard lock(this);
    if (settings_menu_ == nullptr) {
        return;
    }
    lv_obj_remove_flag(settings_menu_, LV_OBJ_FLAG_HIDDEN);
    RefreshSettingsMenuLocked();
}

void ZhengchenMinicamLcdDisplay::HideSettingsMenu() {
    DisplayLockGuard lock(this);
    if (settings_menu_ == nullptr) {
        return;
    }
    lv_obj_add_flag(settings_menu_, LV_OBJ_FLAG_HIDDEN);
}

void ZhengchenMinicamLcdDisplay::ToggleSettingsMenu() {
    if (IsSettingsMenuVisible()) {
        HideSettingsMenu();
    } else {
        ShowSettingsMenu();
    }
}

bool ZhengchenMinicamLcdDisplay::IsSettingsMenuVisible() const {
    return settings_menu_ != nullptr && !lv_obj_has_flag(settings_menu_, LV_OBJ_FLAG_HIDDEN);
}

void ZhengchenMinicamLcdDisplay::SelectPreviousMenuItem() {
    DisplayLockGuard lock(this);
    if (settings_menu_ == nullptr) {
        return;
    }
    SetSelectedIndex(selected_index_ - 1);
    RefreshSettingsMenuLocked();
}

void ZhengchenMinicamLcdDisplay::SelectNextMenuItem() {
    DisplayLockGuard lock(this);
    if (settings_menu_ == nullptr) {
        return;
    }
    SetSelectedIndex(selected_index_ + 1);
    RefreshSettingsMenuLocked();
}

void ZhengchenMinicamLcdDisplay::ActivateSelectedMenuItem() {
    switch (static_cast<MenuItem>(selected_index_)) {
    case MenuItem::Aec:
        if (aec_toggle_callback_) {
            aec_toggle_callback_();
        }
        ShowNotification((std::string("AEC: ") + (aec_getter_ != nullptr && aec_getter_() ? "On" : "Off")).c_str());
        break;
    case MenuItem::Orientation:
        if (orientation_toggle_callback_) {
            orientation_toggle_callback_();
        }
        ShowNotification((std::string("Orientation: ") + (orientation_getter_ != nullptr && orientation_getter_() ? "Landscape" : "Portrait")).c_str());
        break;
    case MenuItem::Reprovision:
        HideSettingsMenu();
        if (reprovision_callback_) {
            reprovision_callback_();
        }
        ShowNotification("重新配网 / Reprovision");
        break;
    case MenuItem::Count:
        break;
    }

    DisplayLockGuard lock(this);
    if (settings_menu_ != nullptr) {
        RefreshSettingsMenuLocked();
    }
}

void ZhengchenMinicamLcdDisplay::SetAecHandlers(std::function<bool()> getter, std::function<void()> toggle_callback) {
    aec_getter_ = std::move(getter);
    aec_toggle_callback_ = std::move(toggle_callback);
}

void ZhengchenMinicamLcdDisplay::SetOrientationHandlers(std::function<bool()> getter, std::function<void()> toggle_callback) {
    orientation_getter_ = std::move(getter);
    orientation_toggle_callback_ = std::move(toggle_callback);
}

void ZhengchenMinicamLcdDisplay::SetReprovisionCallback(std::function<void()> callback) {
    reprovision_callback_ = std::move(callback);
}

bool ZhengchenMinicamLcdDisplay::IsLandscape() const {
    return is_landscape_;
}

void ZhengchenMinicamLcdDisplay::ToggleOrientation() {
    is_landscape_ = !is_landscape_;
    Settings settings(kSettingsNamespace, true);
    settings.SetBool(kOrientationKey, is_landscape_);

    DisplayLockGuard lock(this);
    esp_lcd_panel_swap_xy(panel_, is_landscape_ ? DISPLAY_SWAP_XY_1 : DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel_,
        is_landscape_ ? DISPLAY_MIRROR_X_1 : DISPLAY_MIRROR_X,
        is_landscape_ ? DISPLAY_MIRROR_Y_1 : DISPLAY_MIRROR_Y);
    lv_display_set_rotation(display_, is_landscape_ ? LV_DISPLAY_ROTATION_90 : LV_DISPLAY_ROTATION_0);

    if (container_ != nullptr) {
        lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    }
    if (top_bar_ != nullptr) {
        lv_obj_set_width(top_bar_, LV_HOR_RES);
        lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_update_layout(top_bar_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_set_width(status_bar_, LV_HOR_RES);
        lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);
    }
    if (notification_label_ != nullptr) {
        lv_obj_set_width(notification_label_, LV_HOR_RES * 0.75);
        lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);
    }
    if (status_label_ != nullptr) {
        lv_obj_set_width(status_label_, LV_HOR_RES * 0.75);
        lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);
    }
    if (bottom_bar_ != nullptr) {
        lv_obj_set_width(bottom_bar_, LV_HOR_RES);
        lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);
    }
    if (chat_message_label_ != nullptr) {
        lv_obj_set_width(chat_message_label_, LV_HOR_RES - 8);
        lv_obj_align(chat_message_label_, LV_ALIGN_CENTER, 0, 0);
    }
    if (content_ != nullptr) {
        const lv_coord_t top_bar_height = top_bar_ != nullptr ? lv_obj_get_height(top_bar_) : 0;
        const lv_coord_t status_bar_height = status_bar_ != nullptr ? lv_obj_get_height(status_bar_) : 0;
        lv_obj_set_size(content_, LV_HOR_RES, LV_VER_RES - status_bar_height - top_bar_height);
        if (status_bar_ != nullptr) {
            lv_obj_align_to(content_, status_bar_, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
        } else if (top_bar_ != nullptr) {
            lv_obj_align_to(content_, top_bar_, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
        } else {
            lv_obj_align(content_, LV_ALIGN_TOP_MID, 0, 0);
        }
        lv_obj_update_layout(content_);
    }
    if (low_battery_popup_ != nullptr) {
        lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, lv_obj_get_height(low_battery_popup_));
        lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -4);
    }
#if !CONFIG_USE_WECHAT_MESSAGE_STYLE
    if (preview_image_ != nullptr) {
        lv_obj_set_size(preview_image_, LV_HOR_RES, LV_VER_RES);
        lv_obj_align(preview_image_, LV_ALIGN_CENTER, 0, 0);
        if (preview_image_cached_ != nullptr) {
            ApplyFullscreenPreviewTransform(preview_image_, preview_image_cached_->image_dsc(), is_landscape_);
        }
    }
#endif

    if (settings_menu_ != nullptr) {
        ApplyMenuThemeLocked();
        lv_obj_set_size(settings_menu_, LV_HOR_RES - 24, LV_VER_RES - 48);
        lv_obj_align(settings_menu_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_update_layout(settings_menu_);
        RefreshSettingsMenuLocked();
        lv_obj_update_layout(settings_menu_);
    }
}


void ZhengchenMinicamLcdDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    DisplayLockGuard lock(this);
    if (content_ == nullptr) {
        return;
    }

    if (image == nullptr) {
        return;
    }

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    lv_obj_t* img_bubble = lv_obj_create(content_);
    lv_obj_set_style_radius(img_bubble, 8, 0);
    lv_obj_set_scrollbar_mode(img_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(img_bubble, 0, 0);
    lv_obj_set_style_pad_all(img_bubble, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_bg_color(img_bubble, lvgl_theme->assistant_bubble_color(), 0);
    lv_obj_set_style_bg_opa(img_bubble, LV_OPA_70, 0);
    lv_obj_set_user_data(img_bubble, (void*)"image");

    lv_obj_t* preview_image = lv_image_create(img_bubble);

    lv_coord_t content_width = lv_obj_get_content_width(content_);
    lv_coord_t content_height = lv_obj_get_height(content_);
    if (content_width <= 0) {
        content_width = LV_HOR_RES;
    }
    if (content_height <= 0) {
        content_height = LV_VER_RES;
    }

    lv_coord_t max_width = content_width * 90 / 100;
    lv_coord_t max_height = content_height * 70 / 100;

    auto img_dsc = image->image_dsc();
    lv_coord_t img_width = img_dsc->header.w;
    lv_coord_t img_height = img_dsc->header.h;
    if (img_width == 0 || img_height == 0) {
        img_width = max_width;
        img_height = max_height;
        ESP_LOGW("LcdDisplay", "Invalid image dimensions: %ld x %ld, using default dimensions: %ld x %ld", img_width, img_height, max_width, max_height);
    }

    lv_coord_t display_width = is_landscape_ ? img_height : img_width;
    lv_coord_t display_height = is_landscape_ ? img_width : img_height;
    lv_coord_t zoom_w = (max_width * 256) / display_width;
    lv_coord_t zoom_h = (max_height * 256) / display_height;
    lv_coord_t zoom = (zoom_w < zoom_h) ? zoom_w : zoom_h;
    if (zoom > 256) {
        zoom = 256;
    }

    lv_image_set_src(preview_image, img_dsc);
    lv_image_set_rotation(preview_image, is_landscape_ ? 900 : 0);
    lv_image_set_scale(preview_image, zoom);

    LvglImage* raw_image = image.release();
    lv_obj_add_event_cb(preview_image, [](lv_event_t* e) {
        LvglImage* img = (LvglImage*)lv_event_get_user_data(e);
        if (img != nullptr) {
            delete img;
        }
    }, LV_EVENT_DELETE, (void*)raw_image);

    lv_coord_t scaled_width = (display_width * zoom) / 256;
    lv_coord_t scaled_height = (display_height * zoom) / 256;
    lv_obj_set_width(img_bubble, scaled_width + 16);
    lv_obj_set_height(img_bubble, scaled_height + 16);
    lv_obj_set_style_flex_grow(img_bubble, 0, 0);
    lv_obj_center(preview_image);
    lv_obj_align(img_bubble, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_scroll_to_view_recursive(img_bubble, LV_ANIM_ON);
#else
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        ESP_LOGE("LcdDisplay", "Preview image is not initialized");
        return;
    }

    if (image == nullptr) {
        esp_timer_stop(preview_timer_);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        preview_image_cached_.reset();
        if (gif_controller_) {
            gif_controller_->Start();
        }
        return;
    }

    preview_image_cached_ = std::move(image);
    auto img_dsc = preview_image_cached_->image_dsc();
    lv_image_set_src(preview_image_, img_dsc);
    ApplyFullscreenPreviewTransform(preview_image_, img_dsc, is_landscape_);
    lv_obj_set_size(preview_image_, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(preview_image_, LV_ALIGN_CENTER, 0, 0);

    if (gif_controller_) {
        gif_controller_->Stop();
    }
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    esp_timer_stop(preview_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, PREVIEW_IMAGE_DURATION_MS * 1000));
#endif
}

void ZhengchenMinicamLcdDisplay::CreateSettingsMenuLocked() {
    if (settings_menu_ != nullptr) {
        return;
    }

    auto* screen = lv_screen_active();
    auto* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto* text_font = lvgl_theme->text_font()->font();
    auto metrics = GetSettingsMenuMetrics(is_landscape_);

    settings_menu_ = lv_obj_create(screen);
    lv_obj_set_size(settings_menu_, LV_HOR_RES - metrics.menu_margin, LV_VER_RES - metrics.menu_margin);
    lv_obj_align(settings_menu_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(settings_menu_, 12, 0);
    lv_obj_set_style_border_width(settings_menu_, 2, 0);
    lv_obj_set_style_pad_left(settings_menu_, metrics.menu_pad_x, 0);
    lv_obj_set_style_pad_right(settings_menu_, metrics.menu_pad_x, 0);
    lv_obj_set_style_pad_top(settings_menu_, metrics.menu_pad_y, 0);
    lv_obj_set_style_pad_bottom(settings_menu_, metrics.menu_pad_y, 0);
    lv_obj_set_scrollbar_mode(settings_menu_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(settings_menu_, LV_DIR_VER);
    lv_obj_set_flex_flow(settings_menu_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settings_menu_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    settings_title_label_ = lv_label_create(settings_menu_);
    lv_label_set_text(settings_title_label_, metrics.title_text);
    lv_obj_set_style_text_font(settings_title_label_, text_font, 0);
    lv_obj_set_style_text_align(settings_title_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(settings_title_label_, metrics.line_space, 0);
    lv_obj_set_width(settings_title_label_, LV_PCT(100));
    lv_obj_set_style_pad_bottom(settings_title_label_, metrics.title_pad_bottom, 0);

    for (size_t i = 0; i < menu_item_labels_.size(); ++i) {
        auto* row = lv_obj_create(settings_menu_);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_left(row, metrics.row_pad_x, 0);
        lv_obj_set_style_pad_right(row, metrics.row_pad_x, 0);
        lv_obj_set_style_pad_top(row, metrics.row_pad_y, 0);
        lv_obj_set_style_pad_bottom(row, metrics.row_pad_y, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        menu_item_rows_[i] = row;

        auto* label = lv_label_create(row);
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_set_style_text_font(label, text_font, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_line_space(label, metrics.line_space, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        menu_item_labels_[i] = label;
    }

    lv_obj_add_flag(settings_menu_, LV_OBJ_FLAG_HIDDEN);
}

void ZhengchenMinicamLcdDisplay::ApplyMenuThemeLocked() {
    if (settings_menu_ == nullptr) {
        return;
    }

    auto* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto* text_font = lvgl_theme->text_font()->font();
    auto metrics = GetSettingsMenuMetrics(is_landscape_);

    auto bg_color = lvgl_theme->background_color();
    auto text_color = lvgl_theme->text_color();
    auto border_color = lvgl_theme->border_color();

    lv_obj_set_style_bg_color(settings_menu_, bg_color, 0);
    lv_obj_set_style_bg_opa(settings_menu_, LV_OPA_90, 0);
    lv_obj_set_style_border_color(settings_menu_, border_color, 0);
    lv_obj_set_size(settings_menu_, LV_HOR_RES - metrics.menu_margin, LV_VER_RES - metrics.menu_margin);
    lv_obj_set_style_pad_left(settings_menu_, metrics.menu_pad_x, 0);
    lv_obj_set_style_pad_right(settings_menu_, metrics.menu_pad_x, 0);
    lv_obj_set_style_pad_top(settings_menu_, metrics.menu_pad_y, 0);
    lv_obj_set_style_pad_bottom(settings_menu_, metrics.menu_pad_y, 0);

    if (settings_title_label_ != nullptr) {
        lv_obj_set_style_text_font(settings_title_label_, text_font, 0);
        lv_label_set_text(settings_title_label_, metrics.title_text);
        lv_obj_set_style_text_line_space(settings_title_label_, metrics.line_space, 0);
        lv_obj_set_style_text_color(settings_title_label_, text_color, 0);
        lv_obj_set_style_pad_bottom(settings_title_label_, metrics.title_pad_bottom, 0);
    }

    for (size_t i = 0; i < menu_item_rows_.size(); ++i) {
        auto* row = menu_item_rows_[i];
        auto* label = menu_item_labels_[i];
        if (row == nullptr || label == nullptr) {
            continue;
        }
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(label, text_font, 0);
        lv_obj_set_style_text_line_space(label, metrics.line_space, 0);
        lv_obj_set_style_text_color(label, text_color, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_left(row, metrics.row_pad_x, 0);
        lv_obj_set_style_pad_right(row, metrics.row_pad_x, 0);
        lv_obj_set_style_pad_top(row, metrics.row_pad_y, 0);
        lv_obj_set_style_pad_bottom(row, metrics.row_pad_y, 0);
        lv_obj_set_style_radius(row, 8, 0);
    }
}

void ZhengchenMinicamLcdDisplay::RefreshSettingsMenuLocked() {
    if (settings_menu_ == nullptr) {
        return;
    }

    auto* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_color = lvgl_theme->text_color();
    auto bg_color = lvgl_theme->background_color();

    if (settings_title_label_ != nullptr) {
        lv_label_set_text(settings_title_label_, GetSettingsMenuMetrics(is_landscape_).title_text);
    }

    for (size_t i = 0; i < menu_item_labels_.size(); ++i) {
        auto* row = menu_item_rows_[i];
        auto* label = menu_item_labels_[i];
        if (row == nullptr || label == nullptr) {
            continue;
        }

        const bool selected = static_cast<int>(i) == selected_index_;
        lv_label_set_text(label, GetMenuLabel(static_cast<MenuItem>(i)));
        lv_obj_set_style_text_color(label, selected ? bg_color : text_color, 0);
        lv_obj_set_style_bg_color(row, selected ? text_color : bg_color, 0);
        lv_obj_set_style_bg_opa(row, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

void ZhengchenMinicamLcdDisplay::SetSelectedIndex(int index) {
    const int count = static_cast<int>(MenuItem::Count);
    if (count <= 0) {
        selected_index_ = 0;
        return;
    }

    if (index < 0) {
        selected_index_ = count - 1;
    } else if (index >= count) {
        selected_index_ = 0;
    } else {
        selected_index_ = index;
    }
}

const char* ZhengchenMinicamLcdDisplay::GetAecLabel() const {
    return aec_getter_ != nullptr && aec_getter_() ? "开启" : "关闭";
}

const char* ZhengchenMinicamLcdDisplay::GetOrientationLabel() const {
    return orientation_getter_ != nullptr && orientation_getter_() ? "横屏" : "竖屏";
}

const char* ZhengchenMinicamLcdDisplay::GetMenuLabel(MenuItem item) const {
    static std::string label;
    switch (item) {
    case MenuItem::Aec:
        if (is_landscape_) {
            label = std::string("语音打断:") + GetAecLabel() + "\nVoice interruption: " + (aec_getter_ != nullptr && aec_getter_() ? "On" : "Off");
        } else {
            label = std::string("语音打断 开关：") + GetAecLabel() + "\nVoice interruption: " + (aec_getter_ != nullptr && aec_getter_() ? "On" : "Off");
        }
        break;
    case MenuItem::Orientation:
        if (is_landscape_) {
            label = std::string("方向:") + GetOrientationLabel() + "\nOri: " + (orientation_getter_ != nullptr && orientation_getter_() ? "Land" : "Port");
        } else {
            label = std::string("屏幕方向：") + GetOrientationLabel() + "\nOrientation: " + (orientation_getter_ != nullptr && orientation_getter_() ? "Landscape" : "Portrait");
        }
        break;
    case MenuItem::Reprovision:
        label = is_landscape_ ? "重新配网\nReprovision" : "重新配网\nReprovision";
        break;
    case MenuItem::Count:
        label.clear();
        break;
    }
    return label.c_str();
}
