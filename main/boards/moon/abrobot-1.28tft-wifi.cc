#include "core/lv_obj_pos.h"  // 包含LVGL对象位置相关头文件
#include "font/lv_font.h"     // 包含LVGL字体相关头文件
#include "misc/lv_types.h"    // 包含LVGL类型定义头文件
#include "wifi_board.h"       // 包含WiFi板卡基类头文件
#include "audio_codecs/es8311_audio_codec.h"  // 包含ES8311音频编解码器头文件
#include "display/lcd_display.h"  // 包含LCD显示控制头文件
#include "system_reset.h"      // 包含系统重置功能头文件
#include "application.h"       // 包含应用程序管理头文件
#include "button.h"            // 包含按钮控制头文件
#include "config.h"            // 包含配置管理头文件
#include "iot/thing_manager.h"  // 包含IoT设备管理头文件
#include "lunar_calendar.h"     // 包含农历日期转换头文件

#include <esp_log.h>           // ESP日志系统
#include "i2c_device.h"        // I2C设备控制
#include <driver/i2c_master.h>  // ESP32 I2C主机驱动
#include <driver/ledc.h>        // ESP32 LED控制器驱动
#include <wifi_station.h>       // WiFi站点模式管理
#include <esp_lcd_panel_io.h>   // ESP32 LCD面板IO接口
#include <esp_lcd_panel_ops.h>  // ESP32 LCD面板操作接口
#include <esp_timer.h>          // ESP32定时器
#include "lcd_display.h"        // LCD显示控制
#include <iot_button.h>         // IoT按钮管理
#include <cstring>              // C字符串处理
#include "esp_lcd_gc9a01.h"     // GC9A01 LCD驱动
#include <font_awesome_symbols.h>  // Font Awesome图标符号
#include "assets/lang_config.h"    // 语言配置
#include <esp_http_client.h>      // ESP32 HTTP客户端
#include <cJSON.h>                // JSON解析库
#include "power_manager.h"        // 电源管理
#include "power_save_timer.h"     // 省电定时器
#include <esp_sleep.h>            // ESP32睡眠模式
#include "button.h"               // 按钮控制
#include "circular_led_strip.h"   // 环形LED条
#include "ws2812_task.h"          // WS2812 LED控制任务
#include "settings.h"             // 设置管理
#define TAG "abrobot-1.28tft-wifi"  // 日志标签

// 包含图片资源文件（豆腐动画序列）
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


// 声明使用的LVGL字体
LV_FONT_DECLARE(lunar);      // 农历字体
LV_FONT_DECLARE(time70);     // 70像素大小时间字体
LV_FONT_DECLARE(time50);     // 50像素大小时间字体
LV_FONT_DECLARE(time40);     // 40像素大小时间字体
LV_FONT_DECLARE(font_puhui_20_4);    // 普惠20像素字体
LV_FONT_DECLARE(font_awesome_20_4);  // Font Awesome 20像素图标字体
LV_FONT_DECLARE(font_awesome_30_4);  // Font Awesome 30像素图标字体
 


// 暗色主题颜色定义
#define DARK_BACKGROUND_COLOR       lv_color_hex(0)           // 深色背景色（黑色）
#define DARK_TEXT_COLOR             lv_color_white()          // 白色文本颜色
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0)           // 聊天背景色（黑色）
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37)    // 用户气泡颜色（深绿色）
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333)    // 助手气泡颜色（深灰色）
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A)    // 系统气泡颜色（中灰色）
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA)    // 系统文本颜色（浅灰色）
#define DARK_BORDER_COLOR           lv_color_hex(0)           // 边框颜色（黑色）
#define DARK_LOW_BATTERY_COLOR      lv_color_hex(0xFF0000)    // 低电量提示颜色（红色）

// 亮色主题颜色定义
#define LIGHT_BACKGROUND_COLOR       lv_color_white()          // 亮色背景色（白色）
#define LIGHT_TEXT_COLOR             lv_color_black()          // 黑色文本颜色
#define LIGHT_CHAT_BACKGROUND_COLOR  lv_color_hex(0xE0E0E0)    // 聊天背景色（浅灰色）
#define LIGHT_USER_BUBBLE_COLOR      lv_color_hex(0x95EC69)    // 用户气泡颜色（微信绿）
#define LIGHT_ASSISTANT_BUBBLE_COLOR lv_color_white()          // 助手气泡颜色（白色）
#define LIGHT_SYSTEM_BUBBLE_COLOR    lv_color_hex(0xE0E0E0)    // 系统气泡颜色（浅灰色）
#define LIGHT_SYSTEM_TEXT_COLOR      lv_color_hex(0x666666)    // 系统文本颜色（深灰色）
#define LIGHT_BORDER_COLOR           lv_color_hex(0xE0E0E0)    // 边框颜色（浅灰色）
#define LIGHT_LOW_BATTERY_COLOR      lv_color_black()          // 低电量提示颜色（黑色）

