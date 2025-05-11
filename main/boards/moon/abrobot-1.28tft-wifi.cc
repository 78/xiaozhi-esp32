#include "core/lv_obj_pos.h"
#include "font/lv_font.h"
#include "misc/lv_types.h"
#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "lunar_calendar.h"

#include <esp_log.h>
#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <wifi_station.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include "lcd_display.h"
#include <iot_button.h>
#include <cstring>
#include "esp_lcd_gc9a01.h"
#include <font_awesome_symbols.h>
#include "assets/lang_config.h"
#include <esp_http_client.h>
#include <cJSON.h>
#include "power_manager.h"
#include "power_save_timer.h"
#include <esp_sleep.h>
#include "button.h"
#include "circular_led_strip.h"
#include "ws2812_task.h"
#include "settings.h"
#define TAG "abrobot-1.28tft-wifi"
#include "images/doufu/output_0001.h"
#include "images/doufu/output_0002.h"
#include "images/doufu/output_0003.h"
#include "images/doufu/output_0004.h"
#include "images/doufu/output_0005.h"
#include "images/doufu/output_0006.h"
#include "images/doufu/output_0007.h" 
#include "images/doufu/output_0008.h"
#include "images/doufu/output_0009.h"
#include "images/doufu/output_0010.h"
#include "images/doufu/output_0011.h"
#include "images/doufu/output_0012.h"
#include "images/doufu/output_0013.h"
#include "images/doufu/output_0014.h"
#include "images/doufu/output_0015.h"
#include "images/doufu/output_0016.h"
#include "images/doufu/output_0017.h"



LV_FONT_DECLARE(lunar);
LV_FONT_DECLARE(time70);
LV_FONT_DECLARE(time50);
LV_FONT_DECLARE(time40);
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);
LV_FONT_DECLARE(font_awesome_30_4);
 



// Color definitions for dark theme
#define DARK_BACKGROUND_COLOR       lv_color_hex(0)     // Dark background
#define DARK_TEXT_COLOR             lv_color_white()           // White text
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0)     // Slightly lighter than background
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37)     // Dark green
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333)     // Dark gray
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A)     // Medium gray
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA)     // Light gray text
#define DARK_BORDER_COLOR           lv_color_hex(0)     // Dark gray border
#define DARK_LOW_BATTERY_COLOR      lv_color_hex(0xFF0000)     // Red for dark mode

// Color definitions for light theme
#define LIGHT_BACKGROUND_COLOR       lv_color_white()           // White background
#define LIGHT_TEXT_COLOR             lv_color_black()           // Black text
#define LIGHT_CHAT_BACKGROUND_COLOR  lv_color_hex(0xE0E0E0)     // Light gray background
#define LIGHT_USER_BUBBLE_COLOR      lv_color_hex(0x95EC69)     // WeChat green
#define LIGHT_ASSISTANT_BUBBLE_COLOR lv_color_white()           // White
#define LIGHT_SYSTEM_BUBBLE_COLOR    lv_color_hex(0xE0E0E0)     // Light gray
#define LIGHT_SYSTEM_TEXT_COLOR      lv_color_hex(0x666666)     // Dark gray text
#define LIGHT_BORDER_COLOR           lv_color_hex(0xE0E0E0)     // Light gray border
#define LIGHT_LOW_BATTERY_COLOR      lv_color_black()           // Black for light mode

// Theme color structure
struct ThemeColors {
    lv_color_t background;
    lv_color_t text;
    lv_color_t chat_background;
    lv_color_t user_bubble;
    lv_color_t assistant_bubble;
    lv_color_t system_bubble;
    lv_color_t system_text;
    lv_color_t border;
    lv_color_t low_battery;
};

// Define dark theme colors
static const ThemeColors DARK_THEME = {
    .background = DARK_BACKGROUND_COLOR,
    .text = DARK_TEXT_COLOR,
    .chat_background = DARK_CHAT_BACKGROUND_COLOR,
    .user_bubble = DARK_USER_BUBBLE_COLOR,
    .assistant_bubble = DARK_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = DARK_SYSTEM_BUBBLE_COLOR,
    .system_text = DARK_SYSTEM_TEXT_COLOR,
    .border = DARK_BORDER_COLOR,
    .low_battery = DARK_LOW_BATTERY_COLOR
};

// Define light theme colors
static const ThemeColors LIGHT_THEME = {
    .background = LIGHT_BACKGROUND_COLOR,
    .text = LIGHT_TEXT_COLOR,
    .chat_background = LIGHT_CHAT_BACKGROUND_COLOR,
    .user_bubble = LIGHT_USER_BUBBLE_COLOR,
    .assistant_bubble = LIGHT_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = LIGHT_SYSTEM_BUBBLE_COLOR,
    .system_text = LIGHT_SYSTEM_TEXT_COLOR,
    .border = LIGHT_BORDER_COLOR,
    .low_battery = LIGHT_LOW_BATTERY_COLOR
};
 
// Current theme - initialize based on default config
static ThemeColors current_theme = DARK_THEME;


class CustomLcdDisplay : public SpiLcdDisplay {
public:
    
    lv_timer_t *idle_timer_ = nullptr;  // Add timer pointer member variable
 
