#include "emote_display.h"

// Standard C++ headers
#include <cstring>
#include <memory>
#include <unordered_map>
#include <tuple>
#include <algorithm>
#include <cinttypes>

// Standard C headers
#include <sys/time.h>
#include <time.h>

// ESP-IDF headers
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_timer.h>
#include <lvgl.h>

// FreeRTOS headers
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Project headers
#include "assets/lang_config.h"
#include "assets.h"
#include "board.h"
#include "gfx.h"
#include "expression_emote.h"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);


namespace emote {

// ============================================================================
// Constants and Type Definitions
// ============================================================================

static const char* TAG = "EmoteDisplay";

// ============================================================================
// Forward Declarations
// ============================================================================

class EmoteDisplay;

// ============================================================================
// Helper Functions
// ============================================================================

static bool OnFlushIoReady(const esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_io_event_data_t* const edata, void* user_ctx)
{
    emote_handle_t handle = static_cast<emote_handle_t>(user_ctx);
    if (handle) {
        emote_notify_flush_finished(handle);
    }
    return true;
}

// Flush callback for emote
static void OnFlushCallback(int x_start, int y_start, int x_end, int y_end, const void* data, emote_handle_t handle)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)emote_get_user_data(handle);
    if (panel != nullptr) {
        esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, data);
    }
}

// ============================================================================
// Graphics Initialization Functions
// ============================================================================

static emote_handle_t InitializeEmote(const esp_lcd_panel_handle_t panel, const int width, const int height)
{
    if (!panel) {
        ESP_LOGE(TAG, "Invalid panel");
        return nullptr;
    }

    emote_config_t emote_cfg = {
        .flags = {
            .swap = true,
            .double_buffer = true,
            .buff_dma = false,
        },
        .gfx_emote = {
            .h_res = width,
            .v_res = height,
            .fps = 30,
        },
        .buffers = {
            .buf_pixels = static_cast<size_t>(width * 16),
        },
        .task = {
            .task_priority = 5,
            .task_stack = 6 * 1024,
            .task_affinity = 0,
            .task_stack_in_ext = false,
        },
        .flush_cb = OnFlushCallback,
        .user_data = (void*)panel,
    };

    emote_handle_t emote_handle = emote_init(&emote_cfg);
    if (!emote_handle) {
        ESP_LOGE(TAG, "Failed to initialize emote");
        return nullptr;
    }

    return emote_handle;
}

// ============================================================================
// EmoteDisplay Class Implementation
// ============================================================================

