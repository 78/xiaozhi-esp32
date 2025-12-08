// managed_components\lvgl__lvgl\examples\widgets\canvas\lv_example_canvas_1.c
#include "lcd_display.h"
#include "gif/lvgl_gif.h"
#include "settings.h"
#include "lvgl_theme.h"
#include "assets/lang_config.h"

#include <vector>
#include <algorithm>
#include <font_awesome.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <esp_psram.h>
#include <string>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cmath>
#include <math.h>
#include <cctype>
#include <qrcode.h>

#include "board.h"
#include "esp32_sd_music.h"
#include "application.h"
#include "lvgl.h"

// ============================================================
//  SD MUSIC PLAYER ACCESS (LVGL 9.x)
// ============================================================

static Esp32SdMusic* get_sd_player() {
    return Application::GetInstance().GetSdMusic();
}

// ---------- UTILITY: Convert ms → mm:ss (hoặc hh:mm:ss) ----------
static std::string ms_to_time_string(int64_t ms) {
    if (ms < 0) ms = 0;
	
    int total_sec = ms / 1000;
    int sec  = total_sec % 60;
    int min  = (total_sec / 60) % 60;
    int hour = total_sec / 3600;

    char buf[32];
    if (hour > 0)
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, min, sec);
    else
        snprintf(buf, sizeof(buf), "%02d:%02d", min, sec);

    return std::string(buf);
}


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TAG "LcdDisplay"

//Declare theme color
#define BAR_MAX_HEIGHT (240 / 2)
#define BAR_COL_NUM  40
#define LCD_FFT_SIZE 512
static int current_heights[BAR_COL_NUM] = {0};
static float avg_power_spectrum[LCD_FFT_SIZE/2]={-25.0f};

#define COLOR_BLACK   0x0000
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_WHITE   0xFFFF

// Color definitions for dark theme
#define DARK_BACKGROUND_COLOR       lv_color_hex(0x121212)     // Dark background
#define DARK_TEXT_COLOR             lv_color_white()           // White text
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0x1E1E1E)     // Slightly lighter than background
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37)     // Dark green
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333)     // Dark gray
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A)     // Medium gray
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA)     // Light gray text
#define DARK_BORDER_COLOR           lv_color_hex(0x333333)     // Dark gray border
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


// Define dark theme colors
const ThemeColors DARK_THEME = {
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
const ThemeColors LIGHT_THEME = {
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
//End declare theme color

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_4);

void LcdDisplay::InitializeLcdThemes() {
    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_awesome_30_4);

    // light theme
    auto light_theme = new LvglTheme("light");
    light_theme->set_background_color(lv_color_hex(0xFFFFFF));          //rgb(255, 255, 255)
    light_theme->set_text_color(lv_color_hex(0x000000));                //rgb(0, 0, 0)
    light_theme->set_chat_background_color(lv_color_hex(0xE0E0E0));     //rgb(224, 224, 224)
    light_theme->set_user_bubble_color(lv_color_hex(0x00FF00));         //rgb(0, 128, 0)
    light_theme->set_assistant_bubble_color(lv_color_hex(0xDDDDDD));    //rgb(221, 221, 221)
    light_theme->set_system_bubble_color(lv_color_hex(0xFFFFFF));       //rgb(255, 255, 255)
    light_theme->set_system_text_color(lv_color_hex(0x000000));         //rgb(0, 0, 0)
    light_theme->set_border_color(lv_color_hex(0x000000));              //rgb(0, 0, 0)
    light_theme->set_low_battery_color(lv_color_hex(0x000000));         //rgb(0, 0, 0)
    light_theme->set_text_font(text_font);
    light_theme->set_icon_font(icon_font);
    light_theme->set_large_icon_font(large_icon_font);

    // dark theme
    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_background_color(lv_color_hex(0x000000));           //rgb(0, 0, 0)
    dark_theme->set_text_color(lv_color_hex(0xFFFFFF));                 //rgb(255, 255, 255)
    dark_theme->set_chat_background_color(lv_color_hex(0x1F1F1F));      //rgb(31, 31, 31)
    dark_theme->set_user_bubble_color(lv_color_hex(0x00FF00));          //rgb(0, 128, 0)
    dark_theme->set_assistant_bubble_color(lv_color_hex(0x222222));     //rgb(34, 34, 34)
    dark_theme->set_system_bubble_color(lv_color_hex(0x000000));        //rgb(0, 0, 0)
    dark_theme->set_system_text_color(lv_color_hex(0xFFFFFF));          //rgb(255, 255, 255)
    dark_theme->set_border_color(lv_color_hex(0xFFFFFF));               //rgb(255, 255, 255)
    dark_theme->set_low_battery_color(lv_color_hex(0xFF0000));          //rgb(255, 0, 0)
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("light", light_theme);
    theme_manager.RegisterTheme("dark", dark_theme);
}

LcdDisplay::LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    final_pcm_data_fft = nullptr;
    rotation_degree_ = 0;
    bar_max_hight_ = height_ / 2; // BAR_MAX_HEIGHT

    // Initialize LCD themes
    InitializeLcdThemes();

    // Load theme from settings
    Settings settings("display", false);
    std::string theme_name = settings.GetString("theme", "light");
    current_theme_ = LvglThemeManager::GetInstance().GetTheme(theme_name);

    // Create a timer to hide the preview image
    esp_timer_create_args_t preview_timer_args = {
        .callback = [](void* arg) {
            LcdDisplay* display = static_cast<LcdDisplay*>(arg);
            display->SetPreviewImage(nullptr);
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "preview_timer",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&preview_timer_args, &preview_timer_);
}

SpiLcdDisplay::SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height) {

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    {
        esp_err_t __err = esp_lcd_panel_disp_on_off(panel_, true);
        if (__err == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "Panel does not support disp_on_off; assuming ON");
        } else {
            ESP_ERROR_CHECK(__err);
        }
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

#if CONFIG_SPIRAM
    // lv image cache, currently only PNG is supported
    size_t psram_size_mb = esp_psram_get_size() / 1024 / 1024;
    if (psram_size_mb >= 8) {
        lv_image_cache_resize(2 * 1024 * 1024, true);
        ESP_LOGI(TAG, "Use 2MB of PSRAM for image cache");
    } else if (psram_size_mb >= 2) {
        lv_image_cache_resize(512 * 1024, true);
        ESP_LOGI(TAG, "Use 512KB of PSRAM for image cache");
    }
#endif

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 20),
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
	
    fft_real = (float*)heap_caps_malloc(LCD_FFT_SIZE * sizeof(float), MALLOC_CAP_SPIRAM);
    fft_imag = (float*)heap_caps_malloc(LCD_FFT_SIZE * sizeof(float), MALLOC_CAP_SPIRAM);
    hanning_window_float = (float*)heap_caps_malloc(LCD_FFT_SIZE * sizeof(float), MALLOC_CAP_SPIRAM);

    for (int i = 0; i < LCD_FFT_SIZE; i++) {
        hanning_window_float[i] = 0.5 * (1.0 - cos(2.0 * M_PI * i / (LCD_FFT_SIZE - 1)));
    }
    
    if(audio_data_==nullptr){
        audio_data_=(int16_t*)heap_caps_malloc(sizeof(int16_t)*1152, MALLOC_CAP_SPIRAM);
        memset(audio_data_,0,sizeof(int16_t)*1152);
    }
    if(frame_audio_data==nullptr){
        frame_audio_data=(int16_t*)heap_caps_malloc(sizeof(int16_t)*1152, MALLOC_CAP_SPIRAM);
        memset(frame_audio_data,0,sizeof(int16_t)*1152);
    }
    
    ESP_LOGI(TAG,"Initialize fft_input, audio_data_, frame_audio_data, spectrum_data");
    SetupUI();
}

// RGB LCD implementation
RgbLcdDisplay::RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y,
                           bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height) {

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .buffer_size = static_cast<uint32_t>(width_ * 20),
        .double_buffer = true,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .swap_bytes = 0,
            .full_refresh = 1,
            .direct_mode = 1,
        },
    };

    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,
            .avoid_tearing = true,
        }
    };
    
    display_ = lvgl_port_add_disp_rgb(&display_cfg, &rgb_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add RGB display");
        return;
    }
    
    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    SetupUI();
}

MipiLcdDisplay::MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                            int width, int height,  int offset_x, int offset_y,
                            bool mirror_x, bool mirror_y, bool swap_xy)
    : LcdDisplay(panel_io, panel, width, height) {

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = panel_io,
        .panel_handle = panel,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 50),
        .double_buffer = false,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram =false,
            .sw_rotate = true,
        },
    };

    const lvgl_port_display_dsi_cfg_t dpi_cfg = {
        .flags = {
            .avoid_tearing = false,
        }
    };
    display_ = lvgl_port_add_disp_dsi(&disp_cfg, &dpi_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    SetupUI();
}