// 主题颜色结构体定义
struct ThemeColors {
    lv_color_t background;        // 背景色
    lv_color_t text;              // 文本颜色
    lv_color_t chat_background;   // 聊天背景色
    lv_color_t user_bubble;       // 用户气泡颜色
    lv_color_t assistant_bubble;  // 助手气泡颜色
    lv_color_t system_bubble;     // 系统气泡颜色
    lv_color_t system_text;       // 系统文本颜色
    lv_color_t border;            // 边框颜色
    lv_color_t low_battery;       // 低电量提示颜色
};

// 定义暗色主题颜色
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

// 定义亮色主题颜色
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
 
// 当前主题 - 基于默认配置初始化为暗色主题
static ThemeColors current_theme = DARK_THEME;


// 自定义LCD显示类，继承自SpiLcdDisplay
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    
    lv_timer_t *idle_timer_ = nullptr;  // 空闲定时器指针，用于自动切换到时钟界面
 
    lv_obj_t * tab1 = nullptr;          // 第一个标签页（主界面）
    lv_obj_t * tab2 = nullptr;          // 第二个标签页（时钟界面）
    lv_obj_t * tabview_ = nullptr;      // 标签视图组件
    lv_obj_t * bg_img = nullptr;        // 背景图像对象
    lv_obj_t * bg_img2 = nullptr;       // 第二背景图像对象
    uint8_t bg_index = 1;               // 当前背景索引（1-4）
    lv_obj_t * bg_switch_btn = nullptr; // 切换背景的按钮
 
    // 构造函数：初始化LCD显示
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
                        .text_font = &font_puhui_20_4,       // 设置文本字体
                        .icon_font = &font_awesome_20_4,     // 设置图标字体
                        // 不再使用表情符号字体
                    }) {
        DisplayLockGuard lock(this);  // 获取显示锁，防止多线程访问冲突
        SetupUI();                    // 设置用户界面
    }

    // 设置空闲状态方法，控制是否启用空闲定时器
    void SetIdle(bool status) override 
    {
        // 如果status为false，直接返回（禁用空闲定时器）
        if (status == false)
        {
            if (idle_timer_ != nullptr) {
                lv_timer_del(idle_timer_);  // 删除现有定时器
                idle_timer_ = nullptr;
            }
            return;
        } 

        // 如果已存在定时器，先删除它
        if (idle_timer_ != nullptr) {
            lv_timer_del(idle_timer_);
            idle_timer_ = nullptr;
        }

        // 创建一个定时器，15秒后切换到时钟页面（tab2）
        idle_timer_ = lv_timer_create([](lv_timer_t * t) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_timer_get_user_data(t);
            if (!display) return;
            
            // 查找tabview并切换到tab2
            lv_obj_t *tabview = lv_obj_get_parent(lv_obj_get_parent(display->tab2));
            if (tabview) {
                ws2812_set_mode(WS2812_MODE_OFF);  // 关闭WS2812 LED灯
                // 在切换标签页前加锁，防止异常
                lv_lock();
                lv_tabview_set_act(tabview, 1, LV_ANIM_OFF);  // 切换到tab2（索引1）
                
                // 确保时钟页面始终在最顶层
                lv_obj_move_foreground(display->tab2);
                
                // 如果有画布，将其移到background以确保不会遮挡时钟
                if (display->GetCanvas() != nullptr) {
                    lv_obj_move_background(display->GetCanvas());
                }
                
                lv_unlock();  // 解锁LVGL
            }
            
            // 完成后删除定时器
            lv_timer_del(t);
            display->idle_timer_ = nullptr;
        }, 15000, this);  // 15000ms = 15秒
    }

    // 设置聊天消息内容
    void SetChatMessage(const char* role, const char* content) override{
        DisplayLockGuard lock(this);  // 获取显示锁
        if (chat_message_label_ == nullptr) {
            return;  // 如果聊天消息标签不存在，直接返回
        }
        lv_label_set_text(chat_message_label_, content);  // 设置消息文本
        lv_obj_scroll_to_view_recursive(chat_message_label_, LV_ANIM_OFF);  // 滚动到可见区域
    }

    // 设置第一个标签页（主界面）
    void SetupTab1() {
        DisplayLockGuard lock(this);
         
        // 设置tab1的基本样式
        lv_obj_set_style_text_font(tab1, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab1, current_theme.text, 0);
        lv_obj_set_style_bg_color(tab1, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(tab1, LV_OPA_0, 0);  // 完全透明背景

        /* 创建容器 */
        container_ = lv_obj_create(tab1);
        lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(container_, LV_OPA_0, 0);  // 完全透明背景
        lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(container_, -13, -13);
        lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(container_, 0, 0);
        lv_obj_set_style_border_width(container_, 0, 0);
        
        // 确保容器在前台，这样图片会显示在其后面
        lv_obj_move_foreground(container_);

        /* 状态栏 */
        status_bar_ = lv_obj_create(container_);
        lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height);
        lv_obj_set_style_radius(status_bar_, 0, 0);
        lv_obj_set_style_bg_color(status_bar_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(status_bar_, LV_OPA_0, 0);  // 透明背景
        lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
        
        /* 内容区域 - 使用半透明背景 */
        content_ = lv_obj_create(container_);
        lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_radius(content_, 0, 0);
        lv_obj_set_width(content_, LV_HOR_RES);
        lv_obj_set_height(content_, LV_SIZE_CONTENT);  // 仅适应内容，不覆盖整个屏幕
        lv_obj_set_style_pad_all(content_, 5, 0);
        lv_obj_set_style_bg_color(content_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(content_, LV_OPA_0, 0);  // 完全透明
        lv_obj_set_style_border_width(content_, 0, 0);   // 无边框

        lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

        // 创建聊天消息标签 - 可以使用半透明背景
        chat_message_label_ = lv_label_create(content_);
        lv_label_set_text(chat_message_label_, "");
        lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9);
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);
        lv_obj_set_style_bg_opa(chat_message_label_, LV_OPA_0, 0);  // 完全透明背景

        /* 配置状态栏 */
        lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);  // 设置水平布局
        lv_obj_set_style_pad_all(status_bar_, 0, 0);  // 设置内边距为0
        lv_obj_set_style_border_width(status_bar_, 0, 0);  // 设置边框宽度为0
        lv_obj_set_style_pad_column(status_bar_, 0, 0);  // 设置列间距为0
        lv_obj_set_style_pad_left(status_bar_, 2, 0);  // 设置左内边距为2
        lv_obj_set_style_pad_right(status_bar_, 2, 0);  // 设置右内边距为2
