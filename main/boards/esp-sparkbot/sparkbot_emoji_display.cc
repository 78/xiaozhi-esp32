#include "sparkbot_emoji_display.h"

#include <esp_log.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "display/lcd_display.h"
#include "font_awesome_symbols.h"

#define TAG "SparkbotEmojiDisplay"

// 表情映射表 - 将原版21种表情映射到现有6个GIF
const SparkbotEmojiDisplay::EmotionMap SparkbotEmojiDisplay::emotion_maps_[] = {
    // 中性/平静类表情 -> staticstate
    {"neutral", &staticstate},
    {"relaxed", &staticstate},
    {"sleepy", &staticstate},

    // 积极/开心类表情 -> happy
    {"happy", &happy},
    {"laughing", &happy},
    {"funny", &happy},
    {"loving", &happy},
    {"confident", &happy},
    {"winking", &happy},
    {"cool", &happy},
    {"delicious", &happy},
    {"kissy", &happy},
    {"silly", &happy},

    // 悲伤类表情 -> sad
    {"sad", &sad},
    {"crying", &sad},

    // 愤怒类表情 -> anger
    {"angry", &anger},

    // 惊讶类表情 -> scare
    {"surprised", &scare},
    {"shocked", &scare},

    // 思考/困惑类表情 -> buxue
    {"thinking", &buxue},
    {"confused", &buxue},
    {"embarrassed", &buxue},

    {nullptr, nullptr}  // 结束标记
};

