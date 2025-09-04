
#include <cstring>
#include <memory>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "display/lcd_display.h"
#include "assets/lang_config.h"
#include "emote_display.h"


namespace anim {

static const char* TAG = "emoji";

bool EmoteDisplay::OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io,
                                 esp_lcd_panel_io_event_data_t* edata,
                                 void* user_ctx)
{
    return true;
}

void EmoteDisplay::OnFlush(gfx_handle_t handle, int x_start, int y_start,
                          int x_end, int y_end, const void* color_data)
{
    auto* panel = static_cast<esp_lcd_panel_handle_t>(gfx_emote_get_user_data(handle));
    if (panel) {
        esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);
    }
    gfx_emote_flush_ready(handle, true);
}

void EmoteDisplay::SetUIDisplayMode(UIDisplayMode mode)
{
    gfx_obj_set_visible(obj_anim_listen_, false);
    gfx_obj_set_visible(obj_label_clock_, false);
    gfx_obj_set_visible(obj_label_toast_, false);

    // Show the selected control
    switch (mode) {
    case UIDisplayMode::SHOW_ANIM_LISTENING:
        gfx_obj_set_visible(obj_anim_listen_, true);
        break;
    case UIDisplayMode::SHOW_CLOCK:
        gfx_obj_set_visible(obj_label_clock_, true);
        break;
    case UIDisplayMode::SHOW_TOAST:
        gfx_obj_set_visible(obj_label_toast_, true);
        break;
    }
}


void EmoteDisplay::SetImageDesc(gfx_image_dsc_t* img_dsc, int asset_id)
{
    const void* img_data = mmap_assets_get_mem(assets_handle_, asset_id);
    size_t img_size = mmap_assets_get_size(assets_handle_, asset_id);

    std::memcpy(&img_dsc->header, img_data, sizeof(gfx_image_header_t));
    img_dsc->data = static_cast<const uint8_t*>(img_data) + sizeof(gfx_image_header_t);
    img_dsc->data_size = img_size - sizeof(gfx_image_header_t);
}

EmoteDisplay::EmoteDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, DisplayFonts fonts, mmap_assets_handle_t assets_handle, const EmoteDisplayConfig& config)
    : panel_io_(panel_io), 
      panel_(panel), 
      assets_handle_(assets_handle), 
      current_icon_type_(0),
      fonts_(fonts), 
      config_(config)
{
    ESP_LOGI(TAG, "EmoteDisplay base constructor, width: %d, height: %d", width, height);
}

SPIEmoteDisplay::SPIEmoteDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                 int width, int height,
                                 DisplayFonts fonts, mmap_assets_handle_t assets_handle,
                                 const EmoteDisplayConfig& config)
    : EmoteDisplay(panel_io, panel, width, height, fonts, assets_handle, config)
{
    ESP_LOGI(TAG, "initializing GFX engine and UI components");

    // Initialize GFX engine using config parameters
    gfx_core_config_t gfx_cfg = {
        .flush_cb = EmoteDisplay::OnFlush,
        .user_data = panel_,
        .flags = {
            .swap = true,
            .double_buffer = true,
            .buff_dma = true,
        },
        .h_res = static_cast<uint32_t>(width),
        .v_res = static_cast<uint32_t>(height),
        .fps = 30,
        .buffers = {
            .buf1 = nullptr,
            .buf2 = nullptr,
            .buf_pixels = config.gfx_config.buf_pixels > 0 ? 
                         config.gfx_config.buf_pixels : 
                         static_cast<size_t>(width * 16),
        },
        .task = GFX_EMOTE_INIT_CONFIG()
    };

    gfx_cfg.task.task_stack_caps = MALLOC_CAP_DEFAULT;
    gfx_cfg.task.task_affinity = 0;
    gfx_cfg.task.task_priority = 5;
    gfx_cfg.task.task_stack = config.gfx_config.task_stack_size;
    engine_handle_ = gfx_emote_init(&gfx_cfg);

    // Register callbacks
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = EmoteDisplay::OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io_, &cbs, engine_handle_);
    
    SetupUI(width, height);
}

