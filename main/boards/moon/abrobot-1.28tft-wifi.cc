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
#include "settings.h"             // 设置管理
#include "iot_image_display.h"  // 引入图片显示模式定义
#include "image_manager.h"  // 引入图片资源管理器头文件
#define TAG "abrobot-1.28tft-wifi"  // 日志标签
#define TAG "CIRCLE128S32"

// 在abrobot-1.28tft-wifi.cc文件开头添加外部声明
extern "C" {
    // 图片显示模式
    extern volatile iot::ImageDisplayMode g_image_display_mode;
    extern const unsigned char* g_static_image;  // 静态图片引用
}

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
#define DARK_TEXT_COLOR             lv_color_black()          // 白色文本颜色
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0)           // 聊天背景色（黑色）
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37)    // 用户气泡颜色（深绿色）
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333)    // 助手气泡颜色（深灰色）
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A)    // 系统气泡颜色（中灰色）
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA)    // 系统文本颜色（浅灰色）
#define DARK_BORDER_COLOR           lv_color_hex(0)           // 边框颜色（黑色）
#define DARK_LOW_BATTERY_COLOR      lv_color_hex(0xFF0000)    // 低电量提示颜色（红色）

// 亮色主题颜色定义
#define LIGHT_BACKGROUND_COLOR       lv_color_white()          // 亮色背景色（白色）
#define LIGHT_TEXT_COLOR             lv_color_white()          // 黑色文本颜色
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

