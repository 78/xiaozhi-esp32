#include "oled_display.h"
#include "assets/lang_config.h"
#include "lvgl_theme.h"
#include "lvgl_font.h"
#include "face_engine.h"

#include <string>
#include <algorithm>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <font_awesome.h>

#define TAG "OledDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_1);

// Handhaaf de face engine pointer van de maker
FaceEngine* face_engine_ = nullptr;

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_awesome_30_1);
    
    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("dark", dark_theme);
    current_theme_ = dark_theme;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }
}

void OledDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }
    
    Display::SetupUI();
    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
}

OledDisplay::~OledDisplay() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }
    if (face_engine_ != nullptr) {
        delete face_engine_;
        face_engine_ = nullptr;
    }
    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    // Leeggelaten conform plugin intentie
}

void OledDisplay::SetEmotion(const char* emotion) {
    // Leeggelaten conform plugin intentie
}

void OledDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);
    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();
    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
}

void OledDisplay::SetupUI_128x32() {
    // Fallback mocht het scherm toch 32px hoog zijn
    DisplayLockGuard lock(this);
    auto screen = lv_screen_active();
    face_engine_ = new FaceEngine();
    face_engine_->Init(screen);
}

void OledDisplay::SetStatus(const char* status) {
    if (!face_engine_)
        return;

    if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        face_engine_->SetState(FaceState::Idle);
    } else if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        face_engine_->SetState(FaceState::Listening);
    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        face_engine_->SetState(FaceState::Speaking);
    } else if (strcmp(status, "thinking") == 0 || strcmp(status, "Thinking") == 0) {
        // Vangt de AI-denkstatus op en triggert jouw 23 frames loop
        face_engine_->SetState(FaceState::Thinking);
    } else if (strcmp(status, "focus") == 0 || strcmp(status, "Focus") == 0) {
        // Vangt de focus-status op en start de eenmalige 27-frame animatie
        face_engine_->SetState(FaceState::Focus);
    } else {
        face_engine_->SetState(FaceState::Idle);
    }
}


// // exact overgenomen van de maker, inclusief de container_ fix
// void OledDisplay::SetupUI_128x64() {
//     DisplayLockGuard lock(this);

//     auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
//     auto text_font = lvgl_theme->text_font()->font();
//     auto icon_font = lvgl_theme->icon_font()->font();
//     auto large_icon_font = lvgl_theme->large_icon_font()->font();

//     auto screen = lv_screen_active();
//     lv_obj_set_style_text_font(screen, text_font, 0);
//     lv_obj_set_style_text_color(screen, lv_color_black(), 0);

//     /* Container */
//     container_ = lv_obj_create(screen);
//     lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
//     lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
//     lv_obj_set_style_pad_all(container_, 0, 0);
//     lv_obj_set_style_border_width(container_, 0, 0);
//     lv_obj_set_style_pad_row(container_, 0, 0);

//     /* Content */
//     content_ = lv_obj_create(container_);
//     lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
//     lv_obj_set_style_radius(content_, 0, 0);
//     lv_obj_set_style_pad_all(content_, 0, 0);
//     lv_obj_set_width(content_, LV_HOR_RES);
//     lv_obj_set_size(content_, 128, 48);
//     lv_obj_set_flex_grow(content_, 1);

//     lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
//     lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
//                           LV_FLEX_ALIGN_CENTER);

//     face_engine_ = new FaceEngine();
//     face_engine_->Init(content_);
// }


void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();

    auto screen = lv_screen_active();

    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_text_font(screen, text_font, 0);

    face_engine_ = new FaceEngine();
    face_engine_->Init(screen);
    
    face_engine_->SetState(FaceState::Idle);
    lv_obj_invalidate(screen);
}