void EmoteDisplay::SetupUI(int width, int height)
{
    DisplayLockGuard lock(this);

    gfx_emote_set_bg_color(engine_handle_, GFX_COLOR_HEX(0x000000));
    
    // Initialize eye animation
    int idle_anim_id = GetEmotionId("idle");
    const void* eye_anim_data = mmap_assets_get_mem(assets_handle_, idle_anim_id);
    size_t eye_anim_size = mmap_assets_get_size(assets_handle_, idle_anim_id);

    obj_anim_eye_ = gfx_anim_create(engine_handle_);
    gfx_anim_set_src(obj_anim_eye_, eye_anim_data, eye_anim_size);
    gfx_obj_align(obj_anim_eye_, config_.layout.eye_anim.align, config_.layout.eye_anim.x, config_.layout.eye_anim.y);
    gfx_anim_set_auto_mirror(obj_anim_eye_, true);
    gfx_anim_set_segment(obj_anim_eye_, 0, 0xFFFF, 20, false);
    gfx_anim_start(obj_anim_eye_);

    // Initialize icon
    int wifi_failed_icon_id = GetIconId("error");
    obj_img_status_ = gfx_img_create(engine_handle_);
    gfx_obj_align(obj_img_status_, config_.layout.status_icon.align, config_.layout.status_icon.x, config_.layout.status_icon.y);
    SetImageDesc(&icon_img_dsc_, wifi_failed_icon_id);
    gfx_img_set_src(obj_img_status_, static_cast<void*>(&icon_img_dsc_));
        
    // Set initial state
    current_icon_type_ = wifi_failed_icon_id;
    SetUIDisplayMode(UIDisplayMode::SHOW_TOAST);

    // Initialize toast label
    obj_label_toast_ = gfx_label_create(engine_handle_);
    gfx_obj_align(obj_label_toast_, config_.layout.toast_label.align, config_.layout.toast_label.x, config_.layout.toast_label.y);
    gfx_obj_set_size(obj_label_toast_, config_.layout.toast_label.width, config_.layout.toast_label.height);
    gfx_label_set_text(obj_label_toast_, Lang::Strings::INITIALIZING);
    gfx_label_set_color(obj_label_toast_, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj_label_toast_, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(obj_label_toast_, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_scroll_speed(obj_label_toast_, 20);
    gfx_label_set_scroll_loop(obj_label_toast_, true);
    gfx_label_set_font(obj_label_toast_, (gfx_font_t)fonts_.text_font);

    // Initialize time label
    obj_label_clock_ = gfx_label_create(engine_handle_);
    gfx_obj_align(obj_label_clock_, config_.layout.clock_label.align, config_.layout.clock_label.x, config_.layout.clock_label.y);
    gfx_obj_set_size(obj_label_clock_, config_.layout.clock_label.width, config_.layout.clock_label.height);
    gfx_label_set_text(obj_label_clock_, "--:--");
    gfx_label_set_color(obj_label_clock_, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj_label_clock_, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_font(obj_label_clock_, (gfx_font_t)fonts_.basic_font);

    // Initialize mic listen animation
    int listen_anim_id = GetEmotionId("listen");
    const void* mic_anim_data = mmap_assets_get_mem(assets_handle_, listen_anim_id);
    size_t mic_anim_size = mmap_assets_get_size(assets_handle_, listen_anim_id);
    
    obj_anim_listen_ = gfx_anim_create(engine_handle_);
    gfx_obj_align(obj_anim_listen_, config_.layout.listen_anim.align, config_.layout.listen_anim.x, config_.layout.listen_anim.y);
    gfx_anim_set_src(obj_anim_listen_, mic_anim_data, mic_anim_size);
    gfx_anim_start(obj_anim_listen_);
    gfx_obj_set_visible(obj_anim_listen_, false);
}

EmoteDisplay::~EmoteDisplay()
{
    if (engine_handle_) {
        gfx_emote_deinit(engine_handle_);
        engine_handle_ = nullptr;
    }
}

void EmoteDisplay::SetEmotion(const char* emotion)
{
    if (!engine_handle_) {
        return;
    }

    DisplayLockGuard lock(this);

    ESP_LOGW(TAG, "SetEmotion: %s", emotion);

    auto it = config_.emotion_map.find(emotion);
    if (it != config_.emotion_map.end()) {
        int aaf = std::get<0>(it->second);
        bool repeat = std::get<1>(it->second);
        int fps = std::get<2>(it->second);
        SetEyes(aaf, repeat, fps);
    }
}

void EmoteDisplay::SetEyes(int aaf, bool repeat, int fps)
{
    if (!engine_handle_) {
        return;
    }

    const void* src_data = mmap_assets_get_mem(assets_handle_, aaf);
    size_t src_len = mmap_assets_get_size(assets_handle_, aaf);

    gfx_anim_set_src(obj_anim_eye_, src_data, src_len);
    gfx_anim_set_segment(obj_anim_eye_, 0, 0xFFFF, fps, repeat);
    gfx_anim_start(obj_anim_eye_);
}

int EmoteDisplay::GetIconId(const char* icon_name) const
{
    auto it = config_.icon_map.find(icon_name);
    return (it != config_.icon_map.end()) ? it->second : 0;
}

int EmoteDisplay::GetEmotionId(const char* emotion_name) const
{
    auto it = config_.emotion_map.find(emotion_name);
    return (it != config_.emotion_map.end()) ? std::get<0>(it->second) : 0;
}

void EmoteDisplay::SetIcon(int asset_id)
{
    if (!engine_handle_) {
        return;
    }

    SetImageDesc(&icon_img_dsc_, asset_id);
    gfx_img_set_src(obj_img_status_, static_cast<void*>(&icon_img_dsc_));
    current_icon_type_ = asset_id;
}

void EmoteDisplay::SetChatMessage(const char* role, const char* content)
{
    DisplayLockGuard lock(this);

    ESP_LOGW(TAG, "SetChatMessage: %s", content);

    if (content && strlen(content) > 0) {
        std::string processed_content = content;
        
        size_t pos = 0;
        while ((pos = processed_content.find('\n', pos)) != std::string::npos) {
            processed_content.replace(pos, 1, " ");
            pos += 1; // 移动到下一个位置
        }
        
        gfx_label_set_text(obj_label_toast_, processed_content.c_str());
        SetUIDisplayMode(UIDisplayMode::SHOW_TOAST);
    }
}

void EmoteDisplay::SetStatus(const char* status)
{
    if (!engine_handle_) {
        return;
    }
    DisplayLockGuard lock(this);

    ESP_LOGW(TAG, "SetStatus: %s", status);

    if (std::strcmp(status, Lang::Strings::LISTENING) == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_ANIM_LISTENING);
        SetEyes(GetEmotionId("happy"), true, 20);
        SetIcon("mic");
    } else if (std::strcmp(status, Lang::Strings::STANDBY) == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_CLOCK);
        SetIcon("battery");
    } else if (std::strcmp(status, Lang::Strings::SPEAKING) == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TOAST);
        SetIcon("speaker");
    } else if (std::strcmp(status, Lang::Strings::ERROR) == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TOAST);
        SetIcon("error");
    }

    if (std::strcmp(status, Lang::Strings::CONNECTING) != 0) {
        gfx_label_set_text(obj_label_toast_, status);
    }
}

