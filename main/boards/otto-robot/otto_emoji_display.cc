#include "otto_emoji_display.h"

#include <esp_log.h>

#include <cstring>

#include "assets/lang_config.h"
#include "display/lvgl_display/emoji_collection.h"
#include "display/lvgl_display/lvgl_image.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "otto_emoji_gif.h"

#define TAG "OttoEmojiDisplay"
OttoEmojiDisplay::OttoEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
    InitializeOttoEmojis();
    SetupPreviewImage();
    SetTheme(LvglThemeManager::GetInstance().GetTheme("dark"));
}

void OttoEmojiDisplay::SetupPreviewImage() {
    DisplayLockGuard lock(this);
    lv_obj_set_size(preview_image_, width_ , height_ );
}

void OttoEmojiDisplay::InitializeOttoEmojis() {
    ESP_LOGI(TAG, "初始化Otto GIF表情");

    auto otto_emoji_collection = std::make_shared<EmojiCollection>();

    // 中性/平静类表情 -> staticstate
    otto_emoji_collection->AddEmoji("staticstate", new LvglRawImage((void*)staticstate.data, staticstate.data_size));
    otto_emoji_collection->AddEmoji("neutral", new LvglRawImage((void*)staticstate.data, staticstate.data_size));
    otto_emoji_collection->AddEmoji("relaxed", new LvglRawImage((void*)staticstate.data, staticstate.data_size));
    otto_emoji_collection->AddEmoji("sleepy", new LvglRawImage((void*)staticstate.data, staticstate.data_size));
    otto_emoji_collection->AddEmoji("idle", new LvglRawImage((void*)staticstate.data, staticstate.data_size));

    // 积极/开心类表情 -> happy
    otto_emoji_collection->AddEmoji("happy", new LvglRawImage((void*)happy.data, happy.data_size));
    otto_emoji_collection->AddEmoji("laughing", new LvglRawImage((void*)happy.data, happy.data_size));
    otto_emoji_collection->AddEmoji("funny", new LvglRawImage((void*)happy.data, happy.data_size));
    otto_emoji_collection->AddEmoji("loving", new LvglRawImage((void*)happy.data, happy.data_size));
    otto_emoji_collection->AddEmoji("confident", new LvglRawImage((void*)happy.data, happy.data_size));
    otto_emoji_collection->AddEmoji("winking", new LvglRawImage((void*)happy.data, happy.data_size));
    otto_emoji_collection->AddEmoji("cool", new LvglRawImage((void*)happy.data, happy.data_size));
    otto_emoji_collection->AddEmoji("delicious", new LvglRawImage((void*)happy.data, happy.data_size));
    otto_emoji_collection->AddEmoji("kissy", new LvglRawImage((void*)happy.data, happy.data_size));
    otto_emoji_collection->AddEmoji("silly", new LvglRawImage((void*)happy.data, happy.data_size));

    // 悲伤类表情 -> sad
    otto_emoji_collection->AddEmoji("sad", new LvglRawImage((void*)sad.data, sad.data_size));
    otto_emoji_collection->AddEmoji("crying", new LvglRawImage((void*)sad.data, sad.data_size));

    // 愤怒类表情 -> anger
    otto_emoji_collection->AddEmoji("anger", new LvglRawImage((void*)anger.data, anger.data_size));
    otto_emoji_collection->AddEmoji("angry", new LvglRawImage((void*)anger.data, anger.data_size));

    // 惊讶类表情 -> scare
    otto_emoji_collection->AddEmoji("scare", new LvglRawImage((void*)scare.data, scare.data_size));
    otto_emoji_collection->AddEmoji("surprised", new LvglRawImage((void*)scare.data, scare.data_size));
    otto_emoji_collection->AddEmoji("shocked", new LvglRawImage((void*)scare.data, scare.data_size));

    // 思考/困惑类表情 -> buxue
    otto_emoji_collection->AddEmoji("buxue", new LvglRawImage((void*)buxue.data, buxue.data_size));
    otto_emoji_collection->AddEmoji("thinking", new LvglRawImage((void*)buxue.data, buxue.data_size));
    otto_emoji_collection->AddEmoji("confused", new LvglRawImage((void*)buxue.data, buxue.data_size));
    otto_emoji_collection->AddEmoji("embarrassed", new LvglRawImage((void*)buxue.data, buxue.data_size));

    // 将表情集合添加到主题中
    auto& theme_manager = LvglThemeManager::GetInstance();
    auto light_theme = theme_manager.GetTheme("light");
    auto dark_theme = theme_manager.GetTheme("dark");

    if (light_theme != nullptr) {
        light_theme->set_emoji_collection(otto_emoji_collection);
    }
    if (dark_theme != nullptr) {
        dark_theme->set_emoji_collection(otto_emoji_collection);
    }

    // 设置默认表情为staticstate
    SetEmotion("staticstate");

    ESP_LOGI(TAG, "Otto GIF表情初始化完成");
}

LV_FONT_DECLARE(OTTO_ICON_FONT);
void OttoEmojiDisplay::SetStatus(const char* status) {
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    DisplayLockGuard lock(this);
    if (!status) {
        ESP_LOGE(TAG, "SetStatus: status is nullptr");
        return;
    }

    if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        lv_obj_set_style_text_font(status_label_, &OTTO_ICON_FONT, 0);
        lv_label_set_text(status_label_, "\xEF\x84\xB0");  // U+F130 麦克风图标
        lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        lv_obj_set_style_text_font(status_label_, &OTTO_ICON_FONT, 0);
        lv_label_set_text(status_label_, "\xEF\x80\xA8");  // U+F028 说话图标
        lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    } else if (strcmp(status, Lang::Strings::CONNECTING) == 0) {
        lv_obj_set_style_text_font(status_label_, &OTTO_ICON_FONT, 0);
        lv_label_set_text(status_label_, "\xEF\x83\x81");  // U+F0c1 连接图标
        lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        lv_obj_set_style_text_font(status_label_, text_font, 0);
        lv_label_set_text(status_label_, "");
        lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_set_style_text_font(status_label_, text_font, 0);
    lv_label_set_text(status_label_, status);
}

void OttoEmojiDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        ESP_LOGE(TAG, "Preview image is not initialized");
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
    // 设置图片源并显示预览图片
    lv_image_set_src(preview_image_, img_dsc);
    lv_image_set_rotation(preview_image_, -900);
    if (img_dsc->header.w > 0 && img_dsc->header.h > 0) {
        // zoom factor 1.0
        lv_image_set_scale(preview_image_, 256 * width_ / img_dsc->header.w);
    }

    // Hide emoji_box_
    if (gif_controller_) {
        gif_controller_->Stop();
    }
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    esp_timer_stop(preview_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, PREVIEW_IMAGE_DURATION_MS * 1000));
}