    lv_obj_t * tab1 = nullptr;
    lv_obj_t * tab2 = nullptr;
    lv_obj_t * tabview_ = nullptr;
    lv_obj_t * bg_img = nullptr;  // Background image object
    lv_obj_t * bg_img2 = nullptr;  // Background image object
    uint8_t bg_index = 1;         // Current background index (1-4)
    lv_obj_t * bg_switch_btn = nullptr;  // Button to switch backgrounds
 
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, 
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy) 
        : SpiLcdDisplay(io_handle, panel_handle,
                    width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    {
                        .text_font = &font_puhui_20_4,
                        .icon_font = &font_awesome_20_4,
                        .emoji_font = font_emoji_64_init(),
                    }) {
        DisplayLockGuard lock(this);
        SetupUI();
    }

    void SetIdle(bool status) override 
    {
        // If status is false, just return
        if (status == false)
        {
            if (idle_timer_ != nullptr) {
                lv_timer_del(idle_timer_);
                idle_timer_ = nullptr;
            }
            return;
        } 

        // If there's an existing timer, delete it first
        if (idle_timer_ != nullptr) {
            lv_timer_del(idle_timer_);
            idle_timer_ = nullptr;
        }

        // Create a timer to switch to tab2 after 15 seconds
        idle_timer_ = lv_timer_create([](lv_timer_t * t) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_timer_get_user_data(t);
            if (!display) return;
            
            // Find the tabview and switch to tab2
            lv_obj_t *tabview = lv_obj_get_parent(lv_obj_get_parent(display->tab2));
            if (tabview) {
                ws2812_set_mode(WS2812_MODE_OFF);
                // 在切换标签页前加锁，防止异常
                lv_lock();
                lv_tabview_set_act(tabview, 1, LV_ANIM_OFF);
                
                // 确保时钟页面始终在最顶层
                lv_obj_move_foreground(display->tab2);
                
                // 如果有画布，将其移到background以确保不会遮挡时钟
                if (display->GetCanvas() != nullptr) {
                    lv_obj_move_background(display->GetCanvas());
                }
                
                lv_unlock();
            }
            
            // Delete the timer after it's done
            lv_timer_del(t);
            display->idle_timer_ = nullptr;
        }, 15000, this);  // 15000ms = 15 seconds
    }
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
void SetupTab1() {
 
    lv_obj_set_style_text_font(tab1, fonts_.text_font, 0);
    lv_obj_set_style_text_color(tab1, current_theme.text, 0);
    lv_obj_set_style_bg_color(tab1, current_theme.background, 0);

    /* Background image for tab1 */
    bg_img = lv_img_create(tab1);
    
    /* Set the image descriptor */
    lv_img_set_src(bg_img, &bg1);
    
    /* Position the image to cover the entire tab */
    lv_obj_set_size(bg_img, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(bg_img, -16, -16);
    
    /* Send to background */
    lv_obj_move_background(bg_img);
    
    /* Container */
    container_ = lv_obj_create(tab1);
    // 将container_高度改为原来的四分之三，宽度保持不变
    lv_obj_set_size(container_, LV_HOR_RES * 0.7, LV_VER_RES * 0.7 * 0.75);
    // 将container_放置在屏幕下方
    lv_obj_align(container_, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, current_theme.background, 0);
    lv_obj_set_style_border_color(container_, current_theme.border, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
    
    /* Create right-side toggle button */
    container_toggle_btn = lv_btn_create(tab1);
    lv_obj_set_size(container_toggle_btn, 60, 60);  // 50% larger (40->60)
    // 位置调整为垂直居中
    lv_obj_align(container_toggle_btn, LV_ALIGN_RIGHT_MID, -2, 0);  // 右侧中间位置
    lv_obj_set_style_bg_opa(container_toggle_btn, LV_OPA_50, 0);    // Make semi-transparent
    lv_obj_set_style_radius(container_toggle_btn, 30, 0);           // Round corners (increased to match new size)
    
    lv_obj_t *toggle_btn_label = lv_label_create(container_toggle_btn);
    lv_label_set_text(toggle_btn_label, FONT_AWESOME_XMARK);      // Use X mark icon
    lv_obj_set_style_text_font(toggle_btn_label, fonts_.icon_font, 0);
    lv_obj_center(toggle_btn_label);
    
    // Add click event handler for the toggle button
    lv_obj_add_event_cb(container_toggle_btn, [](lv_event_t *e) {
        CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
        if (!display) return;
        
        // Toggle container visibility
        display->container_visible = !display->container_visible;
        
        // Update container visibility
        if (display->container_visible) {
            lv_obj_clear_flag(display->container_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(lv_obj_get_child(display->container_toggle_btn, 0), FONT_AWESOME_XMARK);
        } else {
            lv_obj_add_flag(display->container_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(lv_obj_get_child(display->container_toggle_btn, 0), FONT_AWESOME_COMMENT);
        }
    }, LV_EVENT_CLICKED, this);
    
    /* Create background switch button */
    bg_switch_btn = lv_btn_create(tab1);
    lv_obj_set_size(bg_switch_btn, 60, 60);  // 50% larger (40->60)
    // 位置调整为垂直居中
    lv_obj_align(bg_switch_btn, LV_ALIGN_LEFT_MID, 2, 0);  // 左侧中间位置
    lv_obj_set_style_bg_opa(bg_switch_btn, LV_OPA_30, 0);    // Make semi-transparent
    lv_obj_set_style_radius(bg_switch_btn, 30, 0);           // Round corners (increased to match new size)
    
    lv_obj_t *btn_label = lv_label_create(bg_switch_btn);
    lv_label_set_text(btn_label, FONT_AWESOME_ARROW_RIGHT);  // Use arrow icon
    lv_obj_set_style_text_font(btn_label, fonts_.icon_font, 0);
    lv_obj_center(btn_label);
    
    // Add click event handler for the button
    lv_obj_add_event_cb(bg_switch_btn, [](lv_event_t *e) {
        CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
        if (!display) return;
        
        // Cycle through backgrounds
        display->bg_index = (display->bg_index % 4) + 1;
        
        // Set the new background image
        switch (display->bg_index) {
            case 1: lv_img_set_src(display->bg_img, &bg1); break;
            case 2: lv_img_set_src(display->bg_img, &bg2); break;
            case 3: lv_img_set_src(display->bg_img, &bg3); break;
            case 4: lv_img_set_src(display->bg_img, &bg4); break;
        }
    }, LV_EVENT_CLICKED, this);
    
    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES * 0.6, fonts_.emoji_font->line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);
    lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
    //lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
    // Set status bar background to semi-transparent
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_50, 0);

    /* Content - Chat area */
    content_ = lv_obj_create(container_);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES * 0.6);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_pad_all(content_, 5, 0);
    lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0); // Background for chat area
    lv_obj_set_style_border_color(content_, current_theme.border, 0); // Border color for chat area

    // Enable scrolling for chat content
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(content_, LV_DIR_VER);
    
    // Create a flex container for chat messages
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content_, 10, 0); // Space between messages

    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);

    // We'll create chat messages dynamically in SetChatMessage
    chat_message_label_ = nullptr;

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 2, 0);
    lv_obj_set_style_pad_right(status_bar_, 2, 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    // 设置状态栏的内容垂直居中
    lv_obj_set_flex_align(status_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 创建emotion_label_在状态栏最左侧
    emotion_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_obj_set_style_text_color(emotion_label_, current_theme.text, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_set_style_margin_right(emotion_label_, 5, 0); // 添加右边距，与后面的元素分隔

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, current_theme.text, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    
    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(network_label_, current_theme.text, 0);
    lv_obj_set_style_margin_left(network_label_, 5, 0); // 添加左边距，与前面的元素分隔

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, current_theme.text, 0);
    lv_obj_set_style_margin_left(battery_label_, 5, 0); // 添加左边距，与前面的元素分隔

    low_battery_popup_ = lv_obj_create(tab1);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.54, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    lv_obj_t* low_battery_label = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label, lv_color_white(), 0);
    lv_obj_center(low_battery_label);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

#define  MAX_MESSAGES 50
void SetChatMessage(const char* role, const char* content) override{
    DisplayLockGuard lock(this);
    if (content_ == nullptr) {
        return;
    }
    
    //避免出现空的消息框
    if(strlen(content) == 0) return;
    
    // Create a message bubble
    lv_obj_t* msg_bubble = lv_obj_create(content_);
    lv_obj_set_style_radius(msg_bubble, 8, 0);
    lv_obj_set_scrollbar_mode(msg_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(msg_bubble, 1, 0);
    lv_obj_set_style_border_color(msg_bubble, current_theme.border, 0);
    lv_obj_set_style_pad_all(msg_bubble, 8, 0);

    // Create the message text
    lv_obj_t* msg_text = lv_label_create(msg_bubble);
    lv_label_set_text(msg_text, content);
    
    // 计算文本实际宽度
    lv_coord_t text_width = lv_txt_get_width(content, strlen(content), fonts_.text_font, 0);

    // 计算气泡宽度
    lv_coord_t max_width = LV_HOR_RES * 0.6 * 85 / 100 - 16;  // 屏幕宽度的85%
    lv_coord_t min_width = 20;  
    lv_coord_t bubble_width;
    
    // 确保文本宽度不小于最小宽度
    if (text_width < min_width) {
        text_width = min_width;
    }

    // 如果文本宽度小于最大宽度，使用文本宽度
    if (text_width < max_width) {
        bubble_width = text_width; 
    } else {
        bubble_width = max_width;
    }
    
    // 设置消息文本的宽度
    lv_obj_set_width(msg_text, bubble_width);  // 减去padding
    lv_label_set_long_mode(msg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(msg_text, fonts_.text_font, 0);

    // 设置气泡宽度
    lv_obj_set_width(msg_bubble, bubble_width);
    lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(msg_bubble, LV_OPA_50, 0);
    // Set alignment and style based on message role
    if (strcmp(role, "user") == 0) {
        // User messages are right-aligned with green background
        lv_obj_set_style_bg_color(msg_bubble, current_theme.user_bubble, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, current_theme.text, 0);
        
        // 设置自定义属性标记气泡类型
        lv_obj_set_user_data(msg_bubble, (void*)"user");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Add some margin 
        lv_obj_set_style_margin_right(msg_bubble, 10, 0);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    } else if (strcmp(role, "assistant") == 0) {
        // Assistant messages are left-aligned with white background
        lv_obj_set_style_bg_color(msg_bubble, current_theme.assistant_bubble, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, current_theme.text, 0);
        
        // 设置自定义属性标记气泡类型
        lv_obj_set_user_data(msg_bubble, (void*)"assistant");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Add some margin
        lv_obj_set_style_margin_left(msg_bubble, -4, 0);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    } else if (strcmp(role, "system") == 0) {
        // System messages are center-aligned with light gray background
        lv_obj_set_style_bg_color(msg_bubble, current_theme.system_bubble, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, current_theme.system_text, 0);
        
        // 设置自定义属性标记气泡类型
        lv_obj_set_user_data(msg_bubble, (void*)"system");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    }
    
    // Create a full-width container for user messages to ensure right alignment
    if (strcmp(role, "user") == 0) {
        // Create a full-width container
        lv_obj_t* container = lv_obj_create(content_);
        lv_obj_set_width(container, LV_HOR_RES * 0.6);
        lv_obj_set_height(container, LV_SIZE_CONTENT);
        
        // Make container transparent and borderless
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        
        // Move the message bubble into this container
        lv_obj_set_parent(msg_bubble, container);
        
        // Right align the bubble in the container
        lv_obj_align(msg_bubble, LV_ALIGN_RIGHT_MID, -10, 0);
        
        // Auto-scroll to this container
        lv_obj_scroll_to_view_recursive(container, LV_ANIM_OFF);
    } else if (strcmp(role, "system") == 0) {
        // 为系统消息创建全宽容器以确保居中对齐
        lv_obj_t* container = lv_obj_create(content_);
        lv_obj_set_width(container, LV_HOR_RES * 0.6);
        lv_obj_set_height(container, LV_SIZE_CONTENT);
        
        // 使容器透明且无边框
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        
        // 将消息气泡移入此容器
        lv_obj_set_parent(msg_bubble, container);
        
        // 将气泡居中对齐在容器中
        lv_obj_align(msg_bubble, LV_ALIGN_CENTER, 0, 0);
        
        // 自动滚动底部
        lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
    } else {
        // For assistant messages
        // Left align assistant messages
        lv_obj_align(msg_bubble, LV_ALIGN_LEFT_MID, 0, 0);

        // Auto-scroll to the message bubble
        lv_obj_scroll_to_view_recursive(msg_bubble, LV_ANIM_ON);
    }
    
    // Store reference to the latest message label
    chat_message_label_ = msg_text;

    // 检查消息数量是否超过限制
    uint32_t msg_count = lv_obj_get_child_cnt(content_);
    while (msg_count >= MAX_MESSAGES) {
        // 删除最早的消息（第一个子节点）
        lv_obj_t* oldest_msg = lv_obj_get_child(content_, 0);
        if (oldest_msg != nullptr) {
            lv_obj_del(oldest_msg);
            msg_count--;
        }else{
            break;
        }
    }
}
#else

void SetChatMessage(const char* role, const char* content) override{
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    lv_label_set_text(chat_message_label_, content);
    lv_obj_scroll_to_view_recursive(chat_message_label_, LV_ANIM_OFF);
}

void SetupTab1() {
    DisplayLockGuard lock(this);

     
    lv_obj_set_style_text_font(tab1, fonts_.text_font, 0);
    lv_obj_set_style_text_color(tab1, current_theme.text, 0);
    lv_obj_set_style_bg_color(tab1, current_theme.background, 0);

    /* Container */
    container_ = lv_obj_create(tab1);
    lv_obj_set_style_bg_color(container_, current_theme.background, 0);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    //lv_obj_center(container_);
    lv_obj_set_pos(container_, -13, -13);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);
    lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
    
    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_pad_all(content_, 5, 0);
    lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);
    lv_obj_set_style_border_color(content_, current_theme.border, 0); // Border color for content

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // 垂直布局（从上到下）
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY); // 子对象居中对齐，等距分布

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_obj_set_style_text_color(emotion_label_, current_theme.text, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9); // 限制宽度为屏幕宽度的 90%
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // 设置为自动换行模式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐
    lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 2, 0);
    lv_obj_set_style_pad_right(status_bar_, 2, 0);