bool EmoteDisplay::Lock(int timeout_ms)
{
    if (engine_handle_) {
        gfx_emote_lock(engine_handle_);
        return true;
    }
    return false;
}

void EmoteDisplay::Unlock()
{
    if (engine_handle_) {
        gfx_emote_unlock(engine_handle_);
    }
}

void EmoteDisplay::SetIcon(const char* icon)
{
    if (!icon || !engine_handle_) {
        return;
    }
    
    auto it = config_.icon_map.find(icon);
    if (it != config_.icon_map.end()) {
        SetIcon(it->second);
    }
}

void EmoteDisplay::SetPreviewImage(const void* image)
{
    // EmoteDisplay 不支持预览图片，使用默认图标
    if (image) {
        ESP_LOGI(TAG, "EmoteDisplay: Preview image not supported, using default icon");
        SetIcon("battery");
    }
}

void EmoteDisplay::SetTheme(const std::string& theme_name)
{
    current_theme_name_ = theme_name;
    ESP_LOGI(TAG, "EmoteDisplay: Theme set to %s", theme_name.c_str());
    
    // 根据主题设置背景色
    if (engine_handle_) {
        DisplayLockGuard lock(this);
        if (theme_name == "dark") {
            gfx_emote_set_bg_color(engine_handle_, GFX_COLOR_HEX(0x000000));
        } else if (theme_name == "light") {
            gfx_emote_set_bg_color(engine_handle_, GFX_COLOR_HEX(0xFFFFFF));
        }
    }
}

void EmoteDisplay::ShowNotification(const char* notification, int duration_ms)
{
    if (!notification || !engine_handle_) {
        return;
    }
    
    ESP_LOGW(TAG, "EmoteDisplay: Show notification: %s", notification);
    
    DisplayLockGuard lock(this);
    gfx_label_set_text(obj_label_toast_, notification);
    SetUIDisplayMode(UIDisplayMode::SHOW_TOAST);
}

void EmoteDisplay::UpdateStatusBar(bool update_all)
{    
    if (!engine_handle_) {
        return;
    }
    
    int battery_icon_id = GetIconId("battery");
    if (current_icon_type_ == battery_icon_id) {
        time_t now;
        struct tm timeinfo;
        time(&now);

        setenv("TZ", "GMT+0", 1);
        tzset();
        localtime_r(&now, &timeinfo);

        char time_str[6];
        snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

        DisplayLockGuard lock(this);
        gfx_label_set_text(obj_label_clock_, time_str);
        SetUIDisplayMode(UIDisplayMode::SHOW_CLOCK);
    }
}

void EmoteDisplay::SetPowerSaveMode(bool on)
{
    if (engine_handle_) {
        DisplayLockGuard lock(this);

        ESP_LOGW(TAG, "SetPowerSaveMode: %d", on);
        if (on) {
            gfx_anim_stop(obj_anim_eye_);
        } else {
            gfx_anim_start(obj_anim_eye_);
        }
    }
}

} // namespace anim