EmoteDisplay::EmoteDisplay(const esp_lcd_panel_handle_t panel, const esp_lcd_panel_io_handle_t panel_io,
                           const int width, const int height)
{
    emote_handle_ = InitializeEmote(panel, width, height);

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, emote_handle_);

    // Create chat label
    chat_label_ = lv_label_create(lv_layer_top());
    lv_obj_set_width(chat_label_, width - 20);
    lv_obj_set_style_text_align(chat_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(chat_label_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(chat_label_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(chat_label_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(chat_label_, LV_OPA_70, 0);
    lv_obj_set_style_radius(chat_label_, 5, 0);
    lv_obj_set_style_pad_all(chat_label_, 5, 0);
    lv_obj_align(chat_label_, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_long_mode(chat_label_, LV_LABEL_LONG_WRAP);
    lv_obj_clear_flag(chat_label_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chat_label_, LV_OBJ_FLAG_HIDDEN);

    // Create mute indicator
    mute_indicator_ = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(mute_indicator_, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(mute_indicator_, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_color(mute_indicator_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(mute_indicator_, LV_OPA_50, 0);
    lv_obj_set_style_pad_all(mute_indicator_, 2, 0);
    lv_obj_align(mute_indicator_, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_label_set_text(mute_indicator_, "MUTE");
    lv_obj_add_flag(mute_indicator_, LV_OBJ_FLAG_HIDDEN);
}

EmoteDisplay::~EmoteDisplay()
{
    if (emote_handle_) {
        emote_deinit(emote_handle_);
        emote_handle_ = nullptr;
    }
    if (chat_timer_) {
        lv_timer_del(chat_timer_);
    }
}

void EmoteDisplay::SetEmotion(const char* const emotion)
{
    ESP_LOGI(TAG, "SetEmotion: %s", emotion);
    if (emote_handle_ && emotion && strlen(emotion) > 0) {
        emote_set_anim_emoji(emote_handle_, emotion);
    }
}

void EmoteDisplay::SetChatMessage(const char* const role, const char* const content)
{
    ESP_LOGI(TAG, "SetChatMessage: %s, %s", role, content);
    if (emote_handle_ && content && strlen(content) > 0) {
        if ((std::strcmp(role, "system") == 0) && std::strstr(content, "xiaozhi.me")) {
            size_t len = strlen(content);
            char* new_content = new char[len + 1];
            strcpy(new_content, content);
            std::replace(new_content, new_content + len, static_cast<char>(0x0A), static_cast<char>(0x20));
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SYS, new_content);
            delete[] new_content;
        } else if (std::strcmp(role, "assistant") == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SPEAK, content);
            
            // Show transcript on screen
            if (chat_label_ != nullptr) {
                lv_label_set_text(chat_label_, content);
                lv_obj_clear_flag(chat_label_, LV_OBJ_FLAG_HIDDEN);
                
                if (chat_timer_ != nullptr) {
                    lv_timer_reset(chat_timer_);
                    lv_timer_resume(chat_timer_);
                } else {
                    chat_timer_ = lv_timer_create([](lv_timer_t* t) {
                        lv_obj_t* label = (lv_obj_t*)lv_timer_get_user_data(t);
                        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
                        lv_timer_pause(t);
                    }, 10000, chat_label_);
                }
            }
        } else {
            // User role - just show transcript, don't trigger "speak" animation
            if (chat_label_ != nullptr) {
                std::string user_msg = ">> " + std::string(content);
                lv_label_set_text(chat_label_, user_msg.c_str());
                lv_obj_clear_flag(chat_label_, LV_OBJ_FLAG_HIDDEN);
                
                if (chat_timer_ != nullptr) {
                    lv_timer_reset(chat_timer_);
                    lv_timer_resume(chat_timer_);
                } else {
                    chat_timer_ = lv_timer_create([](lv_timer_t* t) {
                        lv_obj_t* label = (lv_obj_t*)lv_timer_get_user_data(t);
                        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
                        lv_timer_pause(t);
                    }, 5000, chat_label_);
                }
            }
        }
    }
}

void EmoteDisplay::SetMuteState(bool muted) {
    if (mute_indicator_ != nullptr) {
        if (muted) {
            lv_obj_clear_flag(mute_indicator_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(mute_indicator_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void EmoteDisplay::SetStatus(const char* const status)
{
    ESP_LOGI(TAG, "SetStatus: %s", status);
    if (emote_handle_ && status && strlen(status) > 0) {
        if (std::strcmp(status, Lang::Strings::LISTENING) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_LISTEN, NULL);
        } else if (std::strcmp(status, Lang::Strings::STANDBY) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_IDLE, NULL);
        } else if (std::strcmp(status, Lang::Strings::SPEAKING) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SPEAK, NULL);
        } else if (std::strcmp(status, Lang::Strings::ERROR) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SET, NULL);
        }
    }
}

void EmoteDisplay::ShowNotification(const char* notification, int duration_ms)
{
    ESP_LOGI(TAG, "ShowNotification: %s", notification);
    if (emote_handle_ && notification && strlen(notification) > 0) {
        emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SYS, notification);
    }
}

void EmoteDisplay::UpdateStatusBar(bool update_all)
{
    ESP_LOGD(TAG, "UpdateStatusBar: %s", update_all ? "true" : "false");
    if (!emote_handle_) {
        return;
    }
}

void EmoteDisplay::SetPowerSaveMode(bool on)
{
    ESP_LOGI(TAG, "SetPowerSaveMode: %s", on ? "ON" : "OFF");
    if (!emote_handle_) {
        return;
    }
}

void EmoteDisplay::SetPreviewImage(const void* image)
{
    if (image) {
        ESP_LOGI(TAG, "SetPreviewImage: Preview image not supported, using default icon");
    }
}

void EmoteDisplay::SetTheme(Theme* const theme)
{
    ESP_LOGI(TAG, "SetTheme: %p", theme);
}

bool EmoteDisplay::Lock(const int timeout_ms)
{
    (void)timeout_ms;
    return true;
}

void EmoteDisplay::Unlock()
{
}

bool EmoteDisplay::StopAnimDialog()
{
    ESP_LOGI(TAG, "StopAnimDialog");
    if (emote_handle_) {
        return emote_stop_anim_dialog(emote_handle_);
    }
    return false;
}

bool EmoteDisplay::InsertAnimDialog(const char* emoji_name, uint32_t duration_ms)
{
    ESP_LOGI(TAG, "InsertAnimDialog: %s, %" PRIu32, emoji_name, duration_ms);
    if (emote_handle_ && emoji_name) {
        return emote_insert_anim_dialog(emote_handle_, emoji_name, duration_ms);
    }
    return false;
}

void EmoteDisplay::RefreshAll()
{
    if (emote_handle_) {
        emote_notify_all_refresh(emote_handle_);
        return;
    }
}

} // namespace emote