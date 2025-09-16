#include "emote_display.h"

#include <cstring>
#include <memory>
#include <unordered_map>
#include <tuple>
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>
#include <model_path.h>

#include "display/lcd_display.h"
#include "config.h"
#include "gfx.h"
#include "application.h"

namespace anim {

static const char* TAG = "emoji";

// Asset name mapping from the old constants to file names
static const std::unordered_map<std::string, std::string> asset_name_map = {
    {"angry_one", "angry_one.aaf"},
    {"dizzy_one", "dizzy_one.aaf"},
    {"enjoy_one", "enjoy_one.aaf"},
    {"happy_one", "happy_one.aaf"},
    {"idle_one", "idle_one.aaf"},
    {"listen", "listen.aaf"},
    {"sad_one", "sad_one.aaf"},
    {"shocked_one", "shocked_one.aaf"},
    {"thinking_one", "thinking_one.aaf"},
    {"icon_battery", "icon_Battery.bin"},
    {"icon_wifi_failed", "icon_WiFi_failed.bin"},
    {"icon_mic", "icon_mic.bin"},
    {"icon_speaker_zzz", "icon_speaker_zzz.bin"},
    {"icon_wifi", "icon_wifi.bin"},
    {"srmodels", "srmodels.bin"},
    {"kaiti", "KaiTi.ttf"}
};

// UI element management
static gfx_obj_t* obj_label_tips = nullptr;
static gfx_obj_t* obj_label_time = nullptr;
static gfx_obj_t* obj_anim_eye = nullptr;
static gfx_obj_t* obj_anim_mic = nullptr;
static gfx_obj_t* obj_img_icon = nullptr;
static gfx_image_dsc_t icon_img_dsc;

// Track current icon to determine when to show time
static std::string current_icon_type = "icon_battery";

enum class UIDisplayMode : uint8_t {
    SHOW_ANIM_TOP = 1,  // Show obj_anim_mic
    SHOW_TIME = 2,      // Show obj_label_time
    SHOW_TIPS = 3       // Show obj_label_tips
};

static void SetUIDisplayMode(UIDisplayMode mode)
{
    gfx_obj_set_visible(obj_anim_mic, false);
    gfx_obj_set_visible(obj_label_time, false);
    gfx_obj_set_visible(obj_label_tips, false);

    // Show the selected control
    switch (mode) {
    case UIDisplayMode::SHOW_ANIM_TOP:
        gfx_obj_set_visible(obj_anim_mic, true);
        break;
    case UIDisplayMode::SHOW_TIME:
        gfx_obj_set_visible(obj_label_time, true);
        break;
    case UIDisplayMode::SHOW_TIPS:
        gfx_obj_set_visible(obj_label_tips, true);
        break;
    }
}

static void clock_tm_callback(void* user_data)
{
    // Only display time when battery icon is shown
    if (current_icon_type == "icon_battery") {
        time_t now;
        struct tm timeinfo;
        time(&now);

        setenv("TZ", "GMT+0", 1);
        tzset();
        localtime_r(&now, &timeinfo);

        char time_str[6];
        snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

        gfx_label_set_text(obj_label_time, time_str);
        SetUIDisplayMode(UIDisplayMode::SHOW_TIME);
    }
}


static void InitializeGraphics(esp_lcd_panel_handle_t panel, gfx_handle_t* engine_handle)
{
    gfx_core_config_t gfx_cfg = {
        .flush_cb = EmoteEngine::OnFlush,
        .user_data = panel,
        .flags = {
            .swap = true,
            .double_buffer = true,
            .buff_dma = true,
        },
        .h_res = DISPLAY_WIDTH,
        .v_res = DISPLAY_HEIGHT,
        .fps = 30,
        .buffers = {
            .buf1 = nullptr,
            .buf2 = nullptr,
            .buf_pixels = DISPLAY_WIDTH * 16,
        },
        .task = GFX_EMOTE_INIT_CONFIG()
    };

    gfx_cfg.task.task_stack_caps = MALLOC_CAP_DEFAULT;
    gfx_cfg.task.task_affinity = 1;
    gfx_cfg.task.task_priority = 1;
    gfx_cfg.task.task_stack = 20 * 1024;

    *engine_handle = gfx_emote_init(&gfx_cfg);
}

static void InitializeEyeAnimation(gfx_handle_t engine_handle)
{
    obj_anim_eye = gfx_anim_create(engine_handle);

    void* anim_data = nullptr;
    size_t anim_size = 0;
    auto& assets = Assets::GetInstance();
    if (!assets.GetAssetData(asset_name_map.at("idle_one"), anim_data, anim_size)) {
        ESP_LOGE(TAG, "Failed to get idle_one animation data");
        return;
    }

    gfx_anim_set_src(obj_anim_eye, anim_data, anim_size);

    gfx_obj_align(obj_anim_eye, GFX_ALIGN_LEFT_MID, 10, -20);
    gfx_anim_set_mirror(obj_anim_eye, true, (DISPLAY_WIDTH - (173 + 10) * 2));
    gfx_anim_set_segment(obj_anim_eye, 0, 0xFFFF, 20, false);
    gfx_anim_start(obj_anim_eye);
}

static void InitializeFont(gfx_handle_t engine_handle)
{
    gfx_font_t font;
    void* font_data = nullptr;
    size_t font_size = 0;
    auto& assets = Assets::GetInstance();
    if (!assets.GetAssetData(asset_name_map.at("kaiti"), font_data, font_size)) {
        ESP_LOGE(TAG, "Failed to get kaiti font data");
        return;
    }
    
    gfx_label_cfg_t font_cfg = {
        .name = "DejaVuSans.ttf",
        .mem = font_data,
        .mem_size = font_size,
    };
    gfx_label_new_font(engine_handle, &font_cfg, &font);

    ESP_LOGI(TAG, "stack: %d", uxTaskGetStackHighWaterMark(nullptr));
}

static void InitializeLabels(gfx_handle_t engine_handle)
{
    // Initialize tips label
    obj_label_tips = gfx_label_create(engine_handle);
    gfx_obj_align(obj_label_tips, GFX_ALIGN_TOP_MID, 0, 45);
    gfx_obj_set_size(obj_label_tips, 160, 40);
    gfx_label_set_text(obj_label_tips, "启动中...");
    gfx_label_set_font_size(obj_label_tips, 20);
    gfx_label_set_color(obj_label_tips, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj_label_tips, GFX_TEXT_ALIGN_LEFT);
    gfx_label_set_long_mode(obj_label_tips, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_scroll_speed(obj_label_tips, 20);
    gfx_label_set_scroll_loop(obj_label_tips, true);

    // Initialize time label
    obj_label_time = gfx_label_create(engine_handle);
    gfx_obj_align(obj_label_time, GFX_ALIGN_TOP_MID, 0, 30);
    gfx_obj_set_size(obj_label_time, 160, 50);
    gfx_label_set_text(obj_label_time, "--:--");
    gfx_label_set_font_size(obj_label_time, 40);
    gfx_label_set_color(obj_label_time, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(obj_label_time, GFX_TEXT_ALIGN_CENTER);
}

static void InitializeMicAnimation(gfx_handle_t engine_handle)
{
    obj_anim_mic = gfx_anim_create(engine_handle);
    gfx_obj_align(obj_anim_mic, GFX_ALIGN_TOP_MID, 0, 25);

    void* anim_data = nullptr;
    size_t anim_size = 0;
    auto& assets = Assets::GetInstance();
    if (!assets.GetAssetData(asset_name_map.at("listen"), anim_data, anim_size)) {
        ESP_LOGE(TAG, "Failed to get listen animation data");
        return;
    }
    
    gfx_anim_set_src(obj_anim_mic, anim_data, anim_size);
    gfx_anim_start(obj_anim_mic);
    gfx_obj_set_visible(obj_anim_mic, false);
}

static void InitializeIcon(gfx_handle_t engine_handle)
{
    obj_img_icon = gfx_img_create(engine_handle);
    gfx_obj_align(obj_img_icon, GFX_ALIGN_TOP_MID, -100, 38);

    SetupImageDescriptor(&icon_img_dsc, "icon_wifi_failed");
    gfx_img_set_src(obj_img_icon, static_cast<void*>(&icon_img_dsc));
}

static void RegisterCallbacks(esp_lcd_panel_io_handle_t panel_io, gfx_handle_t engine_handle)
{
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = EmoteEngine::OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, engine_handle);
}

void SetupImageDescriptor(gfx_image_dsc_t* img_dsc, const std::string& asset_name)
{
    auto& assets = Assets::GetInstance();
    std::string filename = asset_name_map.at(asset_name);
    
    void* img_data = nullptr;
    size_t img_size = 0;
    if (!assets.GetAssetData(filename, img_data, img_size)) {
        ESP_LOGE(TAG, "Failed to get asset data for %s", asset_name.c_str());
        return;
    }

    std::memcpy(&img_dsc->header, img_data, sizeof(gfx_image_header_t));
    img_dsc->data = static_cast<const uint8_t*>(img_data) + sizeof(gfx_image_header_t);
    img_dsc->data_size = img_size - sizeof(gfx_image_header_t);
}

EmoteEngine::EmoteEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    ESP_LOGI(TAG, "Create EmoteEngine, panel: %p, panel_io: %p", panel, panel_io);

    InitializeGraphics(panel, &engine_handle_);

    gfx_emote_lock(engine_handle_);
    gfx_emote_set_bg_color(engine_handle_, GFX_COLOR_HEX(0x000000));

    // Initialize all UI components
    InitializeEyeAnimation(engine_handle_);
    InitializeFont(engine_handle_);
    InitializeLabels(engine_handle_);
    InitializeMicAnimation(engine_handle_);
    InitializeIcon(engine_handle_);

    current_icon_type = "icon_wifi_failed";
    SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);