LcdDisplay::~LcdDisplay() {
    SetPreviewImage(nullptr);
    
    // Clean up GIF controller
    if (gif_controller_) {
        gif_controller_->Stop();
        gif_controller_.reset();
    }
    
    if (preview_timer_ != nullptr) {
        esp_timer_stop(preview_timer_);
        esp_timer_delete(preview_timer_);
    }

    if (preview_image_ != nullptr) {
        lv_obj_del(preview_image_);
    }
    if (chat_message_label_ != nullptr) {
        lv_obj_del(chat_message_label_);
    }
    if (emoji_label_ != nullptr) {
        lv_obj_del(emoji_label_);
    }
    if (emoji_image_ != nullptr) {
        lv_obj_del(emoji_image_);
    }
    if (emoji_box_ != nullptr) {
        lv_obj_del(emoji_box_);
    }
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
}

bool LcdDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void LcdDisplay::Unlock() {
    lvgl_port_unlock();
}

#if CONFIG_USE_WECHAT_MESSAGE_STYLE
void LcdDisplay::SetupUI() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lvgl_theme->text_color(), 0);
    lv_obj_set_style_bg_color(screen, lvgl_theme->background_color(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_border_color(container_, lvgl_theme->border_color(), 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_text_color(status_bar_, lvgl_theme->text_color(), 0);
    
    /* Content - Chat area */
    content_ = lv_obj_create(container_);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_pad_all(content_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_bg_color(content_, lvgl_theme->chat_background_color(), 0); // Background for chat area

    // Enable scrolling for chat content
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(content_, LV_DIR_VER);
    
    // Create a flex container for chat messages
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content_, lvgl_theme->spacing(4), 0); // Space between messages

    // We'll create chat messages dynamically in SetChatMessage
    chat_message_label_ = nullptr;

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_top(status_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_bottom(status_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_left(status_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_pad_right(status_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    // 设置状态栏的内容垂直居中
    lv_obj_set_flex_align(status_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lvgl_theme->text_color(), 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    
    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, lvgl_theme->text_color(), 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_margin_left(battery_label_, lvgl_theme->spacing(2), 0); // 添加左边距，与前面的元素分隔

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -lvgl_theme->spacing(4));
    lv_obj_set_style_bg_color(low_battery_popup_, lvgl_theme->low_battery_color(), 0);
    lv_obj_set_style_radius(low_battery_popup_, lvgl_theme->spacing(4), 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    emoji_image_ = lv_img_create(screen);
    lv_obj_align(emoji_image_, LV_ALIGN_TOP_MID, 0, text_font->line_height + lvgl_theme->spacing(8));

    // Display AI logo while booting
    emoji_label_ = lv_label_create(screen);
    lv_obj_center(emoji_label_);
    lv_obj_set_style_text_font(emoji_label_, large_icon_font, 0);
    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(emoji_label_, FONT_AWESOME_MICROCHIP_AI);

    Settings settings("display", false);
    int rotation_degree = settings.GetInt("rotation_degree", 0);
    if (rotation_degree != 0) {
        SetRotation(rotation_degree, false);
    }
}
#if CONFIG_IDF_TARGET_ESP32P4
#define  MAX_MESSAGES 40
#else
#define  MAX_MESSAGES 20
#endif
void LcdDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (content_ == nullptr) {
        return;
    }
    
    // Check if the number of messages exceeds the limit
    uint32_t child_count = lv_obj_get_child_cnt(content_);
    if (child_count >= MAX_MESSAGES) {
        // Delete the oldest message (first child object)
        lv_obj_t* first_child = lv_obj_get_child(content_, 0);
        lv_obj_t* last_child = lv_obj_get_child(content_, child_count - 1);
        if (first_child != nullptr) {
            lv_obj_del(first_child);
        }
        // Scroll to the last message immediately
        if (last_child != nullptr) {
            lv_obj_scroll_to_view_recursive(last_child, LV_ANIM_OFF);
        }
    }
    
    // Collapse system messages (if the message is a system message, check if the last message is also a system message)
    if (strcmp(role, "system") == 0) {
        if (child_count > 0) {
            // Get the last message container
            lv_obj_t* last_container = lv_obj_get_child(content_, child_count - 1);
            if (last_container != nullptr && lv_obj_get_child_cnt(last_container) > 0) {
                // Get the bubble inside the container
                lv_obj_t* last_bubble = lv_obj_get_child(last_container, 0);
                if (last_bubble != nullptr) {
                    // Check if the bubble type is a system message
                    void* bubble_type_ptr = lv_obj_get_user_data(last_bubble);
                    if (bubble_type_ptr != nullptr && strcmp((const char*)bubble_type_ptr, "system") == 0) {
                        // If the last message is also a system message, delete it
                        lv_obj_del(last_container);
                    }
                }
            }
        }
    } else {
        // Hide the centered AI logo
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }

    // Avoid creating an empty message bubble
    if(strlen(content) == 0) {
        return;
    }

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();

    // Create a message bubble
    lv_obj_t* msg_bubble = lv_obj_create(content_);
    lv_obj_set_style_radius(msg_bubble, 8, 0);
    lv_obj_set_scrollbar_mode(msg_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(msg_bubble, 0, 0);
    lv_obj_set_style_pad_all(msg_bubble, lvgl_theme->spacing(4), 0);

    // Create the message text
    lv_obj_t* msg_text = lv_label_create(msg_bubble);
    lv_label_set_text(msg_text, content);
    
    // Calculate the actual text width
    lv_coord_t text_width = lv_txt_get_width(content, strlen(content), text_font, 0);

    // Calculate the bubble width
    lv_coord_t max_width = LV_HOR_RES * 85 / 100 - 16;  // 85% of screen width
    lv_coord_t min_width = 20;  
    lv_coord_t bubble_width;
    
    // Ensure the text width is not less than the minimum width
    if (text_width < min_width) {
        text_width = min_width;
    }

    // If the text width is less than the maximum width, use the text width
    if (text_width < max_width) {
        bubble_width = text_width; 
    } else {
        bubble_width = max_width;
    }
    
    // Set the width of the message text
    lv_obj_set_width(msg_text, bubble_width);  // Subtract padding
    lv_label_set_long_mode(msg_text, LV_LABEL_LONG_WRAP);

    // Set the bubble width
    lv_obj_set_width(msg_bubble, bubble_width);
    lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);

    // Set alignment and style based on message role
    if (strcmp(role, "user") == 0) {
        // User messages are right-aligned with green background
        lv_obj_set_style_bg_color(msg_bubble, lvgl_theme->user_bubble_color(), 0);
        lv_obj_set_style_bg_opa(msg_bubble, LV_OPA_70, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, lvgl_theme->text_color(), 0);
        
        // Set custom property to mark the bubble type
        lv_obj_set_user_data(msg_bubble, (void*)"user");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    } else if (strcmp(role, "assistant") == 0) {
        // Assistant messages are left-aligned with white background
        lv_obj_set_style_bg_color(msg_bubble, lvgl_theme->assistant_bubble_color(), 0);
        lv_obj_set_style_bg_opa(msg_bubble, LV_OPA_70, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, lvgl_theme->text_color(), 0);
        
        // Set custom property to mark the bubble type
        lv_obj_set_user_data(msg_bubble, (void*)"assistant");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    } else if (strcmp(role, "system") == 0) {
        // System messages are center-aligned with light gray background
        lv_obj_set_style_bg_color(msg_bubble, lvgl_theme->system_bubble_color(), 0);
        lv_obj_set_style_bg_opa(msg_bubble, LV_OPA_70, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, lvgl_theme->system_text_color(), 0);
        
        // Set custom property to mark the bubble type
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
        lv_obj_set_width(container, LV_HOR_RES);
        lv_obj_set_height(container, LV_SIZE_CONTENT);
        
        // Make container transparent and borderless
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        
        // Move the message bubble into this container
        lv_obj_set_parent(msg_bubble, container);
        
        // Right align the bubble in the container
        lv_obj_align(msg_bubble, LV_ALIGN_RIGHT_MID, -25, 0);
        
        // Auto-scroll to this container
        lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
    } else if (strcmp(role, "system") == 0) {
        // Create a full-width container for system messages to ensure center alignment
        lv_obj_t* container = lv_obj_create(content_);
        lv_obj_set_width(container, LV_HOR_RES);
        lv_obj_set_height(container, LV_SIZE_CONTENT);
        
        // Make container transparent and borderless
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        
        // Move the message bubble into this container
        lv_obj_set_parent(msg_bubble, container);
        
        // Center align the bubble in the container
        lv_obj_align(msg_bubble, LV_ALIGN_CENTER, 0, 0);
        
        // Auto-scroll to the bottom
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
}

void LcdDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    DisplayLockGuard lock(this);
    if (content_ == nullptr) {
        return;
    }

    if (image == nullptr) {
        return;
    }
    
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    // Create a message bubble for image preview
    lv_obj_t* img_bubble = lv_obj_create(content_);
    lv_obj_set_style_radius(img_bubble, 8, 0);
    lv_obj_set_scrollbar_mode(img_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(img_bubble, 0, 0);
    lv_obj_set_style_pad_all(img_bubble, lvgl_theme->spacing(4), 0);
    
    // Set the background color of the image bubble (similar to assistant messages)
    lv_obj_set_style_bg_color(img_bubble, lvgl_theme->assistant_bubble_color(), 0);
    lv_obj_set_style_bg_opa(img_bubble, LV_OPA_70, 0);
    
    // Set custom property to mark the bubble type
    lv_obj_set_user_data(img_bubble, (void*)"image");

    // Create the image object inside the bubble
    lv_obj_t* preview_image = lv_image_create(img_bubble);
    
    // Calculate appropriate size for the image
    lv_coord_t max_width = LV_HOR_RES * 70 / 100;  // 70% of screen width
    lv_coord_t max_height = LV_VER_RES * 50 / 100; // 50% of screen height
    
    // Calculate zoom factor to fit within maximum dimensions
    auto img_dsc = image->image_dsc();
    lv_coord_t img_width = img_dsc->header.w;
    lv_coord_t img_height = img_dsc->header.h;
    if (img_width == 0 || img_height == 0) {
        img_width = max_width;
        img_height = max_height;
        ESP_LOGW(TAG, "Invalid image dimensions: %ld x %ld, using default dimensions: %ld x %ld", img_width, img_height, max_width, max_height);
    }
    
    lv_coord_t zoom_w = (max_width * 256) / img_width;
    lv_coord_t zoom_h = (max_height * 256) / img_height;
    lv_coord_t zoom = (zoom_w < zoom_h) ? zoom_w : zoom_h;
    
    // Ensure zoom doesn't exceed 256 (100%)
    if (zoom > 256) zoom = 256;
    
    // Set image properties
    lv_image_set_src(preview_image, img_dsc);
    lv_image_set_scale(preview_image, zoom);
    
    // Add event handler to clean up LvglImage when the image is deleted
    // Transfer ownership of the unique_ptr to the event callback
    LvglImage* raw_image = image.release(); // Release ownership of the smart pointer
    lv_obj_add_event_cb(preview_image, [](lv_event_t* e) {
        LvglImage* img = (LvglImage*)lv_event_get_user_data(e);
        if (img != nullptr) {
            delete img; // Properly release memory by deleting the LvglImage object
        }
    }, LV_EVENT_DELETE, (void*)raw_image);
    
    // Calculate actual scaled image dimensions
    lv_coord_t scaled_width = (img_width * zoom) / 256;
    lv_coord_t scaled_height = (img_height * zoom) / 256;
    
    // Set bubble size to be 16 pixels larger than the image (8 pixels on each side)
    lv_obj_set_width(img_bubble, scaled_width + 16);
    lv_obj_set_height(img_bubble, scaled_height + 16);
    
    // Prevent flex layout from growing
    lv_obj_set_style_flex_grow(img_bubble, 0, 0);
    
    // Center the image within the bubble
    lv_obj_center(preview_image);
    
    // Align the image bubble to the left, like assistant messages
    lv_obj_align(img_bubble, LV_ALIGN_LEFT_MID, 0, 0);

    // Auto-scroll to the image bubble
    lv_obj_scroll_to_view_recursive(img_bubble, LV_ANIM_ON);
}
#else
void LcdDisplay::SetupUI() {
    DisplayLockGuard lock(this);
    LvglTheme* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lvgl_theme->text_color(), 0);
    lv_obj_set_style_bg_color(screen, lvgl_theme->background_color(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_border_color(container_, lvgl_theme->border_color(), 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, lvgl_theme->background_color(), 0);
    lv_obj_set_style_text_color(status_bar_, lvgl_theme->text_color(), 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_top(status_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_bottom(status_bar_, lvgl_theme->spacing(2), 0);
    lv_obj_set_style_pad_left(status_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_pad_right(status_bar_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    
    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_bg_color(content_, lvgl_theme->chat_background_color(), 0);

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // Vertical layout (top to bottom)
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY); // Center-align child objects, evenly spaced

    emoji_box_ = lv_obj_create(content_);
    lv_obj_set_size(emoji_box_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(emoji_box_, 0, 0);
    lv_obj_set_style_border_width(emoji_box_, 0, 0);

    emoji_label_ = lv_label_create(emoji_box_);
    lv_obj_set_style_text_font(emoji_label_, large_icon_font, 0);
    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(emoji_label_, FONT_AWESOME_MICROCHIP_AI);

    emoji_image_ = lv_img_create(emoji_box_);
    lv_obj_center(emoji_image_);
    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);

    preview_image_ = lv_image_create(content_);
    lv_obj_set_size(preview_image_, width_ / 2, height_ / 2);
    lv_obj_align(preview_image_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, width_ * 0.9); // Limit width to 90% of screen width
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // Set to word-wrap mode
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // Set text alignment to center
    lv_obj_set_style_text_color(chat_message_label_, lvgl_theme->text_color(), 0);

    /* Status bar */
    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lvgl_theme->text_color(), 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, lvgl_theme->text_color(), 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, lvgl_theme->text_color(), 0);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -lvgl_theme->spacing(4));
    lv_obj_set_style_bg_color(low_battery_popup_, lvgl_theme->low_battery_color(), 0);
    lv_obj_set_style_radius(low_battery_popup_, lvgl_theme->spacing(4), 0);
    
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    Settings settings("display", false);
    int rotation_degree = settings.GetInt("rotation_degree", 0);
    if (rotation_degree != 0) {
        SetRotation(rotation_degree, false);
    }
}

void LcdDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
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
    // Set the image source and display the preview image
    lv_image_set_src(preview_image_, img_dsc);
    if (img_dsc->header.w > 0 && img_dsc->header.h > 0) {
        // zoom factor 0.5
        lv_image_set_scale(preview_image_, 128 * width_ / img_dsc->header.w);
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

void LcdDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    lv_label_set_text(chat_message_label_, content);
}
#endif

void LcdDisplay::SetEmotion(const char* emotion) {
    // Stop any running GIF animation
    if (gif_controller_) {
        DisplayLockGuard lock(this);
        gif_controller_->Stop();
        gif_controller_.reset();
    }
    
    if (emoji_image_ == nullptr) {
        return;
    }

    auto emoji_collection = static_cast<LvglTheme*>(current_theme_)->emoji_collection();
    auto image = emoji_collection != nullptr ? emoji_collection->GetEmojiImage(emotion) : nullptr;
    if (image == nullptr) {
        const char* utf8 = font_awesome_get_utf8(emotion);
        if (utf8 != nullptr && emoji_label_ != nullptr) {
            DisplayLockGuard lock(this);
            lv_label_set_text(emoji_label_, utf8);
            lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    DisplayLockGuard lock(this);
    if (image->IsGif()) {
        // Create new GIF controller
        gif_controller_ = std::make_unique<LvglGif>(image->image_dsc());
        
        if (gif_controller_->IsLoaded()) {
            // Set up frame update callback
            gif_controller_->SetFrameCallback([this]() {
                lv_image_set_src(emoji_image_, gif_controller_->image_dsc());
            });
            
            // Set initial frame and start animation
            lv_image_set_src(emoji_image_, gif_controller_->image_dsc());
            gif_controller_->Start();
            
            // Show GIF, hide others
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        } else {
            ESP_LOGE(TAG, "Failed to load GIF for emotion: %s", emotion);
            gif_controller_.reset();
        }
    } else {
        lv_image_set_src(emoji_image_, image->image_dsc());
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }

#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    // In WeChat message style, if the emotion is neutral, do not display it
    uint32_t child_count = lv_obj_get_child_cnt(content_);
    if (strcmp(emotion, "neutral") == 0 && child_count > 0) {
        // Stop GIF animation if running
        if (gif_controller_) {
            gif_controller_->Stop();
            gif_controller_.reset();
        }
        
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
#endif
}

void LcdDisplay::SetMusicInfo(const char* song_name) {
    // Lưu lại vào biến để DetectSourceFromInfo() dùng
    if (song_name != nullptr)
        music_info_ = song_name;
    else
        music_info_.clear();

#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    // In WeChat mode, do not display the song name, keep the original chat functionality
    return;
#else
    // Non-WeChat mode: display the song name below the emoji
    DisplayLockGuard lock(this);

    // Nếu đang có UI FFT + nhạc trên canvas → update trực tiếp vào label
    if (canvas_ != nullptr && music_root_ != nullptr && lv_obj_is_valid(canvas_)) {
        std::string text = song_name ? song_name : "";
        std::string line1, line2;

        size_t pos = text.find('\n');
        if (pos != std::string::npos) {
            line1 = text.substr(0, pos);
            line2 = text.substr(pos + 1);
        } else {
            line1 = text;
            line2.clear();
        }

        // ==========================
        // 🎵 DÒNG 1 — TITLE (scroll)
        // ==========================
        if (music_title_label_ && lv_obj_is_valid(music_title_label_)) {
            lv_label_set_text(music_title_label_, line1.c_str());
            lv_label_set_long_mode(music_title_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_width(music_title_label_, canvas_width_ - 40);
        }

        // ==========================
        // 🎶 DÒNG 2 — SUBINFO (scroll)
        // ==========================
        if (music_subinfo_label_ && lv_obj_is_valid(music_subinfo_label_)) {
            lv_label_set_text(music_subinfo_label_, line2.c_str());
            lv_label_set_long_mode(music_subinfo_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_width(music_subinfo_label_, canvas_width_ - 40);
        }

        // Nếu rỗng → clear chat bubble
        if ((!song_name || strlen(song_name) == 0) && chat_message_label_) {
            lv_label_set_text(chat_message_label_, "");
        }

        return;  // ⬅️ RẤT QUAN TRỌNG — không để UI chat override
    }

    // ==========================
    // Không có FFT → hiển thị kiểu cũ
    // ==========================
    if (chat_message_label_ == nullptr) return;

    if (song_name != nullptr && strlen(song_name) > 0) {
        std::string music_text = "";
        music_text += song_name;
        lv_label_set_text(chat_message_label_, music_text.c_str());
        
        // Ensure emoji_label_ and chat_message_label_ are visible, and preview_image_ is hidden
        if (emoji_label_ != nullptr) {
            lv_obj_clear_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (preview_image_ != nullptr) {
            lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        // Clear the song name display
        lv_label_set_text(chat_message_label_, "");
    }
#endif
}

LcdDisplay::DisplaySourceType LcdDisplay::DetectSourceFromInfo() const {
    std::string s = music_info_;
    for (auto &c : s) c = tolower(c);

    // 1) Nhận diện Radio trước
    if (s.find("radio") != std::string::npos ||
        s.find("fm")    != std::string::npos)
        return DisplaySourceType::RADIO;

    // 2) Nhận diện Online trước
	if (s.rfind("ONLINE:", 0) == 0)
    return DisplaySourceType::ONLINE;

    if (s.find("online") != std::string::npos ||
        s.find("http")   != std::string::npos ||
        s.find("rtmp")   != std::string::npos ||
        s.find("m3u")    != std::string::npos)
        return DisplaySourceType::ONLINE;

    // 3) SD-card (chỉ khi REAL PLAYING)
    Esp32SdMusic* sd = get_sd_player();
    if (sd && sd->getState() == Esp32SdMusic::PlayerState::Playing)
        return DisplaySourceType::SD_CARD;

    // 4) Default
    return DisplaySourceType::NONE;
}

void LcdDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);
    
    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    
    // Get the active screen
    lv_obj_t* screen = lv_screen_active();

    // Set font
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    if (text_font->line_height >= 40) {
        lv_obj_set_style_text_font(mute_label_, large_icon_font, 0);
        lv_obj_set_style_text_font(battery_label_, large_icon_font, 0);
        lv_obj_set_style_text_font(network_label_, large_icon_font, 0);
    } else {
        lv_obj_set_style_text_font(mute_label_, icon_font, 0);
        lv_obj_set_style_text_font(battery_label_, icon_font, 0);
        lv_obj_set_style_text_font(network_label_, icon_font, 0);
    }

    // Set parent text color
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lvgl_theme->text_color(), 0);

    // Set background image
    if (lvgl_theme->background_image() != nullptr) {
        lv_obj_set_style_bg_image_src(container_, lvgl_theme->background_image()->image_dsc(), 0);
    } else {
        lv_obj_set_style_bg_image_src(container_, nullptr, 0);
        lv_obj_set_style_bg_color(container_, lvgl_theme->background_color(), 0);
    }
    
    // Update status bar background color with 50% opacity
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(status_bar_, lvgl_theme->background_color(), 0);
    
    // Update status bar elements
    lv_obj_set_style_text_color(network_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(status_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(notification_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(mute_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(battery_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);

    // Set content background opacity
    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);

    // If we have the chat message style, update all message bubbles
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    // Iterate through all children of content (message containers or bubbles)
    uint32_t child_count = lv_obj_get_child_cnt(content_);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* obj = lv_obj_get_child(content_, i);
        if (obj == nullptr) continue;
        
        lv_obj_t* bubble = nullptr;
        
        // Check if this object is a container or a bubble
        // If it's a container (user or system message), get its child as the bubble
        // If it's a bubble (assistant message), use it directly
        if (lv_obj_get_child_cnt(obj) > 0) {
            // Possibly a container, check if it's a user or system message container
            // User and system message containers are transparent
            lv_opa_t bg_opa = lv_obj_get_style_bg_opa(obj, 0);
            if (bg_opa == LV_OPA_TRANSP) {
                // This is a user or system message container
                bubble = lv_obj_get_child(obj, 0);
            } else {
                // This might be the assistant message bubble itself
                bubble = obj;
            }
        } else {
            // No children, possibly other UI elements, skip
            continue;
        }
        
        if (bubble == nullptr) continue;
        
        // Use saved user data to identify bubble type
        void* bubble_type_ptr = lv_obj_get_user_data(bubble);
        if (bubble_type_ptr != nullptr) {
            const char* bubble_type = static_cast<const char*>(bubble_type_ptr);
            
            // Apply the correct color based on bubble type
            if (strcmp(bubble_type, "user") == 0) {
                lv_obj_set_style_bg_color(bubble, lvgl_theme->user_bubble_color(), 0);
            } else if (strcmp(bubble_type, "assistant") == 0) {
                lv_obj_set_style_bg_color(bubble, lvgl_theme->assistant_bubble_color(), 0); 
            } else if (strcmp(bubble_type, "system") == 0) {
                lv_obj_set_style_bg_color(bubble, lvgl_theme->system_bubble_color(), 0);
            } else if (strcmp(bubble_type, "image") == 0) {
                lv_obj_set_style_bg_color(bubble, lvgl_theme->system_bubble_color(), 0);
            }
            
            // Update border color
            lv_obj_set_style_border_color(bubble, lvgl_theme->border_color(), 0);
            
            // Update text color for the message
            if (lv_obj_get_child_cnt(bubble) > 0) {
                lv_obj_t* text = lv_obj_get_child(bubble, 0);
                if (text != nullptr) {
                    // Set text color based on bubble type
                    if (strcmp(bubble_type, "system") == 0) {
                        lv_obj_set_style_text_color(text, lvgl_theme->system_text_color(), 0);
                    } else {
                        lv_obj_set_style_text_color(text, lvgl_theme->text_color(), 0);
                    }
                }
            }
        } else {
            ESP_LOGW(TAG, "child[%lu] Bubble type is not found", i);
        }
    }
#else
    // Simple UI mode - just update the main chat message
    if (chat_message_label_ != nullptr) {
        lv_obj_set_style_text_color(chat_message_label_, lvgl_theme->text_color(), 0);
    }
    
    if (emoji_label_ != nullptr) {
        lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);
    }
#endif
    
    // Update low battery popup
    lv_obj_set_style_bg_color(low_battery_popup_, lvgl_theme->low_battery_color(), 0);

    // No errors occurred. Save theme to settings
    Display::SetTheme(lvgl_theme);
}

void LcdDisplay::StartFFT() {
    ESP_LOGI(TAG, "Starting LcdDisplay with periodic data updates");
    
    vTaskDelay(pdMS_TO_TICKS(500));

    // Create a periodic update task
    fft_task_should_stop = false;  // Reset the stop flag
    xTaskCreatePinnedToCore(
        periodicUpdateTaskWrapper,
        "display_fft",      // Task name
        1024 * 3,           // Stack size
        this,               // Parameter
        1,                  // Priority
        &fft_task_handle,   // Save to member variable
        0                   // Run on core 0
    );
}

void LcdDisplay::StopFFT() {
    ESP_LOGI(TAG, "Stopping FFT display");
    
    // Stop the FFT display task
    if (fft_task_handle != nullptr) {
        ESP_LOGI(TAG, "Stopping FFT display task");
        fft_task_should_stop = true;  // Set the stop flag
        
        // Wait for the task to stop (wait up to 1 second)
        int wait_count = 0;
        while (fft_task_handle != nullptr && wait_count < 100) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_count++;
        }
        
        if (fft_task_handle != nullptr) {
            ESP_LOGW(TAG, "FFT task did not stop gracefully, force deleting");
            vTaskDelete(fft_task_handle);
            fft_task_handle = nullptr;
        } else {
            ESP_LOGI(TAG, "FFT display task stopped successfully");
        }
    }
    
    // Use the display lock to protect all operations
    DisplayLockGuard lock(this);
    
    // Reset FFT state variables
    fft_data_ready = false;
    audio_display_last_update = 0;
    
    // Reset the heights of the spectrum bars
    memset(current_heights, 0, sizeof(current_heights));
    
    // Reset the average power spectrum data
    for (int i = 0; i < LCD_FFT_SIZE/2; i++) {
        avg_power_spectrum[i] = -25.0f;
    }
    
    // Delete the FFT canvas object to restore the original UI
    if (canvas_ != nullptr) {
        lv_obj_del(canvas_);
        canvas_ = nullptr;
        ESP_LOGI(TAG, "FFT canvas deleted");
    }
	
	// =============================================
	// XÓA HOÀN TOÀN UI NHẠC (tránh bị đọng label)
	// =============================================
	if (music_root_ && lv_obj_is_valid(music_root_)) {
		lv_obj_del(music_root_);     // delete toàn bộ nhóm UI nhạc
		ESP_LOGI(TAG, "Music UI deleted");
	}
	
	// =========================
	// RESET MUSIC UI POINTERS
	// =========================
	music_root_          = nullptr;
	music_title_label_   = nullptr;
	music_date_label_    = nullptr;
	music_bar_           = nullptr;
	music_time_left_     = nullptr;
	music_time_total_    = nullptr;
	music_time_remain_   = nullptr;    
	music_subinfo_label_ = nullptr;
	music_next_line_     = nullptr;
    
    // Free the canvas buffer memory
    if (canvas_buffer_ != nullptr) {
        heap_caps_free(canvas_buffer_);
        canvas_buffer_ = nullptr;
        ESP_LOGI(TAG, "FFT canvas buffer freed");
    }
    
    // Reset canvas dimension variables
    canvas_width_ = 0;
    canvas_height_ = 0;
	
	// Ẩn các label nhạc nếu vẫn còn tồn tại (trường hợp canvas bị xóa nhưng label vẫn hiện)
    if (music_title_label_)   lv_obj_add_flag(music_title_label_, LV_OBJ_FLAG_HIDDEN);
    if (music_date_label_)    lv_obj_add_flag(music_date_label_, LV_OBJ_FLAG_HIDDEN);
    if (music_bar_)           lv_obj_add_flag(music_bar_, LV_OBJ_FLAG_HIDDEN);
    if (music_time_left_)     lv_obj_add_flag(music_time_left_, LV_OBJ_FLAG_HIDDEN);
    if (music_time_total_)    lv_obj_add_flag(music_time_total_, LV_OBJ_FLAG_HIDDEN);
    if (music_time_remain_)   lv_obj_add_flag(music_time_remain_, LV_OBJ_FLAG_HIDDEN);
    if (music_subinfo_label_) lv_obj_add_flag(music_subinfo_label_, LV_OBJ_FLAG_HIDDEN);
    if (music_next_line_)     lv_obj_add_flag(music_next_line_, LV_OBJ_FLAG_HIDDEN);

	
	// Hiện lại emoji khi thoát giao diện nhạc
    if (emoji_label_) lv_obj_clear_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    if (emoji_image_) lv_obj_clear_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    
    ESP_LOGI(TAG, "FFT display stopped, original UI restored");
}

void LcdDisplay::periodicUpdateTaskWrapper(void* arg) {
    auto self = static_cast<LcdDisplay*>(arg);
    self->periodicUpdateTask();
}

void LcdDisplay::periodicUpdateTask() {
    ESP_LOGI(TAG, "Periodic update task started");
    
    if (canvas_ == nullptr) {
        int status_h = lv_obj_get_height(status_bar_);
        create_canvas(status_h);

        if (canvas_) {
            // MỌI GỌI LVGL PHẢI DƯỚI LOCK
            {
                DisplayLockGuard lock(this);

				// Ẩn emoji khi chuyển sang giao diện phát nhạc (ẩn cả label và icon)
				if (emoji_label_) lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
				if (emoji_image_) lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);

                // Clear nền
                lv_canvas_fill_bg(canvas_, lv_color_black(), LV_OPA_COVER);

                // ================= UI BẮT ĐẦU (LVGL 9 - MODERN STYLE) =================
				// 1. Lấy trạng thái Player và Source
				Esp32SdMusic* sd_player = get_sd_player();
				bool sd_playing = sd_player &&
								  sd_player->getState() == Esp32SdMusic::PlayerState::Playing;

				DisplaySourceType source = DetectSourceFromInfo();

				// 2. Định nghĩa Palette màu sắc sống động (Vivid Colors)
				// Dùng màu Neon để nổi bật trên nền tối
				lv_color_t color_accent;
				const char* icon_symbol;

				switch (source) {
					case DisplaySourceType::SD_CARD:
						color_accent = lv_color_hex(0x00FFC2); // Emerald Neon (Xanh ngọc sáng)
						icon_symbol  = LV_SYMBOL_SD_CARD;
						break;
					case DisplaySourceType::RADIO:
						color_accent = lv_color_hex(0xFF9E40); // Sunset Orange (Cam rực rỡ)
						icon_symbol  = LV_SYMBOL_VOLUME_MAX; // Hoặc icon sóng
						break;
					case DisplaySourceType::ONLINE:
						color_accent = lv_color_hex(0x00D9FF); // Cyber Cyan (Xanh dương điện tử)
						icon_symbol  = LV_SYMBOL_AUDIO; // Hoặc icon Wifi/Cloud
						break;
					case DisplaySourceType::NONE:
					default:
						color_accent = lv_color_hex(0xFFFFFF);
						icon_symbol  = LV_SYMBOL_AUDIO;
						break;
				}

				// Chỉ hiển thị UI nếu có nguồn phát hợp lệ
				if (!(source == DisplaySourceType::NONE && !sd_playing))
				{
					auto theme      = static_cast<LvglTheme*>(current_theme_);
					auto text_font  = theme->text_font()->font();
					auto icon_font  = theme->large_icon_font()->font();

					// Kích thước cơ sở để tính toán tỉ lệ (Responsive)
					const int w = canvas_width_;
					const int h = canvas_height_;
					const int pad_side = (int)(w * 0.04f); // Lề 4% chiều rộng
					const int pad_top  = (int)(h * 0.05f); // Lề 5% chiều cao

					// --- ROOT CONTAINER ---
					music_root_ = lv_obj_create(canvas_);
					lv_obj_remove_style_all(music_root_);
					lv_obj_set_size(music_root_, w, h);
					lv_obj_set_style_bg_opa(music_root_, LV_OPA_TRANSP, 0);

					// =========================================================
					// (1) BACKGROUND OVERLAY: GRADIENT MỀM MẠI
					// Thay vì 2 khối cứng, dùng Gradient dọc để tan vào nền FFT
					// =========================================================
					lv_obj_t* overlay = lv_obj_create(music_root_);
					lv_obj_remove_style_all(overlay);
					lv_obj_set_size(overlay, w, (int)(h * 0.35f)); // Phủ 35% màn hình từ trên xuống
					
					// Tạo hiệu ứng mờ dần từ Đen (70%) -> Trong suốt
					lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
					lv_obj_set_style_bg_grad_color(overlay, lv_color_black(), 0);
					lv_obj_set_style_bg_grad_dir(overlay, LV_GRAD_DIR_VER, 0);
					lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);       // Nền chính
					lv_obj_set_style_bg_main_stop(overlay, 0, 0);            // Bắt đầu gradient ngay đỉnh
					lv_obj_set_style_bg_grad_stop(overlay, 255, 0);          // Kết thúc ở đáy object
					
					// Hack trong suốt bằng Map opacity (Nếu LVGL config hỗ trợ) 
					// Hoặc đơn giản dùng màu đen với OPA giảm dần. 
					// Cách an toàn nhất cho ESP32 ít tốn resource:
					lv_obj_set_style_bg_opa(overlay, 200, 0); // Độ đậm tổng thể

					// =========================================================
					// (2) ICON CHÍNH: CÂN ĐỐI VÀ NỔI BẬT
					// =========================================================
					lv_obj_t* icon = lv_label_create(music_root_);
					lv_obj_set_style_text_font(icon, icon_font, 0);
					lv_obj_set_style_text_color(icon, color_accent, 0);
					lv_label_set_text(icon, icon_symbol);
					
					// Đặt icon ở góc trên trái, nhưng căn chỉnh visual cho cân
					lv_obj_align(icon, LV_ALIGN_TOP_LEFT, pad_side, pad_top);

					// =========================================================
					// (3) XỬ LÝ NỘI DUNG (Content)
					// =========================================================
					
					// Biến tạm để lưu chuỗi hiển thị
					std::string title_str, sub_str;
					bool show_progress = false;

					// Lấy dữ liệu theo Source
					if (source == DisplaySourceType::SD_CARD && sd_playing) {
						title_str = sd_player->getCurrentTrack();
						if (title_str.empty()) title_str = "Unknown Track";
						
						int bitrate = sd_player->getBitrate();
						char buff[32];
						snprintf(buff, sizeof(buff), "%d kbps / MP3", bitrate/1000);
						sub_str = std::string(buff);
						show_progress = true;
					} else {
						// ================================
						// TÁCH 2 DÒNG TỪ music_info_
						// ================================
						std::string line1, line2;
						{
							size_t pos = music_info_.find('\n');
							if (pos != std::string::npos) {
								line1 = music_info_.substr(0, pos);
								line2 = music_info_.substr(pos + 1);
							} else {
								// Nếu không có xuống dòng → lấy nguyên chuỗi
								line1 = music_info_;
								line2 = "";
							}
						}

						// --- Title ---
						title_str = line1.empty()
									? (source == DisplaySourceType::ONLINE ? "Music Online" : "FM Radio")
									: line1;

						// --- Sub info ---
						if (source == DisplaySourceType::ONLINE) {
							// GIỮ nguyên line2 nếu đã được SetMusicInfo gửi xuống
							sub_str = !line2.empty() ? line2 : "Đang phát...";
						}
						else if (source == DisplaySourceType::RADIO) {
							// GIỮ nguyên line2 nếu đã được SetRadioInfo gửi xuống
							sub_str = !line2.empty() ? line2 : "Live Broadcast";
						}

						show_progress = false;
					}

					// --- TITLE (TIÊU ĐỀ) ---
					lv_obj_t* title = lv_label_create(music_root_);
					lv_obj_set_style_text_font(title, text_font, 0);
					lv_obj_set_style_text_color(title, lv_color_white(), 0);
					lv_label_set_long_mode(title, LV_LABEL_LONG_SCROLL_CIRCULAR); // Cuộn tròn nếu dài
					
					// Tính toán chiều rộng để không đè lên icon và lề phải
					// W = Màn hình - (Lề trái + Icon + Khoảng cách icon-text) - Lề phải
					int icon_width = 30; // Ước lượng, hoặc dùng lv_obj_get_width sau khi vẽ
					int text_width = w - (pad_side + icon_width + pad_side) - pad_side; 
					
					lv_obj_set_width(title, text_width);
					lv_label_set_text(title, title_str.c_str());
					
					// Căn title thẳng hàng với đỉnh của icon
					lv_obj_align_to(title, icon, LV_ALIGN_OUT_RIGHT_TOP, pad_side, 0);
					music_title_label_ = title;

					// --- SUB INFO (THÔNG TIN PHỤ) ---
					lv_obj_t* sub = lv_label_create(music_root_);
					lv_obj_set_style_text_font(sub, text_font, 0);
					// Màu xám sáng để dịu mắt hơn trắng tinh, tạo chiều sâu
					lv_obj_set_style_text_color(sub, lv_color_hex(0xAAAAAA), 0); 
					lv_label_set_text(sub, sub_str.c_str());
					lv_label_set_long_mode(sub, LV_LABEL_LONG_SCROLL_CIRCULAR);
					lv_obj_set_width(sub, canvas_width_ - 40);
					
					// Nằm ngay dưới Title
					lv_obj_align_to(sub, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4); // Cách title 4px
					music_subinfo_label_ = sub;

					// =========================================================
					// (4) PROGRESS BAR & NEXT TRACK (CHỈ CHO SD CARD)
					// =========================================================
					if (show_progress) {
						int64_t pos_ms = sd_player->getCurrentPositionMs();
						int64_t dur_ms = sd_player->getDurationMs();
						
						// --- THANH TIẾN ĐỘ TINH TẾ (SLIM BAR) ---
						lv_obj_t* bar = lv_bar_create(music_root_);
						lv_obj_set_size(bar, w - (pad_side * 2), 4); // Cao 4px cho thanh mảnh
						lv_obj_align_to(bar, sub, LV_ALIGN_OUT_BOTTOM_LEFT, -icon_width - pad_side, 12); // Cách sub 12px

						// Style Nền Bar (Tối)
						lv_obj_set_style_bg_color(bar, lv_color_hex(0x303030), LV_PART_MAIN);
						lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);

						// Style Indicator (Màu theo Accent)
						lv_obj_set_style_bg_color(bar, color_accent, LV_PART_INDICATOR);
						lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
						lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
						
						// Hiệu ứng bóng đổ nhẹ cho Indicator (Glow effect giả lập)
						// lv_obj_set_style_shadow_width(bar, 5, LV_PART_INDICATOR);
						// lv_obj_set_style_shadow_color(bar, color_accent, LV_PART_INDICATOR);

						lv_bar_set_range(bar, 0, dur_ms);
						lv_bar_set_value(bar, pos_ms, LV_ANIM_OFF);
						music_bar_ = bar;

						// --- THỜI GIAN (Time Labels) ---
						// Current Time (Bên trái Bar)
						lv_obj_t* t_curr = lv_label_create(music_root_);
						lv_obj_set_style_text_font(t_curr, text_font, 0);
						lv_obj_set_style_text_color(t_curr, color_accent, 0); // Màu giống icon cho đồng bộ
						lv_label_set_text(t_curr, sd_player->getCurrentTimeString().c_str());
						lv_obj_align_to(t_curr, bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
						music_time_left_ = t_curr;

						// Duration / Remaining (Bên phải Bar)
						lv_obj_t* t_dur = lv_label_create(music_root_);
						lv_obj_set_style_text_font(t_dur, text_font, 0);
						lv_obj_set_style_text_color(t_dur, lv_color_hex(0xAAAAAA), 0);
						lv_label_set_text(t_dur, sd_player->getDurationString().c_str()); // Hoặc tính remain
						lv_obj_align_to(t_dur, bar, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 6);
						music_time_remain_ = t_dur; // Tái sử dụng pointer này

						// --- NEXT TRACK INFO (Dòng nhỏ cuối cùng) ---
						// Lấy tên bài tiếp theo
						auto tracks = sd_player->listTracks();
						std::string cur_path = sd_player->getCurrentTrackPath();
						int idx = -1; 
						for(size_t i=0; i<tracks.size(); ++i) if(tracks[i].path == cur_path) idx = i;
						
						std::string next_txt = "End of playlist";
						if(idx >= 0 && idx < (int)tracks.size() - 1) next_txt = tracks[idx+1].name;
						else if(!tracks.empty()) next_txt = tracks[0].name; // Loop về bài đầu

						lv_obj_t* next_lbl = lv_label_create(music_root_);
						lv_obj_set_style_text_font(next_lbl, text_font, 0);
						lv_obj_set_style_text_color(next_lbl, lv_color_hex(0x707070), 0); // Màu tối hơn để ít gây chú ý
						
						char next_buff[128];
						snprintf(next_buff, sizeof(next_buff), "Next: %s", next_txt.c_str());
						lv_label_set_text(next_lbl, next_buff);
						lv_label_set_long_mode(next_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
						lv_obj_set_width(next_lbl, w - pad_side * 2);
						
						// Đặt ở vị trí cân đối phía dưới
						lv_obj_align_to(next_lbl, t_curr, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
						music_next_line_ = next_lbl;
					} 
					else {
						// Reset các pointer không dùng cho Online/Radio để tránh lỗi update
						music_bar_ = nullptr;
						music_time_left_ = nullptr;
						music_time_remain_ = nullptr;
						music_next_line_ = nullptr;
						music_time_total_ = nullptr;
					}
				}
				// ================= UI KẾT THÚC =================

                lv_obj_invalidate(canvas_);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    } else {
        ESP_LOGI(TAG, "Canvas already created");
    }
  
    const TickType_t displayInterval      = pdMS_TO_TICKS(25);  // Display refresh interval (25ms)
    const TickType_t audioProcessInterval = pdMS_TO_TICKS(10);  // Audio processing interval (10ms)
    
    TickType_t lastDisplayTime = xTaskGetTickCount();
    TickType_t lastAudioTime   = xTaskGetTickCount();
    
    while (!fft_task_should_stop) {
        
        TickType_t currentTime = xTaskGetTickCount();
        
        // Process audio data at regular intervals
        if (currentTime - lastAudioTime >= audioProcessInterval) {
            if (final_pcm_data_fft != nullptr) {
                processAudioData();  // Quick processing, non-blocking
            } else {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            lastAudioTime = currentTime;
        }
        
        // Display refresh (30Hz)
        if (currentTime - lastDisplayTime >= displayInterval) {
            if (fft_data_ready) {
                DisplayLockGuard lock(this);
                drawSpectrumIfReady();
                lv_area_t refresh_area;
                refresh_area.x1 = 0;
                refresh_area.y1 = height_ - bar_max_hight_;
                refresh_area.x2 = canvas_width_ - 1;
                refresh_area.y2 = height_ - 1; // Only refresh the spectrum area
                lv_obj_invalidate_area(canvas_, &refresh_area);
                fft_data_ready = false;
                lastDisplayTime = currentTime;
            }
        }
        
        // ================================
        // 🟦 UPDATE MUSIC UI (mỗi 1 giây)
        // ================================
        static TickType_t lastClockUpdate = 0;
        if (currentTime - lastClockUpdate >= pdMS_TO_TICKS(1000)) {

            Esp32SdMusic* sd = get_sd_player();

            if (sd &&
                music_root_        && lv_obj_is_valid(music_root_) &&
                music_bar_         && lv_obj_is_valid(music_bar_))
            {
                DisplayLockGuard lock(this);

                // Cập nhật progress bar
                lv_bar_set_range(music_bar_, 0, sd->getDurationMs());
                lv_bar_set_value(music_bar_, sd->getCurrentPositionMs(), LV_ANIM_OFF);

                // Thời gian hiện tại
                if (music_time_left_ && lv_obj_is_valid(music_time_left_)) {
                    std::string cur = sd->getCurrentTimeString();
                    lv_label_set_text(music_time_left_, cur.c_str());
                }

                // Thời gian còn lại
                if (music_time_remain_ && lv_obj_is_valid(music_time_remain_)) {
					int64_t pos = sd->getCurrentPositionMs();
					int64_t dur = sd->getDurationMs();
					int64_t rem = dur - pos;
					if (rem < 0) rem = 0;

					std::string remain_str = ms_to_time_string(rem);
					lv_label_set_text(music_time_remain_, remain_str.c_str());
				}

                // Tên bài hát (nếu chuyển bài)
                if (music_title_label_ && lv_obj_is_valid(music_title_label_)) {
                    std::string t = sd->getCurrentTrack();
                    if (!t.empty()) {
                        lv_label_set_text(music_title_label_, t.c_str());
                    }
                }

                // Nếu sau này bạn có dùng music_date_label_ thì block này vẫn an toàn
                if (music_date_label_ && lv_obj_is_valid(music_date_label_)) {
                    time_t now = time(NULL);
                    struct tm tm_info;
                    localtime_r(&now, &tm_info);

                    char buf[32];
                    strftime(buf, sizeof(buf), "%d-%m-%Y", &tm_info);
                    lv_label_set_text(music_date_label_, buf);
                }
            
				// --- cập nhật bitrate + tổng thời lượng ---
				if (music_subinfo_label_ && lv_obj_is_valid(music_subinfo_label_)) {

					int br = sd->getBitrate();   // lấy bitrate gốc (thường = 128000)
					if (br > 1000) br /= 1000;   // chuyển bps → kbps (128000 → 128)

					char sub_text[64];
					snprintf(sub_text, sizeof(sub_text),
							 "%d kbps  •  %s",
							 br,
							 sd->getDurationString().c_str());

					lv_label_set_text(music_subinfo_label_, sub_text);
					lv_label_set_long_mode(music_subinfo_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
					lv_obj_set_width(music_subinfo_label_, canvas_width_ - 40);

				}
				
				// --- cập nhật dòng "Tiếp theo: ..." ---
				if (music_next_line_ && lv_obj_is_valid(music_next_line_)) {

					auto list = sd->listTracks();

					// tìm index hiện tại qua path
					std::string cur_path = sd->getCurrentTrackPath();
					int cur = 0;
					for (int i = 0; i < (int)list.size(); i++) {
						if (list[i].path == cur_path) {
							cur = i;
							break;
						}
					}

					int total = list.size();
					int next = (cur + 1) % total;

					std::string next_title =
						(next < total) ? list[next].name : "Không có bài kế tiếp";

					std::string tip = "Tiếp theo: " + next_title;
					lv_label_set_text(music_next_line_, tip.c_str());
				}

			}		
			
            lastClockUpdate = currentTime;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Short delay
    }
    
    ESP_LOGI(TAG, "FFT display task stopped");
    fft_task_handle = nullptr;  // Clear the task handle
    vTaskDelete(NULL);  // Delete the current task
}

void LcdDisplay::create_canvas(int32_t status_bar_height) {
    DisplayLockGuard lock(this);
    if (canvas_ != nullptr) {
        lv_obj_del(canvas_);
    }
    if (canvas_buffer_ != nullptr) {
        heap_caps_free(canvas_buffer_);
        canvas_buffer_ = nullptr;
    }

    // int status_bar_height=lv_obj_get_height(status_bar_);
    ESP_LOGI(TAG, "Status bar height: %d", status_bar_height);
    canvas_width_=width_;
    canvas_height_=height_-status_bar_height;
    ESP_LOGI(TAG, "Creating canvas with width: %d, height: %d", canvas_width_, canvas_height_);

    canvas_buffer_=(uint16_t*)heap_caps_malloc(canvas_width_ * canvas_height_ * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (canvas_buffer_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate canvas buffer");
        return;
    }
    ESP_LOGI(TAG, "canvas buffer allocated successfully");  
    canvas_ = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(canvas_, canvas_buffer_, canvas_width_, canvas_height_, LV_COLOR_FORMAT_RGB565);
    ESP_LOGI(TAG,"width: %d, height: %d", width_, height_);

    

    lv_obj_set_pos(canvas_, 0, status_bar_height);
    lv_obj_set_size(canvas_, canvas_width_, canvas_height_);
    lv_canvas_fill_bg(canvas_, lv_color_make(0, 0, 0), LV_OPA_TRANSP);
    lv_obj_move_foreground(canvas_);
    ESP_LOGI(TAG, "canvas created successfully");  
}

void LcdDisplay::drawSpectrumIfReady() {
    if (fft_data_ready) {
        draw_spectrum(avg_power_spectrum, LCD_FFT_SIZE/2);
        fft_data_ready = false;
    }
}

void LcdDisplay::draw_spectrum(float *power_spectrum,int fft_size){
    const int bartotal=BAR_COL_NUM;
    int bar_height;
    const int bar_max_height = bar_max_hight_;
    const int bar_width=canvas_width_/bartotal;
    int x_pos=0;
    int y_pos = (canvas_height_) - 1;

    float magnitude[bartotal]={0};
    float max_magnitude=0;

    const float MIN_DB = -25.0f;
    const float MAX_DB = 0.0f;
    
    for (int bin = 0; bin < bartotal; bin++) {
        int start = bin * (fft_size / bartotal);
        int end = (bin+1) * (fft_size / bartotal);
        magnitude[bin] = 0;
        int count=0;
        for (int k = start; k < end; k++) {
            magnitude[bin] += sqrt(power_spectrum[k]);
            count++;
        }
        if(count>0){
            magnitude[bin] /= count;
        }
      

        if (magnitude[bin] > max_magnitude) max_magnitude = magnitude[bin];
    }


    magnitude[1]=magnitude[1]*0.6;
    magnitude[2]=magnitude[2]*0.7;
    magnitude[3]=magnitude[3]*0.8;
    magnitude[4]=magnitude[4]*0.8;
    magnitude[5]=magnitude[5]*0.9;

    for (int bin = 1; bin < bartotal; bin++) {
        
        if (magnitude[bin] > 0.0f && max_magnitude > 0.0f) {
            // Relative dB value: 20 * log10(magnitude/ref_level)
            magnitude[bin] = 20.0f * log10f(magnitude[bin] / max_magnitude+ 1e-10);
        } else {
            magnitude[bin] = MIN_DB;
        }
        if (magnitude[bin] > max_magnitude) max_magnitude = magnitude[bin];
    }

    std::fill_n(canvas_buffer_, canvas_width_ * canvas_height_, COLOR_BLACK);
    
    // Skip the DC component (k=0)
    for (int k = 1; k < bartotal; k++) {
        x_pos=canvas_width_/bartotal*(k-1);
        float mag=(magnitude[k] - MIN_DB) / (MAX_DB - MIN_DB);
        mag = std::max(0.0f, std::min(1.0f, mag));
        bar_height=int(mag*(bar_max_height));
        
        int color=get_bar_color(k);
        draw_bar(x_pos,y_pos,bar_width,bar_height, color,k-1);
        //printf("x: %d, y: %d,\n", x_pos, bar_height);
    }

}

int16_t* LcdDisplay::MakeAudioBuffFFT(size_t sample_count) {
    if (final_pcm_data_fft == nullptr) {
        final_pcm_data_fft = (int16_t *)heap_caps_malloc( sample_count, MALLOC_CAP_SPIRAM);
    }
    return final_pcm_data_fft;
}

void LcdDisplay::FeedAudioDataFFT(int16_t* data, size_t sample_count) {
    // Copy PCM data for FFT display
    memcpy( final_pcm_data_fft, data, sample_count);
}

void LcdDisplay::ReleaseAudioBuffFFT(int16_t* buffer) {
    if (final_pcm_data_fft != nullptr) {
        heap_caps_free(final_pcm_data_fft);
        final_pcm_data_fft = nullptr;
    }
}

void LcdDisplay::processAudioData() {
    if(final_pcm_data_fft != nullptr) {
        if(audio_display_last_update <= 2) {
            memcpy(audio_data_, final_pcm_data_fft, sizeof(int16_t) * 1152);
            for(int i = 0; i < 1152; i++) {
                frame_audio_data[i] += audio_data_[i];
            }
            audio_display_last_update++;
        } else {
            const int HOP_SIZE = LCD_FFT_SIZE;
            const int NUM_SEGMENTS = 1 + (1152 - LCD_FFT_SIZE) / HOP_SIZE;

            for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
                int start = seg * HOP_SIZE;
                if (start + LCD_FFT_SIZE > 1152) break;

                // Prepare the current segment data
                for (int i = 0; i < LCD_FFT_SIZE; i++) {
                    int idx = start + i;
                    float sample = frame_audio_data[idx] / 32768.0f;
                    fft_real[i] = sample * hanning_window_float[i];
                    fft_imag[i] = 0.0f;
                }

                compute(fft_real, fft_imag, LCD_FFT_SIZE, true);

                // Calculate and accumulate the power spectrum (double-sided)
                for (int i = 0; i < LCD_FFT_SIZE / 2; i++) {
                    avg_power_spectrum[i] += fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i]; // Power = amplitude squared
                }
            }

            // Compute the average
            for (int i = 0; i < LCD_FFT_SIZE / 2; i++) {
                avg_power_spectrum[i] /= NUM_SEGMENTS;
            }

            audio_display_last_update = 0;
            fft_data_ready = true;
            memset(frame_audio_data, 0, sizeof(int16_t) * 1152);
        }
    } else {
        ESP_LOGI(TAG, "audio_data_ is nullptr");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void LcdDisplay::draw_bar(int x,int y,int bar_width,int bar_height,uint16_t color,int bar_index){

    const int block_space=2;
    const int block_x_size=bar_width-block_space;
    const int block_y_size=4;
    
    int blocks_per_col=(bar_height/(block_y_size+block_space));
    int start_x=(block_x_size+block_space)/2+x;
    
    if(current_heights[bar_index]<bar_height) 
    {
        current_heights[bar_index]=bar_height;
    }
    else{
        int fall_speed=2;
        current_heights[bar_index]=current_heights[bar_index]-fall_speed;
        if(current_heights[bar_index]>(block_y_size+block_space)) 
        draw_block(start_x,canvas_height_-current_heights[bar_index],block_x_size,block_y_size,color,bar_index);

    }
   
    draw_block(start_x,canvas_height_-1,block_x_size,block_y_size,color,bar_index);

    for(int j=1;j<blocks_per_col;j++){
        
        int start_y=j*(block_y_size+block_space);
        draw_block(start_x,canvas_height_-start_y,block_x_size,block_y_size,color,bar_index); 
        
    }
}

void LcdDisplay::draw_block(int x,int y,int block_x_size,int block_y_size,uint16_t color,int bar_index){
    for (int row = y; row > y-block_y_size;row--) {
        // Draw one row at a time
        uint16_t* line_start = &canvas_buffer_[row * canvas_width_ + x];
        std::fill_n(line_start, block_x_size, color);
    }
}

void LcdDisplay::compute(float* real, float* imag, int n, bool forward) {
    // Bit-reversal permutation
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (j > i) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
        
        int m = n >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }

    // FFT computation
    for (int s = 1; s <= (int)log2(n); s++) {
        int m = 1 << s;
        int m2 = m >> 1;
        float w_real = 1.0f;
        float w_imag = 0.0f;
        float angle = (forward ? -2.0f : 2.0f) * M_PI / m;
        float wm_real = cosf(angle);
        float wm_imag = sinf(angle);
        
        for (int j = 0; j < m2; j++) {
            for (int k = j; k < n; k += m) {
                int k2 = k + m2;
                float t_real = w_real * real[k2] - w_imag * imag[k2];
                float t_imag = w_real * imag[k2] + w_imag * real[k2];
                
                real[k2] = real[k] - t_real;
                imag[k2] = imag[k] - t_imag;
                real[k] += t_real;
                imag[k] += t_imag;
            }
            
            float w_temp = w_real;
            w_real = w_real * wm_real - w_imag * wm_imag;
            w_imag = w_temp * wm_imag + w_imag * wm_real;
        }
    }
    
    // Scale for forward transform
    if (forward) {
        for (int i = 0; i < n; i++) {
            real[i] /= n;
            imag[i] /= n;
        }
    }
}

uint16_t LcdDisplay::get_bar_color(int x_pos) {
    // Tăng offset theo thời gian để màu chạy
    static float hue_offset = 0.0f;
    hue_offset += 0.1f;       // tốc độ chạy màu (có thể chỉnh 0.3 → 2.0)

    if (hue_offset >= 360.0f)
        hue_offset -= 360.0f;

    // Hue tổng = hue theo cột + offset
    float base_hue = (float)x_pos * (240.0f / 40.0f);   // blue → red
    float h = fmodf(base_hue + hue_offset, 360.0f);     // hue chạy

    float s = 1.0f;
    float v = 1.0f;
    float c = v * s;

    float hh = h / 60.0f;
    float x = c * (1.0f - fabsf(fmodf(hh, 2.0f) - 1.0f));

    float r1 = 0, g1 = 0, b1 = 0;

    int region = (int)hh;
    switch (region) {
        case 0: r1 = c; g1 = x; b1 = 0; break;
        case 1: r1 = x; g1 = c; b1 = 0; break;
        case 2: r1 = 0; g1 = c; b1 = x; break;
        case 3: r1 = 0; g1 = x; b1 = c; break;
        case 4: r1 = x; g1 = 0; b1 = c; break;
        default:r1 = c; g1 = 0; b1 = x; break;
    }

    float m = v - c;
    float rf = r1 + m;
    float gf = g1 + m;
    float bf = b1 + m;

    uint8_t r = (uint8_t)(rf * 31);
    uint8_t g = (uint8_t)(gf * 63);
    uint8_t b = (uint8_t)(bf * 31);

    return (r << 11) | (g << 5) | b;
}

void LcdDisplay::DisplayQRCode(const uint8_t* qrcode, const char* text) {
    DisplayLockGuard lock(this);
    if (content_ == nullptr || qrcode == nullptr) {
        return;
    }

    // Get QR code size
    int qr_size = esp_qrcode_get_size(qrcode);
    ESP_LOGI(TAG, "QR code size: %d, text: %s", qr_size, text != nullptr ? text : "123456789");

    // Calculate display size (scale QR code to fit screen)
    int max_size = (width_ < height_ ? width_ : height_) * 70 / 100;
    int pixel_size = max_size / qr_size;
    if (pixel_size < 2) pixel_size = 2; // Minimum 2 pixels per module
    ESP_LOGI(TAG, "QR code pixel size: %d", pixel_size);

    create_canvas(lv_obj_get_height(status_bar_));
    lv_canvas_fill_bg(canvas_, lv_color_make(0xFF, 0xFF, 0xFF), LV_OPA_COVER);
    // Initialize layer for drawing
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    // Setup rect descriptor for QR code modules
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_black();
    rect_dsc.bg_opa = LV_OPA_COVER;
    
    // Draw QR code modules
    int qr_pos_x = (canvas_width_ - qr_size * pixel_size) / 2;
    int qr_pos_y = (canvas_height_ - qr_size * pixel_size) / 2;
    for (int y = 0; y < qr_size; y++) {
        for (int x = 0; x < qr_size; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                // Draw black module
                lv_area_t coords_rect;
                coords_rect.x1 = x * pixel_size + qr_pos_x;
                coords_rect.y1 = y * pixel_size + qr_pos_y;
                coords_rect.x2 = (x + 1) * pixel_size - 1 + qr_pos_x;
                coords_rect.y2 = (y + 1) * pixel_size - 1 + qr_pos_y;
                
                lv_draw_rect(&layer, &rect_dsc, &coords_rect);
            }
        }
    }

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_palette_main(LV_PALETTE_ORANGE);
    label_dsc.text = text != nullptr ? text : ip_address_.c_str();
    int th = lv_font_get_line_height(label_dsc.font);
    int32_t text_pos_y = canvas_height_ - 20;
    text_pos_y =  canvas_height_ - qr_pos_y + (qr_pos_y - th) / 2;
    ESP_LOGI(TAG, "Canvas w: %d, h: %d, text y pos: %d", canvas_width_, canvas_height_, text_pos_y);
    lv_area_t coords_text = {qr_pos_x, text_pos_y, canvas_width_ -1, canvas_height_ - 1};
    lv_draw_label(&layer, &label_dsc, &coords_text);

    // lv_draw_label_dsc_t label_dsc;
    // lv_draw_label_dsc_init(&label_dsc);
    // label_dsc.align = LV_TEXT_ALIGN_CENTER;
    // label_dsc.text = text != nullptr ? text : ip_address_.c_str();
    // ESP_LOGI(TAG, "Drawing text: %s", label_dsc.text);
    // label_dsc.color = lv_palette_main(LV_PALETTE_ORANGE);

    // int canvas_w = lv_obj_get_width(canvas_);
    // int canvas_h = lv_obj_get_height(canvas_);
    // int th = lv_font_get_line_height(label_dsc.font);
    // int32_t text_pos_y =  canvas_h - qr_pos_y + (qr_pos_y - th) / 2;
    // ESP_LOGI(TAG, "Canvas w: %d, h: %d, text y pos: %d", canvas_w, canvas_h, text_pos_y);
    // // lv_area_t coords_text = { 0, (canvas_h - 20), canvas_w, canvas_h};
    // lv_area_t coords_text = { 0, (canvas_height_ - 20), canvas_width_, canvas_height_};
    // lv_draw_label(&layer, &label_dsc, &coords_text);
    
    // Finish layer
    lv_canvas_finish_layer(canvas_, &layer);
    ESP_LOGI(TAG, "QR code drawn on canvas");
    qr_code_displayed_ = true;
}

bool LcdDisplay::QRCodeIsSupported() {
    return true;
}

void LcdDisplay::ClearQRCode() {
    if (!qr_code_displayed_) {
        return;
    }

    qr_code_displayed_ = false;
    DisplayLockGuard lock(this);
    if (canvas_ != nullptr) {
        ESP_LOGI(TAG, "Clearing QR code from canvas");
        lv_obj_del(canvas_);
        canvas_ = nullptr;
    }

    // Free the canvas buffer memory
    if (canvas_buffer_ != nullptr) {
        heap_caps_free(canvas_buffer_);
        canvas_buffer_ = nullptr;
        ESP_LOGI(TAG, "FFT canvas buffer freed");
    }
}

void LcdDisplay::SetIpAddress(const std::string& ip_address) {
    ip_address_ = ip_address;
    ESP_LOGI(TAG, "IP address set to: %s", ip_address_.c_str());
}

void LcdDisplay::SetRotationAndOffset(lv_display_rotation_t rotation, int offset_x, int offset_y) {
    DisplayLockGuard lock(this);
    lv_display_set_rotation(display_, rotation);
    lv_display_set_offset(display_, offset_x, offset_y);
}

bool LcdDisplay::SetRotation(int rotation_degree, bool save_setting) {
    if (rotation_degree_ == rotation_degree) {
        return true; // No change needed
    }
    rotation_degree_ = rotation_degree;
    switch (rotation_degree) {
        case 0:
            SetRotationAndOffset(LV_DISPLAY_ROTATION_0, 0, 0);
            break;
        case 90:
            SetRotationAndOffset(LV_DISPLAY_ROTATION_90, (height_ == width_) ? 80 : 0, 0);
            break;
        case 180:
            SetRotationAndOffset(LV_DISPLAY_ROTATION_180, 0, (height_ == width_) ? 80 : 0);
            break;
        case 270:
            SetRotationAndOffset(LV_DISPLAY_ROTATION_270, 0, 0);
            break;
        default:
            ESP_LOGW(TAG, "Unsupported rotation degree: %d", rotation_degree);
            return false;
    }

    if (!save_setting) {
        return true;
    }
    Settings settings("display", true);
    settings.SetInt("rotation_degree", rotation_degree);
    return true;
}