#if 0
        // 网络状态标签（当前被注释）
        network_label_ = lv_label_create(status_bar_);
        lv_label_set_text(network_label_, "");
        lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
        lv_obj_set_style_text_color(network_label_, current_theme.text, 0);
#endif
        // 通知标签
        notification_label_ = lv_label_create(status_bar_);
        lv_obj_set_flex_grow(notification_label_, 1);  // 设置弹性增长系数为1
        lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);  // 设置文本居中对齐
        lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);  // 设置文本颜色
        lv_label_set_text(notification_label_, "");  // 初始化为空文本
        lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);  // 隐藏通知标签

        // 状态标签
        status_label_ = lv_label_create(status_bar_);
        lv_obj_set_flex_grow(status_label_, 1);  // 设置弹性增长系数为1
        lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);  // 设置循环滚动模式
        lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);  // 设置文本居中对齐
        lv_obj_set_style_text_color(status_label_, current_theme.text, 0);  // 设置文本颜色
        lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);  // 设置初始化文本
        
        // 静音标签
        mute_label_ = lv_label_create(status_bar_);
        lv_label_set_text(mute_label_, "");  // 初始化为空文本
        lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);  // 设置图标字体
        lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);  // 设置文本颜色

        // 电池标签
        battery_label_ = lv_label_create(status_bar_);
        lv_label_set_text(battery_label_, "");  // 初始化为空文本
        lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);  // 设置图标字体
        lv_obj_set_style_text_color(battery_label_, current_theme.text, 0);  // 设置文本颜色

        // 低电量弹窗
        low_battery_popup_ = lv_obj_create(tab1);
        lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);  // 关闭滚动条
        lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);  // 设置弹窗大小
        lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);  // 底部居中对齐
        lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);  // 设置背景颜色
        lv_obj_set_style_radius(low_battery_popup_, 10, 0);  // 设置圆角半径为10
        
        // 低电量提示文本
        lv_obj_t* low_battery_label = lv_label_create(low_battery_popup_);
        lv_label_set_text(low_battery_label, Lang::Strings::BATTERY_NEED_CHARGE);  // 设置电池需要充电提示
        lv_obj_set_style_text_color(low_battery_label, lv_color_white(), 0);  // 设置文本为白色
        lv_obj_center(low_battery_label);  // 居中对齐
        lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);  // 初始隐藏弹窗
    }

    // 设置第二个标签页（时钟界面）
    void SetupTab2() {
        lv_obj_set_style_text_font(tab2, fonts_.text_font, 0);  // 设置标签页文本字体
        lv_obj_set_style_text_color(tab2, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_obj_set_style_bg_color(tab2, lv_color_black(), 0);  // 设置背景颜色为黑色
        lv_obj_set_style_bg_opa(tab2, LV_OPA_COVER, 0);  // 设置背景不透明度为100%

        // 创建秒钟标签，使用time40字体
        lv_obj_t *second_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(second_label, &time40, 0);  // 设置40像素时间字体
        lv_obj_set_style_text_color(second_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_obj_align(second_label, LV_ALIGN_TOP_MID, 0, 10);  // 顶部居中对齐，偏移10像素
        lv_label_set_text(second_label, "00");  // 初始显示"00"秒
        
        // 创建日期标签
        lv_obj_t *date_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(date_label, fonts_.text_font, 0);  // 设置文本字体
        lv_obj_set_style_text_color(date_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_label_set_text(date_label, "01-01");  // 初始显示"01-01"日期
        lv_obj_align(date_label, LV_ALIGN_TOP_MID, -60, 35);  // 顶部居中对齐，向左偏移60像素，向下偏移35像素
        
        // 创建星期标签
        lv_obj_t *weekday_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(weekday_label, fonts_.text_font, 0);  // 设置文本字体
        lv_obj_set_style_text_color(weekday_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_label_set_text(weekday_label, "星期一");  // 初始显示"星期一"
        lv_obj_align(weekday_label, LV_ALIGN_TOP_MID, 60, 35);  // 顶部居中对齐，向右偏移60像素，向下偏移35像素
       
        // 创建一个容器用于放置时间标签
        lv_obj_t *time_container = lv_obj_create(tab2);
        // 设置容器的样式
        lv_obj_remove_style_all(time_container);  // 移除所有默认样式
        lv_obj_set_size(time_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // 大小根据内容自适应
        lv_obj_set_style_pad_all(time_container, 0, 0);  // 无内边距
        lv_obj_set_style_bg_opa(time_container, LV_OPA_TRANSP, 0);  // 透明背景
        lv_obj_set_style_border_width(time_container, 0, 0);  // 无边框

        // 设置为水平Flex布局
        lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_ROW);  // 水平布局
        lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 居中对齐
        
        // 设置容器位置为屏幕中央
        lv_obj_align(time_container, LV_ALIGN_CENTER, 0, 0);
        
        // 创建小时标签
        lv_obj_t *hour_label = lv_label_create(time_container);
        lv_obj_set_style_text_font(hour_label, &time70, 0);  // 设置70像素时间字体
        lv_obj_set_style_text_color(hour_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_label_set_text(hour_label, "00 :");  // 初始显示"00 :"小时
        
        // 创建分钟标签，使用橙色显示
        lv_obj_t *minute_label = lv_label_create(time_container);
        lv_obj_set_style_text_font(minute_label, &time70, 0);  // 设置70像素时间字体
        lv_obj_set_style_text_color(minute_label, lv_color_hex(0xFFA500), 0);  // 设置文本颜色为橙色
        lv_label_set_text(minute_label, " 00");  // 初始显示" 00"分钟
        
        // 创建农历标签
        lv_obj_t *lunar_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(lunar_label, &lunar, 0);  // 设置农历字体
        lv_obj_set_style_text_color(lunar_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_obj_set_width(lunar_label, LV_HOR_RES * 0.8);  // 设置宽度为屏幕宽度的80%
        lv_label_set_long_mode(lunar_label, LV_LABEL_LONG_WRAP);  // 设置自动换行模式
        lv_obj_set_style_text_align(lunar_label, LV_TEXT_ALIGN_CENTER, 0);  // 设置文本居中对齐
        lv_label_set_text(lunar_label, "农历癸卯年正月初一");  // 初始显示农历文本
        lv_obj_align(lunar_label, LV_ALIGN_BOTTOM_MID, 0, -36);  // 底部居中对齐，向上偏移36像素
        
        // 定时器更新时间 - 存储静态引用以在回调中使用
        static lv_obj_t* hour_lbl = hour_label;
        static lv_obj_t* minute_lbl = minute_label;
        static lv_obj_t* second_lbl = second_label;
        static lv_obj_t* date_lbl = date_label;
        //static lv_obj_t* year_lbl = year_label;
        static lv_obj_t* weekday_lbl = weekday_label;
        static lv_obj_t* lunar_lbl = lunar_label;
        
        // 创建定时器每秒更新时间
        lv_timer_create([](lv_timer_t *t) {
            
            // 检查标签是否有效
            if (!hour_lbl || !minute_lbl || !second_lbl || 
                !date_lbl || !weekday_lbl || !lunar_lbl) return;
            
            lv_lock();  // 获取LVGL锁
            // 获取当前时间
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            
            // 格式化时、分、秒
            char hour_str[6];
            char minute_str[3];
            char second_str[3];
            
            sprintf(hour_str, "%02d : ", timeinfo.tm_hour);  // 格式化小时，保持两位数
            sprintf(minute_str, "%02d", timeinfo.tm_min);    // 格式化分钟，保持两位数
            sprintf(second_str, "%02d", timeinfo.tm_sec);    // 格式化秒钟，保持两位数
            
            // 更新时间标签
            lv_label_set_text(hour_lbl, hour_str);    // 更新小时
            lv_label_set_text(minute_lbl, minute_str); // 更新分钟
            lv_label_set_text(second_lbl, second_str); // 更新秒钟
            
            // 格式化年份
            char year_str[12];
            snprintf(year_str, sizeof(year_str), "%d", timeinfo.tm_year + 1900);  // 年份从1900年开始计算
            
            // 更新年份标签（当前被注释）
            //lv_label_set_text(year_lbl, year_str);
            
            // 格式化日期为 MM/DD
            char date_str[25];
            snprintf(date_str, sizeof(date_str), "%d/%d", timeinfo.tm_mon + 1, timeinfo.tm_mday);  // 月份从0开始计算
            
            // 获取中文星期
            const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
            
            // 更新日期和星期标签
            lv_label_set_text(date_lbl, date_str);  // 更新日期
            
            // 检查星期是否在有效范围内
            if (timeinfo.tm_wday >= 0 && timeinfo.tm_wday < 7) {
                lv_label_set_text(weekday_lbl, weekdays[timeinfo.tm_wday]);  // 更新星期
            }
            
            // 计算并更新农历日期
            std::string lunar_date = LunarCalendar::GetLunarDate(
                timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday
            );
            lv_label_set_text(lunar_lbl, lunar_date.c_str());  // 更新农历日期
            
            lv_unlock();  // 释放LVGL锁
            
        }, 1000, NULL);  // 每1000毫
    }

    // 设置用户界面，初始化所有UI组件
    virtual void SetupUI() override {
        DisplayLockGuard lock(this);  // 获取显示锁以防止多线程访问冲突
        Settings settings("display", false);  // 加载显示设置，不自动创建
        current_theme_name_ = settings.GetString("theme", "dark");  // 获取主题名称，默认为暗色
        if (current_theme_name_ == "dark" || current_theme_name_ == "DARK") {
            current_theme = DARK_THEME;  // 设置暗色主题
        } else if (current_theme_name_ == "light" || current_theme_name_ == "LIGHT") {
            current_theme = LIGHT_THEME;  // 设置亮色主题
        }  
        ESP_LOGI(TAG, "SetupUI --------------------------------------");  // 日志输出
        
        // 创建tabview，填充整个屏幕
        lv_obj_t * screen = lv_screen_active();  // 获取当前活动屏幕
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);  // 设置屏幕背景为黑色
        tabview_ = lv_tabview_create(lv_scr_act());  // 创建标签视图
        lv_obj_set_size(tabview_, lv_pct(100), lv_pct(100));  // 设置尺寸为100%占满屏幕

        // 隐藏标签栏
        lv_tabview_set_tab_bar_position(tabview_, LV_DIR_TOP);  // 设置标签栏位置在顶部
        lv_tabview_set_tab_bar_size(tabview_, 0);  // 设置标签栏大小为0（隐藏）
        lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview_);  // 获取标签按钮
        lv_obj_add_flag(tab_btns, LV_OBJ_FLAG_HIDDEN);  // 隐藏标签按钮

        // 设置tabview的滚动捕捉模式，确保滑动后停留在固定位置
        lv_obj_t * content = lv_tabview_get_content(tabview_);  // 获取内容区域
        lv_obj_set_scroll_snap_x(content, LV_SCROLL_SNAP_CENTER);  // 设置水平滚动捕捉为中心
        
        // 创建两个页面
        tab1 = lv_tabview_add_tab(tabview_, "Tab1");  // 添加第一个标签页（主界面）
        tab2 = lv_tabview_add_tab(tabview_, "Tab2");  // 添加第二个标签页（时钟界面）

        // 禁用tab1的滚动功能
        lv_obj_clear_flag(tab1, LV_OBJ_FLAG_SCROLLABLE);
        // 隐藏tab1的滚动条
        lv_obj_set_scrollbar_mode(tab1, LV_SCROLLBAR_MODE_OFF);
        
        // 禁用tab2的滚动功能
        lv_obj_clear_flag(tab2, LV_OBJ_FLAG_SCROLLABLE);
        // 隐藏tab2的滚动条
        lv_obj_set_scrollbar_mode(tab2, LV_SCROLLBAR_MODE_OFF);

        // 为两个标签页添加点击事件处理
        lv_obj_add_event_cb(tab1, [](lv_event_t *e) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
            if (!display) return;
            
            // 确保主页画布在最顶层
            if (display->GetCanvas() != nullptr) {
                lv_obj_move_foreground(display->GetCanvas());
            }
            
            // 如果有活动的定时器，删除它
            if (display->idle_timer_ != nullptr) {
                lv_timer_del(display->idle_timer_);
                display->idle_timer_ = nullptr;
            }
        }, LV_EVENT_CLICKED, this);  // 设置tab1点击事件回调

        lv_obj_add_event_cb(tab2, [](lv_event_t *e) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
            if (!display) return;
            
            // 确保时钟页面始终在最顶层
            lv_obj_move_foreground(display->tab2);
            
            // 如果有画布，将其移到background以确保不会遮挡时钟
            if (display->GetCanvas() != nullptr) {
                lv_obj_move_background(display->GetCanvas());
            }
            
            // 如果有活动的定时器，删除它
            if (display->idle_timer_ != nullptr) {
                lv_timer_del(display->idle_timer_);
                display->idle_timer_ = nullptr;
            }
        }, LV_EVENT_CLICKED, this);  // 设置tab2点击事件回调

        // 初始化两个标签页的内容
        SetupTab1();  // 设置第一个标签页
        SetupTab2();  // 设置第二个标签页
    }

    // 设置界面主题
    virtual void SetTheme(const std::string& theme_name) override {
        DisplayLockGuard lock(this);  // 获取显示锁

        current_theme = DARK_THEME;  // 默认设为暗色主题

        if (theme_name == "dark" || theme_name == "DARK") {
            current_theme = DARK_THEME;  // 设置暗色主题
        } else if (theme_name == "light" || theme_name == "LIGHT") {
            current_theme = LIGHT_THEME;  // 设置亮色主题
        } else {
            // 无效的主题名称，记录错误
            ESP_LOGE(TAG, "Invalid theme name: %s", theme_name.c_str());
            return;
        }
        
        // 获取当前活动屏幕
        lv_obj_t* screen = lv_screen_active();
        
        // 更新屏幕颜色 - 添加透明度设置
        lv_obj_set_style_bg_color(screen, current_theme.background, 0);  // 设置背景颜色
        lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, 0);  // 设置背景完全透明
        lv_obj_set_style_text_color(screen, current_theme.text, 0);      // 设置文本颜色
        
        // 更新容器颜色 - 添加透明度设置
        if (container_ != nullptr) {
            lv_obj_set_style_bg_color(container_, current_theme.background, 0);  // 设置背景颜色
            lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);  // 设置背景完全透明
            lv_obj_set_style_border_color(container_, current_theme.border, 0);  // 设置边框颜色
        }
        
        // 更新状态栏颜色 - 添加透明度设置
        if (status_bar_ != nullptr) {
            lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);  // 设置背景颜色
            lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);  // 设置背景完全透明
            lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);      // 设置文本颜色
            
            // 更新状态栏元素
            if (network_label_ != nullptr) {
                lv_obj_set_style_text_color(network_label_, current_theme.text, 0);  // 设置网络标签文本颜色
            }
            if (status_label_ != nullptr) {
                lv_obj_set_style_text_color(status_label_, current_theme.text, 0);  // 设置状态标签文本颜色
            }
            if (notification_label_ != nullptr) {
                lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);  // 设置通知标签文本颜色
            }
            if (mute_label_ != nullptr) {
                lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);  // 设置静音标签文本颜色
            }
            if (battery_label_ != nullptr) {
                lv_obj_set_style_text_color(battery_label_, current_theme.text, 0);  // 设置电池标签文本颜色
            }
        }
        
        // 更新内容区域颜色
        if (content_ != nullptr) {
            lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);  // 设置背景颜色
            lv_obj_set_style_border_color(content_, current_theme.border, 0);       // 设置边框颜色
            
            // 简单UI模式 - 只更新主聊天消息
            if (chat_message_label_ != nullptr) {
                lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);  // 设置文本颜色
            }
        }
        
        // 更新低电量弹窗
        if (low_battery_popup_ != nullptr) {
            lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);  // 设置背景颜色
        }

        // 无错误发生，保存主题到设置
        current_theme_name_ = theme_name;  // 更新当前主题名称
        Settings settings("display", true);  // 打开设置，自动创建
        settings.SetString("theme", theme_name);  // 保存主题设置
    }
    
 