    gfx_timer_create(engine_handle_, clock_tm_callback, 1000, obj_label_tips);

    gfx_emote_unlock(engine_handle_);

    RegisterCallbacks(panel_io, engine_handle_);
}

EmoteEngine::~EmoteEngine()
{
    if (engine_handle_) {
        gfx_emote_deinit(engine_handle_);
        engine_handle_ = nullptr;
    }
}

void EmoteEngine::setEyes(const std::string& asset_name, bool repeat, int fps)
{
    if (!engine_handle_) {
        return;
    }

    auto& assets = Assets::GetInstance();
    std::string filename = asset_name_map.at(asset_name);
    
    void* src_data = nullptr;
    size_t src_len = 0;
    if (!assets.GetAssetData(filename, src_data, src_len)) {
        ESP_LOGE(TAG, "Failed to get asset data for %s", asset_name.c_str());
        return;
    }

    Lock();
    gfx_anim_set_src(obj_anim_eye, src_data, src_len);
    gfx_anim_set_segment(obj_anim_eye, 0, 0xFFFF, fps, repeat);
    gfx_anim_start(obj_anim_eye);
    Unlock();
}

void EmoteEngine::stopEyes()
{
    // Implementation if needed
}

void EmoteEngine::Lock()
{
    if (engine_handle_) {
        gfx_emote_lock(engine_handle_);
    }
}