#if 0
    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(network_label_, current_theme.text, 0);
#endif
    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, current_theme.text, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, current_theme.text, 0);

    low_battery_popup_ = lv_obj_create(tab1);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    lv_obj_t* low_battery_label = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label, lv_color_white(), 0);
    lv_obj_center(low_battery_label);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}
#endif
 

    void SetupTab2() {
        lv_obj_set_style_text_font(tab2, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab2, lv_color_white(), 0);
        lv_obj_set_style_bg_color(tab2, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(tab2, LV_OPA_COVER, 0);  // Set background opacity to 100%


        // 创建秒钟标签，使用time30字体
        lv_obj_t *second_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(second_label, &time40, 0);
        lv_obj_set_style_text_color(second_label, lv_color_white(), 0);
        lv_obj_align(second_label, LV_ALIGN_TOP_MID, 0, 10);    
        lv_label_set_text(second_label, "00");
        
        // Create date label directly on tab2
        lv_obj_t *date_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(date_label, fonts_.text_font, 0);
        lv_obj_set_style_text_color(date_label, lv_color_white(), 0);
        lv_label_set_text(date_label, "01-01");
        lv_obj_align(date_label, LV_ALIGN_TOP_MID, -60, 35);
        
        // Create weekday label directly on tab2
        lv_obj_t *weekday_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(weekday_label, fonts_.text_font, 0);
        lv_obj_set_style_text_color(weekday_label, lv_color_white(), 0);
        lv_label_set_text(weekday_label, "星期一");
        lv_obj_align(weekday_label, LV_ALIGN_TOP_MID, 60, 35);
       
        // 创建一个容器用于放置时间标签
        lv_obj_t *time_container = lv_obj_create(tab2);
        // 设置容器的样式
        lv_obj_remove_style_all(time_container);  // 移除所有默认样式
        lv_obj_set_size(time_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT); // 大小根据内容调整
        lv_obj_set_style_pad_all(time_container, 0, 0); // 无内边距
        lv_obj_set_style_bg_opa(time_container, LV_OPA_TRANSP, 0); // 透明背景
        lv_obj_set_style_border_width(time_container, 0, 0); // 无边框


        
        // 设置为水平Flex布局
        lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // 设置容器位置为屏幕中央
        lv_obj_align(time_container, LV_ALIGN_CENTER, 0, 0);
        
        // 创建小时标签
        lv_obj_t *hour_label = lv_label_create(time_container);
        lv_obj_set_style_text_font(hour_label, &time70, 0);
        lv_obj_set_style_text_color(hour_label, lv_color_white(), 0);
        lv_label_set_text(hour_label, "00 :");
        
        // 创建分钟标签，使用橙色显示
        lv_obj_t *minute_label = lv_label_create(time_container);
        lv_obj_set_style_text_font(minute_label, &time70, 0);
        lv_obj_set_style_text_color(minute_label, lv_color_hex(0xFFA500), 0); // 橙色
        lv_label_set_text(minute_label, " 00");
        
        
        // Create lunar label directly on tab2
        lv_obj_t *lunar_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(lunar_label, &lunar, 0);
        lv_obj_set_style_text_color(lunar_label, lv_color_white(), 0);
        lv_obj_set_width(lunar_label, LV_HOR_RES * 0.8);
        lv_label_set_long_mode(lunar_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(lunar_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lunar_label, "农历癸卯年正月初一");
        lv_obj_align(lunar_label, LV_ALIGN_BOTTOM_MID, 0, -36);
        
        // 定时器更新时间
        static lv_obj_t* hour_lbl = hour_label;
        static lv_obj_t* minute_lbl = minute_label;
        static lv_obj_t* second_lbl = second_label;
        static lv_obj_t* date_lbl = date_label;
        //static lv_obj_t* year_lbl = year_label;
        static lv_obj_t* weekday_lbl = weekday_label;
        static lv_obj_t* lunar_lbl = lunar_label;
        
        lv_timer_create([](lv_timer_t *t) {
            
            if (!hour_lbl || !minute_lbl || !second_lbl || 
                !date_lbl || !weekday_lbl || !lunar_lbl) return;
            
            lv_lock();
            // Get current time
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            
            // 格式化时、分、秒
            char hour_str[6];
            char minute_str[3];
            char second_str[3];
            
            sprintf(hour_str, "%02d : ", timeinfo.tm_hour);
            sprintf(minute_str, "%02d", timeinfo.tm_min);
            sprintf(second_str, "%02d", timeinfo.tm_sec);
            
            // 更新时间标签
            lv_label_set_text(hour_lbl, hour_str);
            lv_label_set_text(minute_lbl, minute_str);
            lv_label_set_text(second_lbl, second_str);
            
            // Format year
            char year_str[12];
            snprintf(year_str, sizeof(year_str), "%d", timeinfo.tm_year + 1900);
            
            // Update year label
            //lv_label_set_text(year_lbl, year_str);
            
            // Format date as MM-DD
            char date_str[25];
            snprintf(date_str, sizeof(date_str), "%d/%d", timeinfo.tm_mon + 1, timeinfo.tm_mday);
            
            // Get day of week in Chinese
            const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
            
            // Update date and weekday labels
            lv_label_set_text(date_lbl, date_str);
            
            if (timeinfo.tm_wday >= 0 && timeinfo.tm_wday < 7) {
                lv_label_set_text(weekday_lbl, weekdays[timeinfo.tm_wday]);
            }
            
            // 计算并更新农历日期
            std::string lunar_date = LunarCalendar::GetLunarDate(
                timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday
            );
            lv_label_set_text(lunar_lbl, lunar_date.c_str());
            
            lv_unlock();
            
        }, 1000, NULL);
    }

    virtual void SetupUI() override {
        DisplayLockGuard lock(this);
        Settings settings("display", false);
        current_theme_name_ = settings.GetString("theme", "dark");
        if (current_theme_name_ == "dark" || current_theme_name_ == "DARK") {
            current_theme = DARK_THEME;
        } else if (current_theme_name_ == "light" || current_theme_name_ == "LIGHT") {
            current_theme = LIGHT_THEME;
        }  
        ESP_LOGI(TAG, "SetupUI --------------------------------------");
        
        // 创建tabview，填充整个屏幕
        lv_obj_t * screen = lv_screen_active();
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
        tabview_ = lv_tabview_create(lv_scr_act());
        lv_obj_set_size(tabview_, lv_pct(100), lv_pct(100));

        
        // 隐藏标签栏
        lv_tabview_set_tab_bar_position(tabview_, LV_DIR_TOP);
        lv_tabview_set_tab_bar_size(tabview_, 0);
        lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview_);
        lv_obj_add_flag(tab_btns, LV_OBJ_FLAG_HIDDEN);

         // 设置tabview的滚动捕捉模式，确保滑动后停留在固定位置
        lv_obj_t * content = lv_tabview_get_content(tabview_);
        lv_obj_set_scroll_snap_x(content, LV_SCROLL_SNAP_CENTER);
        
        // 创建两个页面
        tab1 = lv_tabview_add_tab(tabview_, "Tab1");
        tab2 = lv_tabview_add_tab(tabview_, "Tab2");

        // Disable scrolling for tab1
        lv_obj_clear_flag(tab1, LV_OBJ_FLAG_SCROLLABLE);
        // Hide scrollbar for tab1
        lv_obj_set_scrollbar_mode(tab1, LV_SCROLLBAR_MODE_OFF);
        
        // Disable scrolling for tab2
        lv_obj_clear_flag(tab2, LV_OBJ_FLAG_SCROLLABLE);
        // Hide scrollbar for tab2
        lv_obj_set_scrollbar_mode(tab2, LV_SCROLLBAR_MODE_OFF);

        // Add click event handlers for both tabs
        lv_obj_add_event_cb(tab1, [](lv_event_t *e) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
            if (!display) return;
            
            // 确保主页画布在最顶层
            if (display->GetCanvas() != nullptr) {
                lv_obj_move_foreground(display->GetCanvas());
            }
            
            // If there's an active timer, delete it
            if (display->idle_timer_ != nullptr) {
                lv_timer_del(display->idle_timer_);
                display->idle_timer_ = nullptr;
            }
        }, LV_EVENT_CLICKED, this);

        lv_obj_add_event_cb(tab2, [](lv_event_t *e) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
            if (!display) return;
            
            // 确保时钟页面始终在最顶层
            lv_obj_move_foreground(display->tab2);
            
            // 如果有画布，将其移到background以确保不会遮挡时钟
            if (display->GetCanvas() != nullptr) {
                lv_obj_move_background(display->GetCanvas());
            }
            
            // If there's an active timer, delete it
            if (display->idle_timer_ != nullptr) {
                lv_timer_del(display->idle_timer_);
                display->idle_timer_ = nullptr;
            }
        }, LV_EVENT_CLICKED, this);

        // 在第一个页面中保持原有内容
        SetupTab1();
        SetupTab2();
    }

    virtual void SetTheme(const std::string& theme_name) override {
    DisplayLockGuard lock(this);

    current_theme = DARK_THEME;

    if (theme_name == "dark" || theme_name == "DARK") {
        current_theme = DARK_THEME;
    } else if (theme_name == "light" || theme_name == "LIGHT") {
        current_theme = LIGHT_THEME;
    } else {
        // Invalid theme name, return false
        ESP_LOGE(TAG, "Invalid theme name: %s", theme_name.c_str());
        return;
    }
    
    
    // Get the active screen
    lv_obj_t* screen = lv_screen_active();
    
    // Update the screen colors
    lv_obj_set_style_bg_color(screen, current_theme.background, 0);
    lv_obj_set_style_text_color(screen, current_theme.text, 0);
    
    // Update container colors
    if (container_ != nullptr) {
        lv_obj_set_style_bg_color(container_, current_theme.background, 0);
        lv_obj_set_style_border_color(container_, current_theme.border, 0);
    }
    
    // Update status bar colors
    if (status_bar_ != nullptr) {
        lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);
        lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
        
        // Update status bar elements
        if (network_label_ != nullptr) {
            lv_obj_set_style_text_color(network_label_, current_theme.text, 0);
        }
        if (status_label_ != nullptr) {
            lv_obj_set_style_text_color(status_label_, current_theme.text, 0);
        }
        if (notification_label_ != nullptr) {
            lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);
        }
        if (mute_label_ != nullptr) {
            lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);
        }
        if (battery_label_ != nullptr) {
            lv_obj_set_style_text_color(battery_label_, current_theme.text, 0);
        }
        if (emotion_label_ != nullptr) {
            lv_obj_set_style_text_color(emotion_label_, current_theme.text, 0);
        }
    }
    
    // Update content area colors
    if (content_ != nullptr) {
        lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);
        lv_obj_set_style_border_color(content_, current_theme.border, 0);
        
        // If we have the chat message style, update all message bubbles
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
        // Iterate through all children of content (message containers or bubbles)
        uint32_t child_count = lv_obj_get_child_cnt(content_);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* obj = lv_obj_get_child(content_, i);
            if (obj == nullptr) continue;
            
            lv_obj_t* bubble = nullptr;
            
            // 检查这个对象是容器还是气泡
            // 如果是容器（用户或系统消息），则获取其子对象作为气泡
            // 如果是气泡（助手消息），则直接使用
            if (lv_obj_get_child_cnt(obj) > 0) {
                // 可能是容器，检查它是否为用户或系统消息容器
                // 用户和系统消息容器是透明的
                lv_opa_t bg_opa = lv_obj_get_style_bg_opa(obj, 0);
                if (bg_opa == LV_OPA_TRANSP) {
                    // 这是用户或系统消息的容器
                    bubble = lv_obj_get_child(obj, 0);
                } else {
                    // 这可能是助手消息的气泡自身
                    bubble = obj;
                }
            } else {
                // 没有子元素，可能是其他UI元素，跳过
                continue;
            }
            
            if (bubble == nullptr) continue;
            
            // 使用保存的用户数据来识别气泡类型
            void* bubble_type_ptr = lv_obj_get_user_data(bubble);
            if (bubble_type_ptr != nullptr) {
                const char* bubble_type = static_cast<const char*>(bubble_type_ptr);
                
                // 根据气泡类型应用正确的颜色
                if (strcmp(bubble_type, "user") == 0) {
                    lv_obj_set_style_bg_color(bubble, current_theme.user_bubble, 0);
                } else if (strcmp(bubble_type, "assistant") == 0) {
                    lv_obj_set_style_bg_color(bubble, current_theme.assistant_bubble, 0); 
                } else if (strcmp(bubble_type, "system") == 0) {
                    lv_obj_set_style_bg_color(bubble, current_theme.system_bubble, 0);
                }
                
                // Update border color
                lv_obj_set_style_border_color(bubble, current_theme.border, 0);
                
                // Update text color for the message
                if (lv_obj_get_child_cnt(bubble) > 0) {
                    lv_obj_t* text = lv_obj_get_child(bubble, 0);
                    if (text != nullptr) {
                        // 根据气泡类型设置文本颜色
                        if (strcmp(bubble_type, "system") == 0) {
                            lv_obj_set_style_text_color(text, current_theme.system_text, 0);
                        } else {
                            lv_obj_set_style_text_color(text, current_theme.text, 0);
                        }
                    }
                }
            } else {
                // 如果没有标记，回退到之前的逻辑（颜色比较）
                // ...保留原有的回退逻辑...
                lv_color_t bg_color = lv_obj_get_style_bg_color(bubble, 0);
            
                // 改进bubble类型检测逻辑，不仅使用颜色比较
                bool is_user_bubble = false;
                bool is_assistant_bubble = false;
                bool is_system_bubble = false;
            
                // 检查用户bubble
                if (lv_color_eq(bg_color, DARK_USER_BUBBLE_COLOR) || 
                    lv_color_eq(bg_color, LIGHT_USER_BUBBLE_COLOR) ||
                    lv_color_eq(bg_color, current_theme.user_bubble)) {
                    is_user_bubble = true;
                }
                // 检查系统bubble
                else if (lv_color_eq(bg_color, DARK_SYSTEM_BUBBLE_COLOR) || 
                         lv_color_eq(bg_color, LIGHT_SYSTEM_BUBBLE_COLOR) ||
                         lv_color_eq(bg_color, current_theme.system_bubble)) {
                    is_system_bubble = true;
                }
                // 剩余的都当作助手bubble处理
                else {
                    is_assistant_bubble = true;
                }
            
                // 根据bubble类型应用正确的颜色
                if (is_user_bubble) {
                    lv_obj_set_style_bg_color(bubble, current_theme.user_bubble, 0);
                } else if (is_assistant_bubble) {
                    lv_obj_set_style_bg_color(bubble, current_theme.assistant_bubble, 0);
                } else if (is_system_bubble) {
                    lv_obj_set_style_bg_color(bubble, current_theme.system_bubble, 0);
                }
                
                // Update border color
                lv_obj_set_style_border_color(bubble, current_theme.border, 0);
                
                // Update text color for the message
                if (lv_obj_get_child_cnt(bubble) > 0) {
                    lv_obj_t* text = lv_obj_get_child(bubble, 0);
                    if (text != nullptr) {
                        // 回退到颜色检测逻辑
                        if (lv_color_eq(bg_color, current_theme.system_bubble) ||
                            lv_color_eq(bg_color, DARK_SYSTEM_BUBBLE_COLOR) || 
                            lv_color_eq(bg_color, LIGHT_SYSTEM_BUBBLE_COLOR)) {
                            lv_obj_set_style_text_color(text, current_theme.system_text, 0);
                        } else {
                            lv_obj_set_style_text_color(text, current_theme.text, 0);
                        }
                    }
                }
            }
        }
