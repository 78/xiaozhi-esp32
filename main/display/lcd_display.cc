#include "lcd_display.h"

#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/ledc.h>
#include <vector>
#include <esp_lvgl_port.h>
#include "board.h"

#define TAG "LcdDisplay"
#define LCD_LEDC_CH LEDC_CHANNEL_0

LV_FONT_DECLARE(font_awesome_30_4);


LcdDisplay::LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           gpio_num_t backlight_pin, bool backlight_output_invert,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : panel_io_(panel_io), panel_(panel), backlight_pin_(backlight_pin), backlight_output_invert_(backlight_output_invert),
      fonts_(fonts) {
    width_ = width;
    height_ = height;

    InitializeBacklight(backlight_pin);

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    SetBacklight(100);

    SetupUI();
}

LcdDisplay::~LcdDisplay() {
    // 然后再清理 LVGL 对象
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }
    if (display_ != nullptr) {
        lv_display_delete(display_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    if (config_container_ != nullptr) {
        lv_obj_del(config_container_);
    }
    if (config_text_panel_ != nullptr) {
        lv_obj_del(config_text_panel_);
    }
    if (config_qrcode_panel_ != nullptr) {
        lv_obj_del(config_qrcode_panel_);
    }
    if (qrcode_label_ != nullptr) {
        lv_obj_del(qrcode_label_);
    }
    if (smartconfig_qrcode_ != nullptr) {
        lv_obj_del(smartconfig_qrcode_);
    }
}

void LcdDisplay::InitializeBacklight(gpio_num_t backlight_pin) {
    if (backlight_pin == GPIO_NUM_NC) {
        return;
    }

    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t backlight_channel = {
        .gpio_num = backlight_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = backlight_output_invert_,
        }
    };
    const ledc_timer_config_t backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };

    ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));
}

void LcdDisplay::SetBacklight(int brightness) {
    if (backlight_pin_ == GPIO_NUM_NC) {
        return;
    }

    if (brightness > 100) {
        brightness = 100;
    }
    backlight_brightness_ = brightness;

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness);
    // LEDC resolution set to 10bits, thus: 100% = 1023
    uint32_t duty_cycle = (1023 * brightness) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
}

bool LcdDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void LcdDisplay::Unlock() {
    lvgl_port_unlock();
}

void LcdDisplay::SetupUI() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    
    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 2, 0);
    lv_obj_set_style_pad_right(status_bar_, 2, 0);

#if defined(CONFIG_BOARD_TYPE_PEILIAO_C3) || defined(CONFIG_BOARD_TYPE_PEILIAO_S3)    
    logo_label_ = lv_label_create(status_bar_);
    lv_label_set_text(logo_label_, "");
    lv_obj_set_style_text_font(logo_label_, fonts_.text_font, 0);
#else    
    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
#endif

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "通知");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(status_label_, "正在初始化");
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

#if defined(CONFIG_BOARD_TYPE_PEILIAO_C3) || defined(CONFIG_BOARD_TYPE_PEILIAO_S3)    
    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
#endif

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // 垂直布局（从上到下）
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY); // 子对象居中对齐，等距分布

    // 创建配置页面
    config_container_ = lv_obj_create(content_);
    lv_obj_remove_style_all(config_container_); // 清除默认样式
    lv_obj_set_size(config_container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(config_container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(config_container_, 10, 0); // 整体边距
    lv_obj_set_style_pad_top(config_container_, 25, 0);  //顶部外边距加20像素
    lv_obj_set_style_flex_main_place(config_container_, LV_FLEX_ALIGN_CENTER, 0); // 主轴居中
    lv_obj_set_style_flex_cross_place(config_container_, LV_FLEX_ALIGN_CENTER, 0); // 交叉轴居中

    // 左侧文本说明区
    config_text_panel_ = lv_label_create(config_container_);
    lv_obj_set_width(config_text_panel_, LV_HOR_RES - 150 - 20);
    lv_label_set_text(config_text_panel_,"");
    lv_obj_set_style_text_font(config_text_panel_, fonts_.text_font, 0);
    lv_obj_set_style_text_line_space(config_text_panel_, 5, 0);
    lv_label_set_long_mode(config_text_panel_, LV_LABEL_LONG_WRAP);

    // 右侧二维码区
    lv_obj_t* right_container = lv_obj_create(config_container_);
    lv_obj_remove_style_all(right_container); // 清除默认样式
    lv_obj_set_size(right_container, 140, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right_container, LV_FLEX_FLOW_COLUMN); // 垂直布局
    lv_obj_set_style_pad_gap(right_container, 5, 0); // 元素间距5像素
    lv_obj_set_style_flex_main_place(right_container, LV_FLEX_ALIGN_CENTER, 0); // 主轴居中

    qrcode_label_ = lv_label_create(right_container);
    lv_label_set_text(qrcode_label_, "");
    lv_obj_set_style_text_font(qrcode_label_, fonts_.text_font, 0);
    lv_obj_set_style_text_line_space(qrcode_label_, 2, 0);
    lv_obj_set_style_text_align(qrcode_label_, LV_TEXT_ALIGN_CENTER, 0);

    config_qrcode_panel_ = lv_qrcode_create(right_container);
    lv_qrcode_set_size(config_qrcode_panel_, 120);
    lv_qrcode_set_dark_color(config_qrcode_panel_, lv_color_black());
    lv_qrcode_set_light_color(config_qrcode_panel_, lv_color_white());

    lv_obj_add_flag(config_container_, LV_OBJ_FLAG_HIDDEN);

    // 对话区
    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9); // 限制宽度为屏幕宽度的 90%
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // 设置为自动换行模式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐
}