void EmoteEngine::Unlock()
{
    if (engine_handle_) {
        gfx_emote_unlock(engine_handle_);
    }
}

void EmoteEngine::SetIcon(const std::string& asset_name)
{
    if (!engine_handle_) {
        return;
    }

    Lock();
    SetupImageDescriptor(&icon_img_dsc, asset_name);
    gfx_img_set_src(obj_img_icon, static_cast<void*>(&icon_img_dsc));
    current_icon_type = asset_name;
    Unlock();
}

bool EmoteEngine::OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io,
                                 esp_lcd_panel_io_event_data_t* edata,
                                 void* user_ctx)
{
    gfx_emote_flush_ready((gfx_handle_t)user_ctx, true);
    return true;
}

void EmoteEngine::OnFlush(gfx_handle_t handle, int x_start, int y_start,
                          int x_end, int y_end, const void* color_data)
{
    auto* panel = static_cast<esp_lcd_panel_handle_t>(gfx_emote_get_user_data(handle));
    if (panel) {
        esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);
    }
}

// EmoteDisplay implementation
EmoteDisplay::EmoteDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    InitializeEngine(panel, panel_io);
}

EmoteDisplay::~EmoteDisplay() = default;

void EmoteDisplay::SetEmotion(const char* emotion)
{
    if (!engine_) {
        return;
    }

    using EmotionParam = std::tuple<std::string, bool, int>;
    static const std::unordered_map<std::string, EmotionParam> emotion_map = {
        {"happy",       {"happy_one",     true,  20}},
        {"laughing",    {"enjoy_one",     true,  20}},
        {"funny",       {"happy_one",     true,  20}},
        {"loving",      {"happy_one",     true,  20}},
        {"embarrassed", {"happy_one",     true,  20}},
        {"confident",   {"happy_one",     true,  20}},
        {"delicious",   {"happy_one",     true,  20}},
        {"sad",         {"sad_one",       true,  20}},
        {"crying",      {"happy_one",     true,  20}},
        {"sleepy",      {"happy_one",     true,  20}},
        {"silly",       {"happy_one",     true,  20}},
        {"angry",       {"angry_one",     true,  20}},
        {"surprised",   {"happy_one",     true,  20}},
        {"shocked",     {"shocked_one",   true,  20}},
        {"thinking",    {"thinking_one",  true,  20}},
        {"winking",     {"happy_one",     true,  20}},
        {"relaxed",     {"happy_one",     true,  20}},
        {"confused",    {"dizzy_one",     true,  20}},
        {"neutral",     {"idle_one",      false, 20}},
        {"idle",        {"idle_one",      false, 20}},
    };

    auto it = emotion_map.find(emotion);
    if (it != emotion_map.end()) {
        std::string asset_name = std::get<0>(it->second);
        bool repeat = std::get<1>(it->second);
        int fps = std::get<2>(it->second);
        engine_->setEyes(asset_name, repeat, fps);
    }
}