private:
    // 静态回调函数
 
};


// 自定义板卡类，继承自WifiBoard
class CustomBoard : public WifiBoard {
private:

    i2c_master_bus_handle_t codec_i2c_bus_;  // 编解码器I2C总线句柄
    CustomLcdDisplay* display_;              // LCD显示对象指针
    Button boot_btn;                         // 启动按钮
 
    esp_lcd_panel_io_handle_t panel_io = nullptr;  // LCD面板IO句柄
    esp_lcd_panel_handle_t panel = nullptr;        // LCD面板句柄

    // 图片显示任务句柄
    TaskHandle_t image_task_handle_ = nullptr;

    // 初始化编解码器I2C总线
    void InitializeCodecI2c() {
        // 初始化I2C外设
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,                    // 使用I2C0端口
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,    // SDA引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,    // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,        // 默认时钟源
            .glitch_ignore_cnt = 7,                   // 抗干扰计数
            .intr_priority = 0,                       // 中断优先级
            .trans_queue_depth = 0,                   // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,          // 启用内部上拉电阻
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 创建I2C主机总线
    }
 
    // 初始化SPI总线
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;        // MOSI引脚
        buscfg.miso_io_num = GPIO_NUM_NC;             // 不使用MISO
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;         // SCLK引脚
        buscfg.quadwp_io_num = GPIO_NUM_NC;           // 不使用QSPI WP
        buscfg.quadhd_io_num = GPIO_NUM_NC;           // 不使用QSPI HD
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);  // 最大传输大小
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));    // 初始化SPI总线
    }

    // 初始化LCD显示器
    void InitializeLcdDisplay() {
        
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;         // 片选引脚
        io_config.dc_gpio_num = DISPLAY_DC_PIN;         // 数据/命令选择引脚
        io_config.spi_mode = DISPLAY_SPI_MODE;          // SPI模式
        io_config.pclk_hz = 40 * 1000 * 1000;           // 时钟频率40MHz
        io_config.trans_queue_depth = 10;               // 传输队列深度
        io_config.lcd_cmd_bits = 8;                     // 命令位数
        io_config.lcd_param_bits = 8;                   // 参数位数
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));  // 创建SPI面板IO

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;    // 复位引脚
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;   // RGB元素顺序
        panel_config.bits_per_pixel = 16;                 // 每像素位数
 
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));  // 创建GC9A01面板
   
        esp_lcd_panel_reset(panel);  // 重置面板
 
        esp_lcd_panel_init(panel);  // 初始化面板
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);  // 是否反转颜色
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);            // 是否交换XY坐标
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);  // 设置镜像
        
        // 创建自定义LCD显示对象
        display_ = new CustomLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }
    
    // 自定义按钮初始化
    void InitializeButtonsCustom() {
        gpio_reset_pin(BOOT_BUTTON_GPIO);                // 重置启动按钮引脚                     
        gpio_set_direction(BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);   // 设置为输入模式
    }

    // 按钮初始化
    void InitializeButtons() {
        boot_btn.OnClick([this]() {
            auto& app = Application::GetInstance();
            // 如果设备正在启动且WiFi未连接，则重置WiFi配置
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 重置WiFi配置
            }
            app.ToggleChatState();  // 切换聊天状态
            
        });
 
    }

    // 初始化IoT设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));      // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Screen"));       // 添加屏幕设备
        thing_manager.AddThing(iot::CreateThing("ColorStrip"));   // 添加彩色灯带设备
        thing_manager.AddThing(iot::CreateThing("RotateDisplay")); // 添加旋转显示设备
    }

    // 启动图片循环显示任务
    void StartImageSlideshow() {
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 4096, this, 3, &image_task_handle_);  // 创建图片轮播任务
        ESP_LOGI(TAG, "图片循环显示任务已启动");  // 输出日志
    }

    // 图片循环显示任务实现
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
        
        // 获取CustomLcdDisplay实例
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display);
        
        // 设置图片显示参数
        int imgWidth = 240;   // 图片宽度
        int imgHeight = 240;  // 图片高度
        
        // 创建一个图像容器，放在tab1上
        lv_obj_t* img_container = nullptr;
        
        {
            DisplayLockGuard lock(display);
            
            // 创建图像容器，直接放在tab1上而不是内容区域
            img_container = lv_obj_create(customDisplay->tab1);
            
            // 清除所有默认样式，确保完全自定义
            lv_obj_remove_style_all(img_container);
            
            // 设置容器大小与屏幕匹配
            lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
            
            // 设置容器位置在屏幕中央
            lv_obj_center(img_container);
            
            // 设置无边框和完全透明的背景
            lv_obj_set_style_border_width(img_container, 0, 0);
            lv_obj_set_style_bg_opa(img_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(img_container, 0, 0);
            
            // 确保此容器在所有其他元素之下
            lv_obj_move_background(img_container);
            
            // 创建图像对象
            lv_obj_t* img_obj = lv_img_create(img_container);
            
            // 设置图像位置在容器中央
            lv_obj_center(img_obj);
            
            // 确保图像显示在正确的层级上
            lv_obj_move_foreground(img_obj);
        }
        
        // 设置图片数组 - 包含所有动画帧
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
        
        // 创建图像描述符
        lv_image_dsc_t img_dsc = {
            .header = {
                .magic = LV_IMAGE_HEADER_MAGIC,
                .cf = LV_COLOR_FORMAT_RGB565,
                .flags = 0,
                .w = (uint32_t)imgWidth,
                .h = (uint32_t)imgHeight,
                .stride = (uint32_t)(imgWidth * 2),
                .reserved_2 = 0,
            },
            .data_size = (uint32_t)(imgWidth * imgHeight * 2),
            .data = (const uint8_t*)convertedData,
            .reserved = NULL
        };
        
        // 先显示第一张图片
        int currentIndex = 0;
        const uint8_t* currentImage = imageArray[currentIndex];
        
        // 转换并显示第一张图片
        for (int i = 0; i < imgWidth * imgHeight; i++) {
            uint16_t pixel = ((uint16_t*)currentImage)[i];
            convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
        }
        
        {
            DisplayLockGuard lock(display);
            // 获取图像对象并设置图像源
            lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
            img_dsc.data = (const uint8_t*)convertedData;
            lv_img_set_src(img_obj, &img_dsc);
            
            // 确保图像容器和图像对象在正确的层级
            lv_obj_move_to_index(img_container, 0);  // 移到最下层
            lv_obj_move_foreground(img_obj);         // 图像对象在前景
            
            // 检查并确认UI层级正确性
            lv_obj_t* tab_content = lv_obj_get_parent(customDisplay->tab1);
            if (tab_content) {
                lv_obj_clear_flag(tab_content, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
            }
        }
        
        ESP_LOGI(TAG, "初始显示图片");
        
        // 持续监控和处理图片显示
        TickType_t lastUpdateTime = xTaskGetTickCount();  // 记录上次更新时间
        const TickType_t cycleInterval = pdMS_TO_TICKS(120);  // 图片切换间隔120毫秒
        
        // 定义用于判断是否正在播放音频的变量
        bool isAudioPlaying = false;       // 当前是否播放音频
        bool wasAudioPlaying = false;      // 上次是否播放音频
        DeviceState previousState = app.GetDeviceState();  // 上次设备状态
        bool pendingAnimationStart = false;  // 是否有待启动动画
        TickType_t stateChangeTime = 0;      // 状态变化时间点
        
        while (true) {
            // 获取当前设备状态
            DeviceState currentState = app.GetDeviceState();
            TickType_t currentTime = xTaskGetTickCount();
            
            // 检查当前是否在时钟页面（tab2）
            bool isClockTabActive = false;
            if (customDisplay && customDisplay->tabview_) {
                int active_tab = lv_tabview_get_tab_act(customDisplay->tabview_);
                isClockTabActive = (active_tab == 1);
            }
            
            // 如果时钟页面处于活跃状态，隐藏图像容器
            if (isClockTabActive) {
                DisplayLockGuard lock(display);
                if (img_container) {
                    lv_obj_add_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            } else {
                // 在主界面，显示图像容器并确保在正确位置
                DisplayLockGuard lock(display);
                if (img_container) {
                    lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_align(img_container, LV_ALIGN_CENTER, 0, 0);
                    lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
                    lv_obj_move_to_index(img_container, 0);  // 确保在底层
                    
                    // 确保图像对象在容器中居中
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj) {
                        lv_obj_center(img_obj);
                        lv_obj_move_foreground(img_obj);
                    }
                }
            }
            
            // 检测到状态刚变为Speaking，设置动画延迟启动
            if (currentState == kDeviceStateSpeaking && previousState != kDeviceStateSpeaking) {
                pendingAnimationStart = true;  // 标记待启动动画
                stateChangeTime = currentTime;  // 记录状态变化时间
                ESP_LOGI(TAG, "检测到音频状态改变，准备启动动画");  // 输出日志
            }
            
            // 延迟启动动画，等待音频实际开始播放
            // 设置1200ms延迟，实验找到最佳匹配值
            if (pendingAnimationStart && (currentTime - stateChangeTime >= pdMS_TO_TICKS(1200))) {
                // 状态变为Speaking后延迟一段时间，开始动画以匹配实际音频输出
                currentIndex = 1;  // 从第二张图片开始
                currentImage = imageArray[currentIndex];  // 获取图片数据
                
                // 转换并显示新图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);  // 字节序转换
                }
                
                {
                    DisplayLockGuard lock(display);  // 获取显示锁
                    // 更新图像源
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj) {
                        img_dsc.data = (const uint8_t*)convertedData;
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                }
                
                ESP_LOGI(TAG, "开始播放动画，与音频同步");  // 输出日志
                
                lastUpdateTime = currentTime;  // 更新上次更新时间
                isAudioPlaying = true;         // 设置为音频播放状态
                pendingAnimationStart = false;  // 清除待启动标记
            }
            
            // 正常的图片循环逻辑
            isAudioPlaying = (currentState == kDeviceStateSpeaking);  // 更新音频播放状态
            
            if (isAudioPlaying && !pendingAnimationStart && (currentTime - lastUpdateTime >= cycleInterval)) {
                // 更新索引到下一张图片
                currentIndex = (currentIndex + 1) % totalImages;
                currentImage = imageArray[currentIndex];  // 获取下一帧图片数据
                
                // 转换并显示新图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);  // 字节序转换
                }
                
                {
                    DisplayLockGuard lock(display);  // 获取显示锁
                    // 更新图像源
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj) {
                        img_dsc.data = (const uint8_t*)convertedData;
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                }
                
                // 更新上次更新时间
                lastUpdateTime = currentTime;
            }
            else if ((!isAudioPlaying && wasAudioPlaying) || (!isAudioPlaying && currentIndex != 0)) {
                // 切换回第一张图片（静止状态）
                currentIndex = 0;
                currentImage = imageArray[currentIndex];  // 获取第一帧图片数据
                
                // 转换并显示第一张图片
                for (int i = 0; i < imgWidth * imgHeight; i++) {
                    uint16_t pixel = ((uint16_t*)currentImage)[i];
                    convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);  // 字节序转换
                }
                
                {
                    DisplayLockGuard lock(display);  // 获取显示锁
                    // 更新图像源
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj) {
                        img_dsc.data = (const uint8_t*)convertedData;
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                }
                
                ESP_LOGI(TAG, "返回显示初始图片");  // 输出日志
                pendingAnimationStart = false;  // 清除待启动标记
            }
            
            // 更新状态记录
            wasAudioPlaying = isAudioPlaying;  // 更新上次音频播放状态
            previousState = currentState;      // 更新上次设备状态
            
            // 保持较高的检测频率以确保响应灵敏
            vTaskDelay(pdMS_TO_TICKS(10));  // 延时10ms
        }
        
        // 释放资源
        delete[] convertedData;
        vTaskDelete(NULL);
    }