// 在文件开头添加全局变量，用于安全地传递下载进度状态
static struct {
    bool pending;
    int progress;
    char message[64];
    SemaphoreHandle_t mutex;
} g_download_progress = {false, 0, "", NULL};

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
        
        // 创建一个用于保护下载进度状态的互斥锁
        if (g_download_progress.mutex == NULL) {
            g_download_progress.mutex = xSemaphoreCreateMutex();
        }
        
        // 创建定时器定期检查并更新下载进度显示
        lv_timer_create([](lv_timer_t* timer) {
            CustomLcdDisplay* display = (CustomLcdDisplay*)lv_timer_get_user_data(timer);
            if (!display) return;
            
            // 检查是否有待更新的进度
            if (g_download_progress.mutex && xSemaphoreTake(g_download_progress.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (g_download_progress.pending) {
                    int progress = g_download_progress.progress;
                    char message[64];
                    strncpy(message, g_download_progress.message, sizeof(message));
                    
                    // 重置标志
                    g_download_progress.pending = false;
                    xSemaphoreGive(g_download_progress.mutex);
                    
                    // 更新UI
                    display->UpdateDownloadProgressUI(true, progress, message);
                } else {
                    xSemaphoreGive(g_download_progress.mutex);
                }
            }
        }, 100, this); // 100ms检查一次
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

        // 如果用户交互被禁用（例如在预加载期间），不启用空闲定时器
        if (user_interaction_disabled_) {
            ESP_LOGI(TAG, "用户交互已禁用，暂不启用空闲定时器");
            return;
        }

        // 如果已存在定时器，先删除它
        if (idle_timer_ != nullptr) {
            lv_timer_del(idle_timer_);
            idle_timer_ = nullptr;
        }
        
        // 获取当前设备状态，判断是否应该启用空闲定时器
        auto& app = Application::GetInstance();
        DeviceState currentState = app.GetDeviceState();
        
        // 检查下载UI是否实际可见
        bool download_ui_is_active_and_visible = false;
        if (download_progress_container_ != nullptr &&
            !lv_obj_has_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
            download_ui_is_active_and_visible = true;
        }
        
        // 检查预加载UI是否实际可见
        bool preload_ui_is_active_and_visible = false;
        if (preload_progress_container_ != nullptr &&
            !lv_obj_has_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
            preload_ui_is_active_and_visible = true;
        }
        
        if (currentState == kDeviceStateStarting || 
            currentState == kDeviceStateWifiConfiguring ||
            download_ui_is_active_and_visible ||
            preload_ui_is_active_and_visible) { 
            ESP_LOGI(TAG, "设备处于启动/配置状态或下载/预加载UI可见，暂不启用空闲定时器");
            return;
        }
        
        // 创建一个定时器，15秒后切换到时钟页面（tab2）
        idle_timer_ = lv_timer_create([](lv_timer_t * t) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_timer_get_user_data(t);
            if (!display) return;
            
            // 再次检查当前状态，确保在切换前设备不在特殊状态
            auto& app = Application::GetInstance();
            DeviceState currentState = app.GetDeviceState();
            
            // 如果设备已进入某些特殊状态，取消切换
            if (currentState == kDeviceStateStarting || 
                currentState == kDeviceStateWifiConfiguring ||
                display->download_progress_container_ != nullptr ||
                display->preload_progress_container_ != nullptr ||
                display->user_interaction_disabled_) {
                // 删除定时器但不切换
                lv_timer_del(t);
                display->idle_timer_ = nullptr;
                return;
            }
            
            // 查找tabview并切换到tab2
            lv_obj_t *tabview = lv_obj_get_parent(lv_obj_get_parent(display->tab2));
            if (tabview) {
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
        
        // 如果当前处于WiFi配置模式，显示到时钟页面也显示提示
        if (std::string(content).find(Lang::Strings::CONNECT_TO_HOTSPOT) != std::string::npos) {
            // 在时钟页面添加配网提示
            DisplayLockGuard lock(this);
            lv_obj_t* wifi_hint = lv_label_create(tab2);
            lv_obj_set_size(wifi_hint, LV_HOR_RES * 0.8, LV_SIZE_CONTENT);
            lv_obj_align(wifi_hint, LV_ALIGN_CENTER, 0, -20);
            lv_obj_set_style_text_font(wifi_hint, fonts_.text_font, 0);
            lv_obj_set_style_text_color(wifi_hint, lv_color_hex(0xFF9500), 0); // 使用明亮的橙色
            lv_obj_set_style_text_align(wifi_hint, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(wifi_hint, "请连接热点进行WiFi配置\n设备尚未连接网络");
            lv_obj_set_style_bg_color(wifi_hint, lv_color_hex(0x222222), 0);
            lv_obj_set_style_bg_opa(wifi_hint, LV_OPA_70, 0);
            lv_obj_set_style_radius(wifi_hint, 10, 0);
            lv_obj_set_style_pad_all(wifi_hint, 10, 0);
        }
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
        lv_obj_set_style_pad_all(content_, 5, 0);
        lv_obj_set_style_bg_color(content_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(content_, LV_OPA_0, 0);  // 完全透明
        lv_obj_set_style_border_width(content_, 0, 0);   // 无边框

        // 新增：限制内容区域高度、启用纵向滚动并隐藏滚动条，防止消息过多挤出状态栏
        lv_obj_set_height(content_, LV_VER_RES - fonts_.text_font->line_height - 10); // 高度为屏幕减去状态栏
        lv_obj_set_scroll_dir(content_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);

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

        // 添加以下代码，设置上边距增加文本显示下移
        lv_obj_set_style_pad_top(chat_message_label_, 100, 0);  // 添加60像素的上边距

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
            
        }, 1000, NULL);  // 每1000毫秒更新一次

        // 创建电池状态显示 - 调整位置和样式
        lv_obj_t *battery_container = lv_obj_create(tab2);
        lv_obj_remove_style_all(battery_container);
        // 增加容器宽度，确保有足够空间容纳图标和文本
        lv_obj_set_size(battery_container, 100, 30);
        // 添加半透明背景使图标更醒目
        lv_obj_set_style_bg_opa(battery_container, LV_OPA_30, 0);
        lv_obj_set_style_bg_color(battery_container, lv_color_black(), 0);
        lv_obj_set_style_radius(battery_container, 15, 0);  // 圆角边框
        lv_obj_set_style_border_width(battery_container, 0, 0);

        // 设置为水平Flex布局，让元素自动排列
        lv_obj_set_flex_flow(battery_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(battery_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(battery_container, 8, 0);  // 设置内边距
        lv_obj_set_style_pad_column(battery_container, 5, 0);  // 设置元素间距

        // 位置居中底部，往下移动一点
        lv_obj_align(battery_container, LV_ALIGN_BOTTOM_MID, 0, -5);

        // 创建电池图标
        lv_obj_t *tab2_battery_label = lv_label_create(battery_container);
        lv_obj_set_style_text_font(tab2_battery_label, fonts_.icon_font, 0);
        // 使用更亮的颜色确保可见
        lv_obj_set_style_text_color(tab2_battery_label, lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(tab2_battery_label, FONT_AWESOME_BATTERY_FULL);  // 先设置一个初始值确保可见
        // 不再需要手动居中，Flex布局会自动处理

        // 添加电量百分比显示
        lv_obj_t *battery_percent = lv_label_create(battery_container); 
        lv_obj_set_style_text_font(battery_percent, fonts_.text_font, 0);
        lv_obj_set_style_text_color(battery_percent, lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(battery_percent, "100%");  // 初始化为可见值
        // 不再需要手动对齐，Flex布局会自动处理

        // 更新静态引用
        static lv_obj_t* batt_icon = tab2_battery_label;
        static lv_obj_t* batt_text = battery_percent;

        // 更新定时器代码，同时更新图标和百分比
        lv_timer_create([](lv_timer_t *t) {
            if (!batt_icon || !batt_text) return;
            
            auto& board = Board::GetInstance();
            int battery_level;
            bool charging, discharging;
            
            if (board.GetBatteryLevel(battery_level, charging, discharging)) {
                lv_lock();
                // 更新电池图标
                const char* icon = nullptr;
                if (charging) {
                    icon = FONT_AWESOME_BATTERY_CHARGING;
                } else {
                    const char* levels[] = {
                        FONT_AWESOME_BATTERY_EMPTY,  // 0-19%
                        FONT_AWESOME_BATTERY_1,      // 20-39%
                        FONT_AWESOME_BATTERY_2,      // 40-59%
                        FONT_AWESOME_BATTERY_3,      // 60-79%
                        FONT_AWESOME_BATTERY_FULL,   // 80-100%
                    };
                    icon = levels[battery_level / 20];
                }
                
                // 更新图标
                if (icon) {
                    lv_label_set_text(batt_icon, icon);
                }
                
                // 更新百分比文本
                char percent_str[8];
                snprintf(percent_str, sizeof(percent_str), "%d%%", battery_level);
                lv_label_set_text(batt_text, percent_str);
                
                lv_unlock();
            }
        }, 3000, NULL);  // 缩短更新间隔为3秒
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
    
    // 显示或隐藏下载进度条
    void ShowDownloadProgress(bool show, int progress = 0, const char* message = nullptr) {
        if (!show || !message) {
            return;
        }

        // 直接使用SetChatMessage显示下载进度
        DisplayLockGuard lock(this);
        if (chat_message_label_ != nullptr) {
            char full_message[256];
            if (progress > 0 && progress < 100) {
                snprintf(full_message, sizeof(full_message), 
                        "正在下载图片资源...\n%s\n进度：%d%%", 
                        message, progress);
            } else {
                snprintf(full_message, sizeof(full_message), "%s", message);
            }
            lv_label_set_text(chat_message_label_, full_message);
            lv_obj_scroll_to_view_recursive(chat_message_label_, LV_ANIM_OFF);
        }
    }
    
public:
    // 修改成员变量，删除进度条相关变量
    lv_obj_t* download_progress_container_ = nullptr;
    lv_obj_t* download_progress_label_ = nullptr; // 百分比标签
    lv_obj_t* message_label_ = nullptr;          // 状态消息标签
    
    // 添加预加载UI相关变量
    lv_obj_t* preload_progress_container_ = nullptr;
    lv_obj_t* preload_progress_label_ = nullptr;
    lv_obj_t* preload_message_label_ = nullptr;
    
    // 用户交互禁用状态标志
    bool user_interaction_disabled_ = false;
    
    // 更新预加载进度UI
    void UpdatePreloadProgressUI(bool show, int current, int total, const char* message) {
        DisplayLockGuard lock(this);
        
        // 如果容器不存在但需要显示，创建UI
        if (preload_progress_container_ == nullptr && show) {
            CreatePreloadProgressUI();
            DisableUserInteraction(); // 禁用用户交互
        }
        
        // 如果容器仍不存在，直接返回
        if (preload_progress_container_ == nullptr) {
            return;
        }
        
        if (show) {
            // 更新进度标签
            if (preload_progress_label_) {
                char progress_text[32];
                snprintf(progress_text, sizeof(progress_text), "%d/%d", current, total);
                lv_label_set_text(preload_progress_label_, progress_text);
            }
            
            // 更新消息
            if (message && preload_message_label_ != nullptr) {
                lv_label_set_text(preload_message_label_, message);
            }
            
            // 确保容器可见
            lv_obj_clear_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
            
            // 确保在最顶层显示
            lv_obj_move_foreground(preload_progress_container_);
            
            // 如果当前在时钟页面，切换回主页面
            if (tabview_) {
                uint32_t active_tab = lv_tabview_get_tab_act(tabview_);
                if (active_tab == 1) {
                    lv_tabview_set_act(tabview_, 0, LV_ANIM_OFF);
                }
            }
        } else {
            // 隐藏容器
            if (preload_progress_container_) {
                lv_obj_add_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
            }
            
            // 重新启用用户交互
            EnableUserInteraction();
        }
    }
    
private:
    
    // 创建下载进度UI
    void CreateDownloadProgressUI() {
        // 创建一个简单的文本容器
        download_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(download_progress_container_, lv_pct(80), lv_pct(30));
        lv_obj_center(download_progress_container_);
        
        // 设置文本容器样式
        lv_obj_set_style_radius(download_progress_container_, 10, 0); // 圆角矩形
        lv_obj_set_style_bg_color(download_progress_container_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(download_progress_container_, LV_OPA_80, 0);  // 使用正确的不透明度常量
        lv_obj_set_style_border_width(download_progress_container_, 2, 0);
        lv_obj_set_style_border_color(download_progress_container_, lv_color_hex(0x00AAFF), 0);
        
        // 设置垂直布局
        lv_obj_set_flex_flow(download_progress_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(download_progress_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(download_progress_container_, 15, 0);
        lv_obj_set_style_pad_row(download_progress_container_, 10, 0);

        // 标题标签
        lv_obj_t* title_label = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(title_label, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
        lv_label_set_text(title_label, "下载图片资源");
        
        // 进度百分比标签
        download_progress_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(download_progress_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(download_progress_label_, lv_color_hex(0x00AAFF), 0);
        lv_label_set_text(download_progress_label_, "0%");
        
        // 消息标签
        message_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(message_label_, lv_color_white(), 0);
        lv_obj_set_width(message_label_, lv_pct(90));
        lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(message_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(message_label_, "准备下载...");
        
        // 确保UI在最顶层
        lv_obj_move_foreground(download_progress_container_);
    }

    // 创建预加载进度UI
    void CreatePreloadProgressUI() {
        // 创建一个简单的文本容器
        preload_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(preload_progress_container_, lv_pct(85), lv_pct(35));
        lv_obj_center(preload_progress_container_);
        
        // 设置文本容器样式
        lv_obj_set_style_radius(preload_progress_container_, 12, 0); // 圆角矩形
        lv_obj_set_style_bg_color(preload_progress_container_, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_bg_opa(preload_progress_container_, LV_OPA_90, 0);  // 更高的不透明度
        lv_obj_set_style_border_width(preload_progress_container_, 2, 0);
        lv_obj_set_style_border_color(preload_progress_container_, lv_color_hex(0xFF9500), 0); // 橙色边框
        
        // 设置垂直布局
        lv_obj_set_flex_flow(preload_progress_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(preload_progress_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(preload_progress_container_, 18, 0);
        lv_obj_set_style_pad_row(preload_progress_container_, 12, 0);

        // 标题标签
        lv_obj_t* title_label = lv_label_create(preload_progress_container_);
        lv_obj_set_style_text_font(title_label, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(title_label, lv_color_hex(0xFF9500), 0); // 橙色标题
        lv_label_set_text(title_label, "预加载图片资源");
        
        // 进度百分比标签
        preload_progress_label_ = lv_label_create(preload_progress_container_);
        lv_obj_set_style_text_font(preload_progress_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(preload_progress_label_, lv_color_white(), 0);
        lv_label_set_text(preload_progress_label_, "0/0");
        
        // 消息标签
        preload_message_label_ = lv_label_create(preload_progress_container_);
        lv_obj_set_style_text_font(preload_message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(preload_message_label_, lv_color_white(), 0);
        lv_obj_set_width(preload_message_label_, lv_pct(90));
        lv_obj_set_style_text_align(preload_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(preload_message_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(preload_message_label_, "准备预加载...");
        
        // 添加提示文本
        lv_obj_t* hint_label = lv_label_create(preload_progress_container_);
        lv_obj_set_style_text_font(hint_label, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(hint_label, lv_color_hex(0xAAAAAA), 0); // 灰色提示
        lv_obj_set_width(hint_label, lv_pct(90));
        lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(hint_label, LV_LABEL_LONG_WRAP);
        lv_label_set_text(hint_label, "请勿操作设备");
        
        // 确保UI在最顶层
        lv_obj_move_foreground(preload_progress_container_);
    }

    // 禁用用户交互
    void DisableUserInteraction() {
        user_interaction_disabled_ = true;
        ESP_LOGI(TAG, "用户交互已禁用");
        
        // 禁用空闲定时器，防止自动切换页面
        SetIdle(false);
    }
    
    // 启用用户交互
    void EnableUserInteraction() {
        user_interaction_disabled_ = false;
        ESP_LOGI(TAG, "用户交互已启用");
        
        // 重新启用空闲定时器
        SetIdle(true);
    }

    // 添加新方法直接更新UI，只在主线程中调用
    void UpdateDownloadProgressUI(bool show, int progress, const char* message) {
        // UI更新逻辑
        DisplayLockGuard lock(this);
        
        // 如果容器不存在但需要显示，创建UI
        if (download_progress_container_ == nullptr && show) {
            CreateDownloadProgressUI();
        }
        
        // 如果容器仍不存在，直接返回
        if (download_progress_container_ == nullptr) {
            return;
        }
        
        if (show) {
            // 确保进度值在0-100范围内
            if (progress < 0) progress = 0;
            if (progress > 100) progress = 100;
            
            // 更新进度标签
            if (download_progress_label_) {
                char percent_text[16];
                snprintf(percent_text, sizeof(percent_text), "进度: %d%%", progress);
                lv_label_set_text(download_progress_label_, percent_text);
            }
            
            // 更新消息
            if (message && message_label_ != nullptr) {
                lv_label_set_text(message_label_, message);
            }
            
            // 确保容器可见
            lv_obj_clear_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            
            // 确保在最顶层显示
            lv_obj_move_foreground(download_progress_container_);
            
            // 禁用空闲定时器
            SetIdle(false);
            
            // 如果当前在时钟页面，切换回主页面
            if (tabview_) {
                uint32_t active_tab = lv_tabview_get_tab_act(tabview_);
                if (active_tab == 1) {
                    lv_tabview_set_act(tabview_, 0, LV_ANIM_OFF);
                }
            }
        } else {
            // 隐藏容器
            lv_obj_add_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            // 重新启用空闲定时器
            SetIdle(true);
        }
    }
};


// 自定义板卡类，继承自WifiBoard
class CustomBoard : public WifiBoard {
private:

    i2c_master_bus_handle_t codec_i2c_bus_;  // 编解码器I2C总线句柄
    CustomLcdDisplay* display_;              // LCD显示对象指针
    Button boot_btn;                         // 启动按钮
 
    esp_lcd_panel_io_handle_t io_handle = nullptr;  // LCD面板IO句柄
    esp_lcd_panel_handle_t panel = nullptr;        // LCD面板句柄

    // 图片显示任务句柄
    TaskHandle_t image_task_handle_ = nullptr;

    // 将URL定义为静态变量 - 现在只需要一个API URL
    static const char* API_URL;
    static const char* VERSION_URL;

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
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(DISPLAY_SPI_SCLK_PIN, DISPLAY_SPI_MOSI_PIN, 
                                    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));    // 初始化SPI总线
    }

    // 初始化LCD显示器
    void InitializeLcdDisplay() {
        ESP_LOGI(TAG, "Init GC9A01 display");
        
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(DISPLAY_SPI_CS_PIN, DISPLAY_SPI_DC_PIN, NULL, NULL);
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));  // 创建SPI面板IO

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;    // 复位引脚
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;           // RGB字节序
        panel_config.bits_per_pixel = 16;                       // 每像素位数
 
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel));  // 创建GC9A01面板
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));  // 重置面板
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));  // 初始化面板
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));  // 反转颜色
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));            // 是否交换XY坐标
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));  // 设置镜像
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true)); // 开启显示
        
        // 创建自定义LCD显示对象
        display_ = new CustomLcdDisplay(io_handle, panel,
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
            // 检查用户交互是否被禁用
            if (display_ && static_cast<CustomLcdDisplay*>(display_)->user_interaction_disabled_) {
                ESP_LOGW(TAG, "用户交互已禁用，忽略按钮点击");
                return;
            }
            
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
        thing_manager.AddThing(iot::CreateThing("Speaker"));         // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Screen"));          // 添加屏幕设备
        thing_manager.AddThing(iot::CreateThing("RotateDisplay"));   // 添加旋转显示设备
        thing_manager.AddThing(iot::CreateThing("ImageDisplay"));    // 添加图片显示控制设备
#if CONFIG_USE_ALARM
        thing_manager.AddThing(iot::CreateThing("AlarmIot"));
#endif
    }

    // 初始化图片资源管理器
    void InitializeImageResources() {
        auto& image_manager = ImageResourceManager::GetInstance();
        esp_err_t result = image_manager.Initialize();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "图片资源管理器初始化失败");
        }
    }

    // 检查图片资源更新
    void CheckImageResources() {
        auto& image_manager = ImageResourceManager::GetInstance();
        
        // 等待WiFi连接
        auto& wifi = WifiStation::GetInstance();
        while (!wifi.IsConnected()) {
            ESP_LOGI(TAG, "等待WiFi连接以检查图片资源...");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
        
        ESP_LOGI(TAG, "WiFi已连接，等待开机提示音播放完成...");
        
        // 等待开机提示音播放完成，避免与图片预加载冲突
        auto& app = Application::GetInstance();
        int wait_count = 0;
        const int max_wait_time = 8; // 最多等待8秒
        bool audio_finished = false;
        
        while (wait_count < max_wait_time && !audio_finished) {
            DeviceState state = app.GetDeviceState();
            bool queue_empty = app.IsAudioQueueEmpty();
            
            // 如果设备处于空闲状态且音频队列为空，说明提示音已播放完成
            if (state == kDeviceStateIdle && queue_empty) {
                // 再等待1秒确保音频完全播放完成（包括硬件缓冲区）
                if (wait_count >= 1) {
                    audio_finished = true;
                    break;
                }
            }
            
            ESP_LOGI(TAG, "等待开机提示音播放完成... (%d/%d秒) [状态:%d, 队列空:%s]", 
                    wait_count + 1, max_wait_time, (int)state, queue_empty ? "是" : "否");
            vTaskDelay(pdMS_TO_TICKS(1000));
            wait_count++;
        }
        
        if (audio_finished) {
            ESP_LOGI(TAG, "开机提示音播放完成，开始检查图片资源");
        } else {
            ESP_LOGW(TAG, "等待超时，强制开始检查图片资源");
        }
        
        // 一次性检查并更新所有资源（动画图片和logo）
        esp_err_t all_resources_result = image_manager.CheckAndUpdateAllResources(API_URL, VERSION_URL);
        
        // 为了兼容后续逻辑，设置对应的结果值
        esp_err_t animation_result = all_resources_result;
        esp_err_t logo_result = all_resources_result;
        
        // 处理一次性资源检查结果
        bool has_updates = false;
        bool has_errors = false;
        
        if (all_resources_result == ESP_OK) {
            ESP_LOGI(TAG, "图片资源更新完成（一次API请求完成所有下载）");
            has_updates = true;
        } else if (all_resources_result == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "所有图片资源已是最新版本，无需更新");
        } else {
            ESP_LOGE(TAG, "图片资源检查/下载失败");
            has_errors = true;
        }
        
        // 更新静态logo图片
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            iot::g_static_image = logo;
            ESP_LOGI(TAG, "logo图片已设置");
        } else {
            ESP_LOGW(TAG, "未能获取logo图片，将使用默认显示");
        }
        
        // 仅当有实际下载更新且无严重错误时才重启
        if (has_updates && !has_errors) {
            ESP_LOGI(TAG, "图片资源有更新，3秒后重启设备...");
            for (int i = 3; i > 0; i--) {
                ESP_LOGI(TAG, "将在 %d 秒后重启...", i);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            esp_restart();
        } else if (has_errors) {
            ESP_LOGW(TAG, "图片资源下载存在错误，设备继续运行但可能缺少部分图片");
        } else {
            ESP_LOGI(TAG, "所有图片资源已是最新版本，无需重启");
        }
        
        // 在系统初始化完成后，预加载所有剩余图片
        ESP_LOGI(TAG, "系统初始化完成，准备开始预加载剩余图片...");
        
        // 再次确认音频系统稳定后才开始预加载
        auto& app_preload = Application::GetInstance();
        int preload_wait = 0;
        while (preload_wait < 3) { // 最多再等3秒
            if (app_preload.GetDeviceState() == kDeviceStateIdle && app_preload.IsAudioQueueEmpty()) {
                break;
            }
            ESP_LOGI(TAG, "等待音频系统完全稳定后开始预加载... (%d/3秒)", preload_wait + 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
            preload_wait++;
        }
        
        ESP_LOGI(TAG, "开始预加载剩余图片...");
        esp_err_t preload_result = image_manager.PreloadRemainingImages();
        if (preload_result == ESP_OK) {
            ESP_LOGI(TAG, "图片预加载完成，动画播放将更加流畅");
        } else if (preload_result == ESP_ERR_NO_MEM) {
            ESP_LOGW(TAG, "内存不足，跳过图片预加载，将继续使用按需加载策略");
        } else {
            ESP_LOGW(TAG, "图片预加载失败，将继续使用按需加载策略");
        }
    }

    // 启动图片循环显示任务
    void StartImageSlideshow() {
        // 设置图片资源管理器的进度回调
        auto& image_manager = ImageResourceManager::GetInstance();
        
        // 设置下载进度回调函数，更新UI进度条
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
        
        image_manager.SetDownloadProgressCallback([customDisplay](int current, int total, const char* message) {
            if (customDisplay) {
                // 计算正确的百分比并传递
                int percent = (total > 0) ? (current * 100 / total) : 0;
                
                // 简化：直接调用显示方法
                customDisplay->ShowDownloadProgress(message != nullptr, percent, message);
            }
        });
        
        // 设置预加载进度回调函数，更新预加载UI进度
        image_manager.SetPreloadProgressCallback([customDisplay](int current, int total, const char* message) {
            if (customDisplay) {
                // 使用预加载专用的UI更新方法
                customDisplay->UpdatePreloadProgressUI(message != nullptr, current, total, message);
            }
        });
        
        // 启动图片轮播任务
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 8192, this, 3, &image_task_handle_);
        ESP_LOGI(TAG, "图片循环显示任务已启动");
        
        // 设置图片资源检查回调，等待OTA检查完成后执行
        auto& app = Application::GetInstance();
        app.SetImageResourceCallback([this]() {
            ESP_LOGI(TAG, "OTA检查完成，开始检查图片资源");
            // 创建后台任务执行图片资源检查
            xTaskCreate([](void* param) {
                CustomBoard* board = static_cast<CustomBoard*>(param);
                board->CheckImageResources();
                vTaskDelete(NULL);
            }, "img_resource_check", 16384, this, 3, NULL);
        });
    }

    // 添加帮助函数用于创建回调参数
    template<typename T, typename... Args>
    static T* malloc_struct(Args... args) {
        T* result = (T*)malloc(sizeof(T));
        if (result) {
            *result = {args...};
        }
        return result;
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
        int imgWidth = 240;
        int imgHeight = 240;
        
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
            .data = NULL,  // 会在更新时设置
            .reserved = NULL
        };
        
        // 创建一个图像容器，放在tab1上
        lv_obj_t* img_container = nullptr;
        lv_obj_t* img_obj = nullptr;
        
        {
            DisplayLockGuard lock(display);
            
            // 创建图像容器
            img_container = lv_obj_create(customDisplay->tab1);
            lv_obj_remove_style_all(img_container);
            lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
            lv_obj_center(img_container);
            lv_obj_set_style_border_width(img_container, 0, 0);
            lv_obj_set_style_bg_opa(img_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(img_container, 0, 0);
            lv_obj_move_foreground(img_container);  // 确保显示在最前面
            
            // 创建图像对象
            img_obj = lv_img_create(img_container);
            lv_obj_center(img_obj);
            lv_obj_move_foreground(img_obj);
        }
        
        // 获取图片资源管理器实例
        auto& image_manager = ImageResourceManager::GetInstance();
        
        // 添加延迟，确保资源管理器有足够时间初始化
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // 尝试从资源管理器获取logo图片
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            iot::g_static_image = logo;
            ESP_LOGI(TAG, "已从资源管理器获取logo图片");
        } else {
            ESP_LOGW(TAG, "暂无logo图片，等待下载...");
        }
        
        // 立即尝试显示静态图片
        if (g_image_display_mode == iot::MODE_STATIC && g_static_image) {
            // 如果有静态图片（logo），使用它
            DisplayLockGuard lock(display);
            img_dsc.data = g_static_image;
            lv_img_set_src(img_obj, &img_dsc);
            ESP_LOGI(TAG, "开机立即显示logo图片");
        } else {
            // 否则尝试使用资源管理器中的图片
            const auto& imageArray = image_manager.GetImageArray();
            if (!imageArray.empty()) {
                const uint8_t* currentImage = imageArray[0];
                if (currentImage) {
                    DisplayLockGuard lock(display);
                    img_dsc.data = currentImage;
                    lv_img_set_src(img_obj, &img_dsc);
                    ESP_LOGI(TAG, "开机立即显示存储的图片");
                } else {
                    ESP_LOGW(TAG, "图片数据为空");
                }
            } else {
                ESP_LOGW(TAG, "图片数组为空");
            }
        }
        
        // 等待预加载完成（如果正在预加载）
        ESP_LOGI(TAG, "检查预加载状态...");
        int preload_check_count = 0;
        while (preload_check_count < 100) { // 最多等待10秒
            bool isPreloadActive = false;
            if (customDisplay && customDisplay->preload_progress_container_ &&
                !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                isPreloadActive = true;
            }
            
            if (!isPreloadActive) {
                break; // 预加载已完成或未开始
            }
            
            ESP_LOGI(TAG, "等待预加载完成... (%d/100)", preload_check_count + 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            preload_check_count++;
        }
        
        if (preload_check_count >= 100) {
            ESP_LOGW(TAG, "预加载等待超时，继续启动图片轮播");
        } else {
            ESP_LOGI(TAG, "预加载已完成，开始图片轮播");
        }
        
        // 资源检查现在由OTA完成后的回调触发，这里不再需要定时器检查
        
        // 当前索引和方向控制
        int currentIndex = 0;
        bool directionForward = true;  // 动画方向：true为正向，false为反向
        const uint8_t* currentImage = nullptr;
        
        // 主循环
        TickType_t lastUpdateTime = xTaskGetTickCount();  // 记录上次更新时间
        const TickType_t cycleInterval = pdMS_TO_TICKS(120);  // 图片切换间隔120毫秒
        
        // 循环变量定义
        bool isAudioPlaying = false;       
        bool wasAudioPlaying = false;      
        DeviceState previousState = app.GetDeviceState();  
        bool pendingAnimationStart = false;  
        TickType_t stateChangeTime = 0;      
        
        while (true) {
            // 获取图片数组
            const auto& imageArray = image_manager.GetImageArray();
            
            // 资源检查现在由OTA完成后的回调处理，不再需要定时器
            
            // 如果没有图片资源，等待一段时间后重试
            if (imageArray.empty()) {
                static int wait_count = 0;
                wait_count++;
                
                if (wait_count <= 60) {  // 等待最多5分钟 (60 * 5秒)
                    ESP_LOGW(TAG, "图片资源未加载，等待... (%d/60)", wait_count);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    continue;
                } else {
                    ESP_LOGE(TAG, "图片资源等待超时，显示黑屏");
                    // 隐藏图像容器，显示黑屏
                    DisplayLockGuard lock(display);
                    if (img_container) {
                        lv_obj_add_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    }
                    vTaskDelay(pdMS_TO_TICKS(10000));  // 等待10秒后重新检查
                    wait_count = 0;  // 重置计数器
                    continue;
                }
            }
            
            // 确保currentIndex在有效范围内
            if (currentIndex >= imageArray.size()) {
                currentIndex = 0;
            }
            
            // 获取当前设备状态
            DeviceState currentState = app.GetDeviceState();
            TickType_t currentTime = xTaskGetTickCount();
            
            // 检查当前是否在时钟页面（tab2）
            bool isClockTabActive = false;
            if (customDisplay && customDisplay->tabview_) {
                int active_tab = lv_tabview_get_tab_act(customDisplay->tabview_);
                isClockTabActive = (active_tab == 1);
            }
            
            // 检查预加载UI是否可见
            bool isPreloadUIVisible = false;
            if (customDisplay && customDisplay->preload_progress_container_ &&
                !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                isPreloadUIVisible = true;
            }
            
            // 时钟页面或预加载UI显示时的处理逻辑
            if (isClockTabActive || isPreloadUIVisible) {
                DisplayLockGuard lock(display);
                if (img_container) {
                    lv_obj_add_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            } else {
                // 主界面显示处理
                DisplayLockGuard lock(display);
                if (img_container) {
                    lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_align(img_container, LV_ALIGN_CENTER, 0, 0);
                    lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
                    lv_obj_move_to_index(img_container, 0);
                    
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj) {
                        lv_obj_center(img_obj);
                        lv_obj_move_foreground(img_obj);
                    }
                }
            }
            
            // 检测到状态变为Speaking
            if (currentState == kDeviceStateSpeaking && previousState != kDeviceStateSpeaking) {
                pendingAnimationStart = true;
                stateChangeTime = currentTime;
                directionForward = true;  // 重置方向为正向
                ESP_LOGI(TAG, "检测到音频状态改变，准备启动动画");
            }
            
            // 如果状态不是Speaking，确保isAudioPlaying为false
            if (currentState != kDeviceStateSpeaking && isAudioPlaying) {
                isAudioPlaying = false;
                ESP_LOGI(TAG, "退出说话状态，停止动画");
            }
            
            // 延迟启动动画，等待音频实际开始播放
            if (pendingAnimationStart && (currentTime - stateChangeTime >= pdMS_TO_TICKS(1200))) {
                currentIndex = 1;  // 从第二帧开始
                directionForward = true;  // 确保方向为正向
                
                if (currentIndex < imageArray.size()) {
                    // 检查图片是否已加载（应该已经在预加载阶段完成）
                    int actual_image_index = currentIndex + 1;  // 转换为1基索引
                    if (!image_manager.IsImageLoaded(actual_image_index)) {
                        ESP_LOGW(TAG, "动画启动：图片 %d 未预加载，正在紧急加载...", actual_image_index);
                        if (!image_manager.LoadImageOnDemand(actual_image_index)) {
                            ESP_LOGE(TAG, "动画启动：图片 %d 紧急加载失败，使用第一张图片", actual_image_index);
                            currentIndex = 0;  // 回退到第一张图片
                        }
                    } else {
                        ESP_LOGI(TAG, "动画启动：图片 %d 已预加载，开始流畅播放", actual_image_index);
                    }
                    
                    currentImage = imageArray[currentIndex];
                    
                    // 直接使用图片数据，不再进行字节序转换
                    DisplayLockGuard lock(display);
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj && currentImage) {
                        img_dsc.data = currentImage;  // 直接使用原始图像数据
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                    
                    ESP_LOGI(TAG, "开始播放动画，与音频同步");
                    
                    lastUpdateTime = currentTime;
                    isAudioPlaying = true;         
                    pendingAnimationStart = false; 
                }
            }
            
            // 根据显示模式确定是否应该动画
            bool shouldAnimate = isAudioPlaying && g_image_display_mode == iot::MODE_ANIMATED;

            // 动画播放逻辑 - 实现双向循环（支持按需加载）
            if (shouldAnimate && !pendingAnimationStart && (currentTime - lastUpdateTime >= cycleInterval)) {
                // 根据方向更新索引
                if (directionForward) {
                    currentIndex++;
                    // 如果到达末尾，切换方向
                    if (currentIndex >= imageArray.size() - 1) {
                        directionForward = false;
                    }
                } else {
                    currentIndex--;
                    // 如果回到开始，切换方向
                    if (currentIndex <= 0) {
                        directionForward = true;
                        currentIndex = 0;  // 确保不会出现负索引
                    }
                }
                
                // 确保索引在有效范围内
                if (currentIndex >= 0 && currentIndex < imageArray.size()) {
                    // 检查图片是否已加载（应该已经在预加载阶段完成）
                    int actual_image_index = currentIndex + 1;  // 转换为1基索引
                    if (!image_manager.IsImageLoaded(actual_image_index)) {
                        ESP_LOGW(TAG, "动画播放：图片 %d 未预加载，正在紧急加载...", actual_image_index);
                        if (!image_manager.LoadImageOnDemand(actual_image_index)) {
                            ESP_LOGE(TAG, "动画播放：图片 %d 紧急加载失败，跳过此帧", actual_image_index);
                            lastUpdateTime = currentTime;
                            continue;  // 跳过这一帧，继续下一帧
                        }
                    }
                    
                    currentImage = imageArray[currentIndex];
                    
                    // 直接使用图片数据，不再进行字节序转换
                    DisplayLockGuard lock(display);
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj && currentImage) {
                        img_dsc.data = currentImage;  // 直接使用原始图像数据
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                }
                
                lastUpdateTime = currentTime;
            }
            // 处理静态图片显示
            else if ((!isAudioPlaying && wasAudioPlaying) || 
                     (g_image_display_mode == iot::MODE_STATIC && currentIndex != 0) || 
                     (!isAudioPlaying && currentIndex != 0)) {
                
                if (g_image_display_mode == iot::MODE_STATIC && iot::g_static_image) {
                    currentImage = iot::g_static_image;
                } else if (!imageArray.empty()) {
                    currentIndex = 0;
                    currentImage = imageArray[currentIndex];
                }
                
                // 直接使用图片数据，不再进行字节序转换
                if (currentImage) {
                    DisplayLockGuard lock(display);
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj) {
                        img_dsc.data = currentImage;  // 直接使用原始图像数据
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                    
                    ESP_LOGI(TAG, "显示%s图片", g_image_display_mode == iot::MODE_STATIC ? "logo" : "初始");
                    pendingAnimationStart = false;
                }
            }
            
            // 更新状态记录
            wasAudioPlaying = isAudioPlaying;
            previousState = currentState;
            
            // 短暂延时
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // 资源检查现在由OTA完成后的回调处理，不再使用定时器
        vTaskDelete(NULL);
    }

public:
    // 构造函数
    CustomBoard() : boot_btn(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();        // 初始化编解码器I2C总线
        InitializeSpi();             // 初始化SPI总线
        InitializeLcdDisplay();      // 初始化LCD显示器
        InitializeButtons();         // 初始化按钮
        InitializeIot();             // 初始化IoT设备
        InitializeImageResources();  // 初始化图片资源管理器
        GetBacklight()->RestoreBrightness();  // 恢复背光亮度
        
        // 显示初始化欢迎信息
        ShowWelcomeMessage();
        
        // 启动图片循环显示任务
        StartImageSlideshow();
    }
    
    // 显示欢迎信息
    void ShowWelcomeMessage() {
        if (!display_) return;
        
        // 获取WiFi状态
        auto& wifi_station = WifiStation::GetInstance();
        
        // 检查WiFi连接状态
        if (!wifi_station.IsConnected()) {
            // 显示配网提示
            display_->SetChatMessage("system", "欢迎使用独众AI伴侣\n设备连接网络中\n");
            
            // 将此消息也添加到通知区域，确保用户能看到
            display_->ShowNotification("请配置网络连接", 0);
        } else {
            // 已连接网络，显示正常欢迎信息
            display_->SetChatMessage("system", "欢迎使用独众AI伴侣\n正在初始化...");
        }
    }

    // LED功能已完全移除

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

// 将URL定义为静态变量 - 现在只需要一个API URL
const char* CustomBoard::API_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";
const char* CustomBoard::VERSION_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";

// 声明自定义板卡类为当前使用的板卡
DECLARE_BOARD(CustomBoard);
