#include "lcd_display.h"

#include <vector>
#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include "assets/lang_config.h"

#include "board.h"

#define TAG "LcdDisplay"  // 定义日志标签

LV_FONT_DECLARE(font_awesome_30_4);  // 声明Font Awesome字体

// SpiLcdDisplay类的构造函数
SpiLcdDisplay::SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts) {  // 调用基类构造函数
    width_ = width;  // 设置屏幕宽度
    height_ = height;  // 设置屏幕高度

    // 绘制白色背景
    std::vector<uint16_t> buffer(width_, 0xFFFF);  // 创建一个白色像素的缓冲区
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());  // 逐行绘制白色背景
    }

    // 打开显示屏
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));  // 打开LCD面板

    // 初始化LVGL库
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    // 初始化LVGL端口
    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();  // 获取默认配置
    port_cfg.task_priority = 1;  // 设置任务优先级
    lvgl_port_init(&port_cfg);  // 初始化LVGL端口

    // 添加LCD屏幕
    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,  // 面板IO句柄
        .panel_handle = panel_,  // 面板句柄
        .control_handle = nullptr,  // 控制句柄（未使用）
        .buffer_size = static_cast<uint32_t>(width_ * 10),  // 缓冲区大小
        .double_buffer = false,  // 不使用双缓冲
        .trans_size = 0,  // 传输大小
        .hres = static_cast<uint32_t>(width_),  // 水平分辨率
        .vres = static_cast<uint32_t>(height_),  // 垂直分辨率
        .monochrome = false,  // 非单色显示
        .rotation = {
            .swap_xy = swap_xy,  // 是否交换XY轴
            .mirror_x = mirror_x,  // 是否水平镜像
            .mirror_y = mirror_y,  // 是否垂直镜像
        },
        .color_format = LV_COLOR_FORMAT_RGB565,  // 颜色格式为RGB565
        .flags = {
            .buff_dma = 1,  // 使用DMA缓冲区
            .buff_spiram = 0,  // 不使用SPIRAM
            .sw_rotate = 0,  // 不使用软件旋转
            .swap_bytes = 1,  // 交换字节顺序
            .full_refresh = 0,  // 不使用全刷新
            .direct_mode = 0,  // 不使用直接模式
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);  // 添加显示设备
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");  // 如果添加失败，记录错误日志
        return;
    }

    // 设置显示偏移
    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    SetupUI();  // 初始化用户界面
}

// RgbLcdDisplay类的构造函数
RgbLcdDisplay::RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y,
                           bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts) {  // 调用基类构造函数
    width_ = width;  // 设置屏幕宽度
    height_ = height;  // 设置屏幕高度
    
    // 绘制白色背景
    std::vector<uint16_t> buffer(width_, 0xFFFF);  // 创建一个白色像素的缓冲区
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());  // 逐行绘制白色背景
    }

    // 初始化LVGL库
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    // 初始化LVGL端口
    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();  // 获取默认配置
    port_cfg.task_priority = 1;  // 设置任务优先级
    lvgl_port_init(&port_cfg);  // 初始化LVGL端口

    // 添加LCD屏幕
    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,  // 面板IO句柄
        .panel_handle = panel_,  // 面板句柄
        .buffer_size = static_cast<uint32_t>(width_ * 10),  // 缓冲区大小
        .double_buffer = true,  // 使用双缓冲
        .hres = static_cast<uint32_t>(width_),  // 水平分辨率
        .vres = static_cast<uint32_t>(height_),  // 垂直分辨率
        .rotation = {
            .swap_xy = swap_xy,  // 是否交换XY轴
            .mirror_x = mirror_x,  // 是否水平镜像
            .mirror_y = mirror_y,  // 是否垂直镜像
        },
        .flags = {
            .buff_dma = 1,  // 使用DMA缓冲区
            .swap_bytes = 0,  // 不交换字节顺序
            .full_refresh = 1,  // 使用全刷新
            .direct_mode = 1,  // 使用直接模式
        },
    };

    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,  // 使用回写模式
            .avoid_tearing = true,  // 避免撕裂
        }
    };
    
    display_ = lvgl_port_add_disp_rgb(&display_cfg, &rgb_cfg);  // 添加RGB显示设备
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add RGB display");  // 如果添加失败，记录错误日志
        return;
    }
    
    // 设置显示偏移
    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    SetupUI();  // 初始化用户界面
}

// LcdDisplay类的析构函数
LcdDisplay::~LcdDisplay() {
    // 清理LVGL对象
    if (content_ != nullptr) {
        lv_obj_del(content_);  // 删除内容对象
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);  // 删除状态栏对象
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);  // 删除侧边栏对象
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);  // 删除容器对象
    }
    if (display_ != nullptr) {
        lv_display_delete(display_);  // 删除显示设备
    }

    // 清理LCD面板和IO
    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);  // 删除LCD面板
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);  // 删除LCD面板IO
    }
}

// 锁定LVGL端口
bool LcdDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);  // 尝试锁定LVGL端口
}

// 解锁LVGL端口
void LcdDisplay::Unlock() {
    lvgl_port_unlock();  // 解锁LVGL端口
}