public:
    // 构造函数
    CustomBoard() : boot_btn(BOOT_BUTTON_GPIO){
        InitializeCodecI2c();        // 初始化编解码器I2C总线
        InitializeSpi();             // 初始化SPI总线
        InitializeLcdDisplay();      // 初始化LCD显示器
        InitializeButtons();         // 初始化按钮
        InitializeIot();             // 初始化IoT设备
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
        
        // 启动图片循环显示任务
        StartImageSlideshow();
    }

    // 获取LED对象
    virtual Led* GetLed() override {
        static CircularLedStrip led(BUILTIN_LED_GPIO);  // 创建环形LED灯带对象
        return &led;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);  // 创建ES8311音频编解码器对象
        return &audio_codec;
    }

    // 获取显示器对象
    virtual Display* GetDisplay() override {
        return display_;  // 返回LCD显示对象
    }
    
    // 获取背光控制对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 创建PWM背光控制对象
        return &backlight;
    }

    // 析构函数
    ~CustomBoard() {
        // 如果任务在运行中，停止它
        if (image_task_handle_ != nullptr) {
            vTaskDelete(image_task_handle_);  // 删除图片显示任务
            image_task_handle_ = nullptr;
        }
    }
};

// 声明自定义板卡类为当前使用的板卡
DECLARE_BOARD(CustomBoard);