void EmoteDisplay::SetChatMessage(const char* role, const char* content)
{
    engine_->Lock();
    if (content && strlen(content) > 0) {
        gfx_label_set_text(obj_label_tips, content);
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);
    }
    engine_->Unlock();
}

void EmoteDisplay::SetStatus(const char* status)
{
    if (!engine_) {
        return;
    }

    if (std::strcmp(status, "聆听中...") == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_ANIM_TOP);
        engine_->setEyes("happy_one", true, 20);
        engine_->SetIcon("icon_mic");
    } else if (std::strcmp(status, "待命") == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIME);
        engine_->SetIcon("icon_battery");
    } else if (std::strcmp(status, "说话中...") == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);
        engine_->SetIcon("icon_speaker_zzz");
    } else if (std::strcmp(status, "错误") == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS);
        engine_->SetIcon("icon_wifi_failed");
    }

    engine_->Lock();
    if (std::strcmp(status, "连接中...") != 0) {
        gfx_label_set_text(obj_label_tips, status);
    }
    engine_->Unlock();
}

void EmoteDisplay::InitializeEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    engine_ = std::make_unique<EmoteEngine>(panel, panel_io);
}

bool EmoteDisplay::Lock(int timeout_ms)
{
    return true;
}

void EmoteDisplay::Unlock()
{
    // Implementation if needed
}

} // namespace anim