#else
        // Simple UI mode - just update the main chat message
        if (chat_message_label_ != nullptr) {
            lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);
        }
        
        if (emotion_label_ != nullptr) {
            lv_obj_set_style_text_color(emotion_label_, current_theme.text, 0);
        }
#endif
    }
    
    // Update low battery popup
    if (low_battery_popup_ != nullptr) {
        lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);
    }

    // No errors occurred. Save theme to settings
    current_theme_name_ = theme_name;
    Settings settings("display", true);
    settings.SetString("theme", theme_name);
}
    
 
private:
    // 静态回调函数
 
};


 

class CustomBoard : public WifiBoard {
private:

    i2c_master_bus_handle_t codec_i2c_bus_;
    CustomLcdDisplay* display_;
    Button boot_btn;
 
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;

    // 添加图片显示任务句柄
    TaskHandle_t image_task_handle_ = nullptr;

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }
 

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
 
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
   
        esp_lcd_panel_reset(panel);
 

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new CustomLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }
    
 
    void InitializeButtonsCustom() {
        gpio_reset_pin(BOOT_BUTTON_GPIO);                                     
        gpio_set_direction(BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);   
    }

    void InitializeButtons() {
        boot_btn.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
            
        });
 
    }

    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("ColorStrip"));
        thing_manager.AddThing(iot::CreateThing("RotateDisplay"));
    }

    // 添加启动图片循环显示任务函数
    void StartImageSlideshow() {
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 4096, this, 3, &image_task_handle_);
        ESP_LOGI(TAG, "图片循环显示任务已启动");
    }

    // 图片循环显示任务
    static void ImageSlideshowTask(void* arg) {
        CustomBoard* board = static_cast<CustomBoard*>(arg);
        Display* display = board->GetDisplay();
        
        if (!display) {
            ESP_LOGE(TAG, "无法获取显示设备");
            vTaskDelete(NULL);
            return;
        }
        
        // 获取Application实例
        auto& app = Application::GetInstance();
        
        // 创建画布（如果不存在）
        if (!display->HasCanvas()) {
            display->CreateCanvas();
        }
        
        // 设置图片显示参数
        int imgWidth = 240;
        int imgHeight = 240;
        int x = 0;
        int y = 0;
        
        // 设置图片数组
        const uint8_t* imageArray[] = {
            gImage_output_0001,
            gImage_output_0002,
            gImage_output_0003,
            gImage_output_0004,
            gImage_output_0005,
            gImage_output_0006,
            gImage_output_0007,
            gImage_output_0008,
            gImage_output_0009,
            gImage_output_0010,
            gImage_output_0011,
            gImage_output_0012,
            gImage_output_0013,
            gImage_output_0014,
            gImage_output_0015,
            gImage_output_0016,
            gImage_output_0017,
            // gImage_output_0018,
            // gImage_output_0019,
            // gImage_output_0018,
            // gImage_output_0017,
            gImage_output_0016,
            gImage_output_0015,
            gImage_output_0014,
            gImage_output_0013,
            gImage_output_0012,
            gImage_output_0011,
            gImage_output_0010,
            gImage_output_0009,
            gImage_output_0008,
            gImage_output_0007,
            gImage_output_0006,
            gImage_output_0005,
            gImage_output_0004,
            gImage_output_0003,
            gImage_output_0002,
            gImage_output_0001,
        };
        const int totalImages = sizeof(imageArray) / sizeof(imageArray[0]);
        
        // 创建临时缓冲区用于字节序转换
        uint16_t* convertedData = new uint16_t[imgWidth * imgHeight];
        if (!convertedData) {
            ESP_LOGE(TAG, "无法分配内存进行图像转换");
            vTaskDelete(NULL);
            return;
        }
        
        // 先显示第一张图片
        int currentIndex = 0;
        const uint8_t* currentImage = imageArray[currentIndex];
        
        // 转换并显示第一张图片
        for (int i = 0; i < imgWidth * imgHeight; i++) {
            uint16_t pixel = ((uint16_t*)currentImage)[i];
            convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
        }
        
        // 使用DrawImageOnCanvas而不是DrawImage
        display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
        ESP_LOGI(TAG, "初始显示图片");
        
        // 持续监控和处理图片显示
        TickType_t lastUpdateTime = xTaskGetTickCount();
        const TickType_t cycleInterval = pdMS_TO_TICKS(120); // 图片切换间隔120毫秒
        
        // 定义用于判断是否正在播放音频的变量
        bool isAudioPlaying = false;
        bool wasAudioPlaying = false;
        DeviceState previousState = app.GetDeviceState();
        bool pendingAnimationStart = false;
        TickType_t stateChangeTime = 0;
        
        // 获取CustomLcdDisplay实例以检查当前活动的标签页
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display);
        
        while (true) {
            // 获取当前设备状态
            DeviceState currentState = app.GetDeviceState();
            TickType_t currentTime = xTaskGetTickCount();
            
            // 检查当前是否在时钟页面（tab2）
            bool isClockTabActive = false;
            if (customDisplay && customDisplay->tabview_) {
                int active_tab = lv_tabview_get_tab_act(customDisplay->tabview_);
                isClockTabActive = (active_tab == 1); // tab2是索引1
            }
            
            // 如果时钟页面处于活跃状态，不绘制任何图片
            if (isClockTabActive) {
                // 休眠一小段时间后再次检查
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            // 检测到状态刚变为Speaking，设置动画延迟启动
            if (currentState == kDeviceStateSpeaking && previousState != kDeviceStateSpeaking) {
                pendingAnimationStart = true;
                stateChangeTime = currentTime;
                ESP_LOGI(TAG, "检测到音频状态改变，准备启动动画");
            }
            
            // 延迟启动动画，等待音频实际开始播放
            // 设置800ms延迟，实验找到最佳匹配值
            if (pendingAnimationStart && (currentTime - stateChangeTime >= pdMS_TO_TICKS(1200))) {
                // 状态变为Speaking后延迟一段时间，开始动画以匹配实际音频输出
                currentIndex = 1; // 从第二张图片开始
                currentImage = imageArray[currentIndex];
                
                // 转换并显示新图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                }
                display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                ESP_LOGI(TAG, "开始播放动画，与音频同步");
                
                lastUpdateTime = currentTime;
                isAudioPlaying = true;
                pendingAnimationStart = false;
            }
            
            // 正常的图片循环逻辑
            isAudioPlaying = (currentState == kDeviceStateSpeaking);
            
            if (isAudioPlaying && !pendingAnimationStart && (currentTime - lastUpdateTime >= cycleInterval)) {
                // 更新索引到下一张图片
                currentIndex = (currentIndex + 1) % totalImages;
                currentImage = imageArray[currentIndex];
                
                // 转换并显示新图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                }
                display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                
                // 更新上次更新时间
                lastUpdateTime = currentTime;
            }
            else if ((!isAudioPlaying && wasAudioPlaying) || (!isAudioPlaying && currentIndex != 0)) {
                // 切换回第一张图片
                currentIndex = 0;
                currentImage = imageArray[currentIndex];
                
                // 转换并显示第一张图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                }
                display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                ESP_LOGI(TAG, "返回显示初始图片");
                pendingAnimationStart = false;
            }
            
            // 更新状态记录
            wasAudioPlaying = isAudioPlaying;
            previousState = currentState;
            
            // 保持较高的检测频率以确保响应灵敏
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // 释放资源
        delete[] convertedData;
        vTaskDelete(NULL);
    }

public:
    CustomBoard() : boot_btn(BOOT_BUTTON_GPIO){
        InitializeCodecI2c();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
        
        // 在构造函数最后添加启动图片循环显示任务
        StartImageSlideshow();
    }

    virtual Led* GetLed() override {
        static CircularLedStrip led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

 
 

    ~CustomBoard() {
        // 如果任务在运行中，停止它
        if (image_task_handle_ != nullptr) {
            vTaskDelete(image_task_handle_);
            image_task_handle_ = nullptr;
        }
    }
};

DECLARE_BOARD(CustomBoard);