void LcdDisplay::SetChatMessage(const std::string &role, const std::string &content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    lv_label_set_text(chat_message_label_, content.c_str());
}

void LcdDisplay::SetEmotion(const std::string &emotion) {
    struct Emotion {
        const char* icon;
        const char* text;
    };

    static const std::vector<Emotion> emotions = {
        {"😶", "neutral"},
        {"🙂", "happy"},
        {"😆", "laughing"},
        {"😂", "funny"},
        {"😔", "sad"},
        {"😠", "angry"},
        {"😭", "crying"},
        {"😍", "loving"},
        {"😳", "embarrassed"},
        {"😯", "surprised"},
        {"😱", "shocked"},
        {"🤔", "thinking"},
        {"😉", "winking"},
        {"😎", "cool"},
        {"😌", "relaxed"},
        {"🤤", "delicious"},
        {"😘", "kissy"},
        {"😏", "confident"},
        {"😴", "sleepy"},
        {"😜", "silly"},
        {"🙄", "confused"}
    };
    
    // 查找匹配的表情
    auto it = std::find_if(emotions.begin(), emotions.end(),
        [&emotion](const Emotion& e) { return e.text == emotion; });

    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }

    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    lv_obj_set_style_text_font(emotion_label_, fonts_.emoji_font, 0);
    if (it != emotions.end()) {
        lv_label_set_text(emotion_label_, it->icon);
    } else {
        lv_label_set_text(emotion_label_, "😶");
    }
}

void LcdDisplay::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, icon);
}
void LcdDisplay::lv_chat_page() {
     // 隐藏配置页面元素
    lv_obj_add_flag(config_container_, LV_OBJ_FLAG_HIDDEN);
    lv_page_index = PageIndex::PAGE_CHAT;
    // 显示表情标签和聊天消息标签
    lv_obj_clear_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
}

void LcdDisplay::lv_config_page() {
    // 隐藏表情标签和聊天消息标签
    lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
    lv_page_index = PageIndex::PAGE_CONFIG;
    // 显示配置页面元素
    lv_obj_clear_flag(config_container_, LV_OBJ_FLAG_HIDDEN);
}

void LcdDisplay::lv_switch_page() {
    if (lv_page_index == PageIndex::PAGE_CHAT) {
        lv_config_page();
    } else {
        lv_chat_page();
    }
}

void LcdDisplay::SetConfigPage(const std::string& config_text, 
                              const std::string& qrcode_label_text,
                              const std::string& qrcode_content) {
    DisplayLockGuard lock(this);
    if (config_text_panel_) {
        lv_label_set_text(config_text_panel_, config_text.c_str());
    }
    if (qrcode_label_) {
        lv_label_set_text(qrcode_label_, qrcode_label_text.c_str());
    }
    if (config_qrcode_panel_) {
        lv_qrcode_update(config_qrcode_panel_, qrcode_content.c_str(), qrcode_content.length());
    }
}

void LcdDisplay::lv_smartconfig_page(const std::string& qrcode_content) {
    DisplayLockGuard lock(this);
    // 隐藏原页面元素
    lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);

    // 创建智能配置二维码
    smartconfig_qrcode_ = lv_qrcode_create(content_);
    lv_qrcode_set_size(smartconfig_qrcode_, 120);
    lv_qrcode_set_dark_color(smartconfig_qrcode_, lv_color_black());
    lv_qrcode_set_light_color(smartconfig_qrcode_, lv_color_white());
    lv_qrcode_update(smartconfig_qrcode_, qrcode_content.c_str(), qrcode_content.length());
}