// 初始化用户界面
void LcdDisplay::SetupUI() {
    DisplayLockGuard lock(this);  // 加锁确保线程安全

    auto screen = lv_screen_active();  // 获取当前活动屏幕
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);  // 设置屏幕的字体
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);  // 设置屏幕的文本颜色为黑色

    /* 容器 */
    container_ = lv_obj_create(screen);  // 创建容器对象
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);  // 设置容器大小为屏幕大小
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);  // 设置容器为垂直布局
    lv_obj_set_style_pad_all(container_, 0, 0);  // 设置容器内边距为0
    lv_obj_set_style_border_width(container_, 0, 0);  // 设置容器边框宽度为0
    lv_obj_set_style_pad_row(container_, 0, 0);  // 设置容器行间距为0

    /* 状态栏 */
    status_bar_ = lv_obj_create(container_);  // 创建状态栏对象
    lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height);  // 设置状态栏高度为字体行高
    lv_obj_set_style_radius(status_bar_, 0, 0);  // 设置状态栏圆角为0
    
    /* 内容区域 */
    content_ = lv_obj_create(container_);  // 创建内容区域对象
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);  // 关闭滚动条
    lv_obj_set_style_radius(content_, 0, 0);  // 设置内容区域圆角为0
    lv_obj_set_width(content_, LV_HOR_RES);  // 设置内容区域宽度为屏幕宽度
    lv_obj_set_flex_grow(content_, 1);  // 设置内容区域可以扩展

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);  // 设置内容区域为垂直布局
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);  // 设置内容区域子对象居中对齐

    emotion_label_ = lv_label_create(content_);  // 创建表情标签
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);  // 设置表情标签的字体
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);  // 设置表情标签的初始文本

    chat_message_label_ = lv_label_create(content_);  // 创建聊天消息标签
    lv_label_set_text(chat_message_label_, "");  // 设置聊天消息标签的初始文本为空
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9);  // 设置聊天消息标签宽度为屏幕宽度的90%
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);  // 设置聊天消息标签为自动换行模式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);  // 设置聊天消息标签文本居中对齐

    /* 状态栏布局 */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);  // 设置状态栏为水平布局
    lv_obj_set_style_pad_all(status_bar_, 0, 0);  // 设置状态栏内边距为0
    lv_obj_set_style_border_width(status_bar_, 0, 0);  // 设置状态栏边框宽度为0
    lv_obj_set_style_pad_column(status_bar_, 0, 0);  // 设置状态栏列间距为0
    lv_obj_set_style_pad_left(status_bar_, 2, 0);  // 设置状态栏左内边距为2
    lv_obj_set_style_pad_right(status_bar_, 2, 0);  // 设置状态栏右内边距为2

    network_label_ = lv_label_create(status_bar_);  // 创建网络状态标签
    lv_label_set_text(network_label_, "");  // 设置网络状态标签的初始文本为空
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);  // 设置网络状态标签的字体

    notification_label_ = lv_label_create(status_bar_);  // 创建通知标签
    lv_obj_set_flex_grow(notification_label_, 1);  // 设置通知标签可以扩展
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);  // 设置通知标签文本居中对齐
    lv_label_set_text(notification_label_, "");  // 设置通知标签的初始文本为空
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);  // 隐藏通知标签

    status_label_ = lv_label_create(status_bar_);  // 创建状态标签
    lv_obj_set_flex_grow(status_label_, 1);  // 设置状态标签可以扩展
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);  // 设置状态标签为循环滚动模式
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);  // 设置状态标签文本居中对齐
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);  // 设置状态标签的初始文本为“初始化中”

    mute_label_ = lv_label_create(status_bar_);  // 创建静音标签
    lv_label_set_text(mute_label_, "");  // 设置静音标签的初始文本为空
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);  // 设置静音标签的字体

    battery_label_ = lv_label_create(status_bar_);  // 创建电池标签
    lv_label_set_text(battery_label_, "");  // 设置电池标签的初始文本为空
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);  // 设置电池标签的字体
}

// 设置表情图标
void LcdDisplay::SetEmotion(const char* emotion) {
    struct Emotion {
        const char* icon;  // 表情图标
        const char* text;  // 表情文本
    };

    // 定义所有支持的表情
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
    std::string_view emotion_view(emotion);
    auto it = std::find_if(emotions.begin(), emotions.end(),
        [&emotion_view](const Emotion& e) { return e.text == emotion_view; });

    DisplayLockGuard lock(this);  // 加锁确保线程安全
    if (emotion_label_ == nullptr) {
        return;  // 如果表情标签未初始化，直接返回
    }

    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    lv_obj_set_style_text_font(emotion_label_, fonts_.emoji_font, 0);  // 设置表情标签的字体
    if (it != emotions.end()) {
        lv_label_set_text(emotion_label_, it->icon);  // 设置表情标签的图标
    } else {
        lv_label_set_text(emotion_label_, "😶");  // 设置默认表情图标
    }
}

// 设置图标
void LcdDisplay::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);  // 加锁确保线程安全
    if (emotion_label_ == nullptr) {
        return;  // 如果表情标签未初始化，直接返回
    }
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);  // 设置表情标签的字体
    lv_label_set_text(emotion_label_, icon);  // 设置表情标签的图标
}