SparkbotEmojiDisplay::SparkbotEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   int width, int height, int offset_x, int offset_y, bool mirror_x,
                                   bool mirror_y, bool swap_xy, DisplayFonts fonts)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    fonts),
      emotion_gif_(nullptr),
      preview_image_obj_(nullptr),
      preview_timer_(nullptr) {
    
    // 创建预览图片定时器
    esp_timer_create_args_t preview_timer_args = {
        .callback = [](void *arg) {
            SparkbotEmojiDisplay *display = static_cast<SparkbotEmojiDisplay*>(arg);
            display->HidePreviewImage();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "preview_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&preview_timer_args, &preview_timer_));
    
    SetupGifContainer();
};

SparkbotEmojiDisplay::~SparkbotEmojiDisplay() {
    if (preview_timer_ != nullptr) {
        esp_timer_stop(preview_timer_);
        esp_timer_delete(preview_timer_);
        preview_timer_ = nullptr;
    }
}

void SparkbotEmojiDisplay::SetupGifContainer() {
    DisplayLockGuard lock(this);

    if (emotion_label_) {
        lv_obj_del(emotion_label_);
    }

    if (chat_message_label_) {
        lv_obj_del(chat_message_label_);
    }
    if (preview_image_obj_) {
        lv_obj_del(preview_image_obj_);
    }
    if (content_) {
        lv_obj_del(content_);
    }

    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(content_, LV_HOR_RES, LV_HOR_RES);
    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_center(content_);

    emotion_label_ = lv_label_create(content_);
    lv_label_set_text(emotion_label_, "");
    lv_obj_set_width(emotion_label_, 0);
    lv_obj_set_style_border_width(emotion_label_, 0, 0);
    lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);

    emotion_gif_ = lv_gif_create(content_);
    int gif_size = LV_HOR_RES;
    lv_obj_set_size(emotion_gif_, gif_size, gif_size);
    lv_obj_set_style_border_width(emotion_gif_, 0, 0);
    lv_obj_set_style_bg_opa(emotion_gif_, LV_OPA_TRANSP, 0);
    lv_obj_center(emotion_gif_);
    lv_gif_set_src(emotion_gif_, &staticstate);

    // 创建图片预览组件
    preview_image_obj_ = lv_image_create(content_);
    lv_obj_set_size(preview_image_obj_, LV_HOR_RES * 0.8, LV_HOR_RES * 0.8);
    lv_obj_set_style_border_width(preview_image_obj_, 0, 0);
    lv_obj_set_style_bg_opa(preview_image_obj_, LV_OPA_TRANSP, 0);
    lv_obj_center(preview_image_obj_);
    lv_obj_add_flag(preview_image_obj_, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(chat_message_label_, lv_color_white(), 0);
    lv_obj_set_style_border_width(chat_message_label_, 0, 0);

    lv_obj_set_style_bg_opa(chat_message_label_, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(chat_message_label_, lv_color_black(), 0);
    lv_obj_set_style_pad_ver(chat_message_label_, 5, 0);

    lv_obj_align(chat_message_label_, LV_ALIGN_BOTTOM_MID, 0, 0);

    LcdDisplay::SetTheme("dark");
}

void SparkbotEmojiDisplay::SetEmotion(const char* emotion) {
    if (!emotion || !emotion_gif_) {
        return;
    }

    DisplayLockGuard lock(this);


    // 如果正在预览图片，则隐藏图片预览
    /* if (preview_image_obj_ && !lv_obj_has_flag(preview_image_obj_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(preview_image_obj_, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "隐藏图片预览以显示表情");
    } */
    
    for (const auto& map : emotion_maps_) {
        if (map.name && strcmp(map.name, emotion) == 0) {
            lv_gif_set_src(emotion_gif_, map.gif);
            ESP_LOGI(TAG, "设置表情: %s", emotion);
            return;
        }
    }

    lv_gif_set_src(emotion_gif_, &staticstate);
    ESP_LOGI(TAG, "未知表情'%s'，使用默认", emotion);
}

void SparkbotEmojiDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    if (content == nullptr || strlen(content) == 0) {
        lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(chat_message_label_, content);
    lv_obj_clear_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "设置聊天消息 [%s]: %s", role, content);
}

void SparkbotEmojiDisplay::SetIcon(const char* icon) {
    if (!icon) {
        return;
    }

    DisplayLockGuard lock(this);

    if (chat_message_label_ != nullptr) {
        std::string icon_message = std::string(icon) + " ";

        if (strcmp(icon, FONT_AWESOME_DOWNLOAD) == 0) {
            icon_message += "正在升级...";
        } else {
            icon_message += "系统状态";
        }

        lv_label_set_text(chat_message_label_, icon_message.c_str());
        lv_obj_clear_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);

        ESP_LOGI(TAG, "设置图标: %s", icon);
    }
}

void SparkbotEmojiDisplay::HidePreviewImage() {
    DisplayLockGuard lock(this);
    if (preview_image_obj_ == nullptr) {
        return;
    }
    
    // 隐藏预览图片，显示GIF表情
    lv_obj_add_flag(preview_image_obj_, LV_OBJ_FLAG_HIDDEN);
    if (emotion_gif_ != nullptr) {
        lv_obj_clear_flag(emotion_gif_, LV_OBJ_FLAG_HIDDEN);
    }
    
    ESP_LOGI(TAG, "预览图片定时隐藏，恢复表情显示");
}

void SparkbotEmojiDisplay::SetPreviewImage(const lv_img_dsc_t* img_dsc) {
    DisplayLockGuard lock(this);
    if (preview_image_obj_ == nullptr) {
        return;
    }
    
    // 停止之前的定时器
    if (preview_timer_ != nullptr) {
        esp_timer_stop(preview_timer_);
    }
    
    if (img_dsc != nullptr) {
        // 计算合适的缩放比例
        lv_coord_t max_size = LV_HOR_RES * 0.8;  // 最大尺寸为屏幕宽度的80%
        lv_coord_t img_width = img_dsc->header.w;
        lv_coord_t img_height = img_dsc->header.h;
        
        // 计算缩放因子，保持宽高比
        lv_coord_t zoom_w = (max_size * 256) / img_width;
        lv_coord_t zoom_h = (max_size * 256) / img_height;
        lv_coord_t zoom = (zoom_w < zoom_h) ? zoom_w : zoom_h;
        
        // 确保缩放不超过100%
        if (zoom > 256) zoom = 256;
        
        // 设置图片源和缩放
        lv_image_set_src(preview_image_obj_, img_dsc);
        lv_image_set_scale(preview_image_obj_, zoom);
        
        // 显示预览图片，隐藏GIF表情
        lv_obj_clear_flag(preview_image_obj_, LV_OBJ_FLAG_HIDDEN);
        if (emotion_gif_ != nullptr) {
            lv_obj_add_flag(emotion_gif_, LV_OBJ_FLAG_HIDDEN);
        }
        
        ESP_LOGI(TAG, "显示图片预览，尺寸: %ldx%ld，缩放: %ld", (long)img_width, (long)img_height, (long)zoom);
        
        // 启动2秒定时器，自动隐藏预览图片
        if (preview_timer_ != nullptr) {
            ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, 2000000));  // 2秒 = 2,000,000微秒
            ESP_LOGI(TAG, "启动2秒定时器，将自动隐藏预览图片");
        }

    } else {
        // 隐藏预览图片，显示GIF表情
        lv_obj_add_flag(preview_image_obj_, LV_OBJ_FLAG_HIDDEN);
        if (emotion_gif_ != nullptr) {
            lv_obj_clear_flag(emotion_gif_, LV_OBJ_FLAG_HIDDEN);
        }
        
        ESP_LOGI(TAG, "隐藏图片预览，恢复表情显示");
    }
}
