#include <cstring>
#include "display/lcd_display.h"
#include <esp_log.h>
#include "emoji_display.h"
#include "assets/lang_config.h"
#include "assets.h"

#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

static const char *TAG = "emoji";

namespace anim {

// Emoji asset name mapping based on usage pattern
static const std::unordered_map<std::string, std::string> emoji_asset_name_map = {
    {"connecting", "connecting.aaf"},
    {"wake", "wake.aaf"},
    {"asking", "asking.aaf"},
    {"happy_loop", "happy_loop.aaf"},
    {"sad_loop", "sad_loop.aaf"},
    {"anger_loop", "anger_loop.aaf"},
    {"panic_loop", "panic_loop.aaf"},
    {"blink_quick", "blink_quick.aaf"},
    {"scorn_loop", "scorn_loop.aaf"}
};

bool EmojiPlayer::OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    auto* disp_drv = static_cast<anim_player_handle_t*>(user_ctx);
    anim_player_flush_ready(disp_drv);
    return true;
}

void EmojiPlayer::OnFlush(anim_player_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    auto* panel = static_cast<esp_lcd_panel_handle_t>(anim_player_get_user_data(handle));
    esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);
}

EmojiPlayer::EmojiPlayer(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    ESP_LOGI(TAG, "Create EmojiPlayer, panel: %p, panel_io: %p", panel, panel_io);

    anim_player_config_t player_cfg = {
        .flush_cb = OnFlush,
        .update_cb = NULL,
        .user_data = panel,
        .flags = {.swap = true},
        .task = ANIM_PLAYER_INIT_CONFIG()
    };

    player_cfg.task.task_priority = 1;
    player_cfg.task.task_stack = 4096;
    player_handle_ = anim_player_init(&player_cfg);

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, player_handle_);
    StartPlayer("connecting", true, 15);
}

EmojiPlayer::~EmojiPlayer()
{
    if (player_handle_) {
        anim_player_update(player_handle_, PLAYER_ACTION_STOP);
        anim_player_deinit(player_handle_);
        player_handle_ = nullptr;
    }
}

void EmojiPlayer::StartPlayer(const std::string& asset_name, bool repeat, int fps)
{
    if (player_handle_) {
        uint32_t start, end;
        void *src_data = nullptr;
        size_t src_len = 0;

        auto& assets = Assets::GetInstance();
        std::string filename = emoji_asset_name_map.at(asset_name);
        if (!assets.GetAssetData(filename, src_data, src_len)) {
            ESP_LOGE(TAG, "Failed to get asset data for %s", asset_name.c_str());
            return;
        }

        anim_player_set_src_data(player_handle_, src_data, src_len);
        anim_player_get_segment(player_handle_, &start, &end);
        if(asset_name == "wake"){
            start = 7;
        }
        anim_player_set_segment(player_handle_, start, end, fps, true);
        anim_player_update(player_handle_, PLAYER_ACTION_START);
    }
}

void EmojiPlayer::StopPlayer()
{
    if (player_handle_) {
        anim_player_update(player_handle_, PLAYER_ACTION_STOP);
    }
}

EmojiWidget::EmojiWidget(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    InitializePlayer(panel, panel_io);
}

EmojiWidget::~EmojiWidget()
{

}

void EmojiWidget::SetEmotion(const char* emotion)
{
    if (!player_) {
        return;
    }

    using Param = std::tuple<std::string, bool, int>;
    static const std::unordered_map<std::string, Param> emotion_map = {
        {"happy",       {"happy_loop", true, 25}},
        {"laughing",    {"happy_loop", true, 25}},
        {"funny",       {"happy_loop", true, 25}},
        {"loving",      {"happy_loop", true, 25}},
        {"embarrassed", {"happy_loop", true, 25}},
        {"confident",   {"happy_loop", true, 25}},
        {"delicious",   {"happy_loop", true, 25}},
        {"sad",         {"sad_loop",   true, 25}},
        {"crying",      {"sad_loop",   true, 25}},
        {"sleepy",      {"sad_loop",   true, 25}},
        {"silly",       {"sad_loop",   true, 25}},
        {"angry",       {"anger_loop", true, 25}},
        {"surprised",   {"panic_loop", true, 25}},
        {"shocked",     {"panic_loop", true, 25}},
        {"thinking",    {"happy_loop", true, 25}},
        {"winking",     {"blink_quick", true, 5}},
        {"relaxed",     {"scorn_loop", true, 25}},
        {"confused",    {"scorn_loop", true, 25}},
    };

    auto it = emotion_map.find(emotion);
    if (it != emotion_map.end()) {
        const auto& [aaf, repeat, fps] = it->second;
        player_->StartPlayer(aaf, repeat, fps);
    } else if (strcmp(emotion, "neutral") == 0) {
    }
}

void EmojiWidget::SetStatus(const char* status)
{
    if (player_) {
        if (strcmp(status, Lang::Strings::LISTENING) == 0) {
            player_->StartPlayer("asking", true, 15);
        } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
            player_->StartPlayer("wake", true, 15);
        }
    }
}

void EmojiWidget::InitializePlayer(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io)
{
    player_ = std::make_unique<EmojiPlayer>(panel, panel_io);
}

bool EmojiWidget::Lock(int timeout_ms)
{
    return true;
}

void EmojiWidget::Unlock()
{
}

} // namespace anim
