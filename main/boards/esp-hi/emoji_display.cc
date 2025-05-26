#include <cstring>
#include "display/lcd_display.h"
#include <esp_log.h>
#include "mmap_generate_emoji.h"
#include "emoji_display.h"

#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

static const char *TAG = "emoji";

namespace anim {

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
    const mmap_assets_config_t assets_cfg = {
        .partition_label = "assets_A",
        .max_files = MMAP_EMOJI_FILES,
        .checksum = MMAP_EMOJI_CHECKSUM,
        .flags = {.mmap_enable = true, .full_check = true}
    };

    mmap_assets_new(&assets_cfg, &assets_handle_);

    anim_player_config_t player_cfg = {
        .flush_cb = OnFlush,
        .update_cb = NULL,
        .user_data = panel,
        .flags = {.swap = true},
        .task = ANIM_PLAYER_INIT_CONFIG()
    };

    player_handle_ = anim_player_init(&player_cfg);

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, player_handle_);
    StartPlayer(MMAP_EMOJI_CONNECTING_AAF, true, 15);
}

EmojiPlayer::~EmojiPlayer()
{
    if (player_handle_) {
        anim_player_update(player_handle_, PLAYER_ACTION_STOP);
        anim_player_deinit(player_handle_);
        player_handle_ = nullptr;
    }

    if (assets_handle_) {
        mmap_assets_del(assets_handle_);
        assets_handle_ = NULL;
    }
}

void EmojiPlayer::StartPlayer(int aaf, bool repeat, int fps)
{
    if (player_handle_) {
        uint32_t start, end;
        const void *src_data;
        size_t src_len;

        src_data = mmap_assets_get_mem(assets_handle_, aaf);
        src_len = mmap_assets_get_size(assets_handle_, aaf);

        anim_player_set_src_data(player_handle_, src_data, src_len);
        anim_player_get_segment(player_handle_, &start, &end);
        if(MMAP_EMOJI_WAKE_AAF == aaf){
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

    using Param = std::tuple<int, bool, int>;
    static const std::unordered_map<std::string, Param> emotion_map = {
        {"happy",       {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"laughing",    {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"funny",       {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"loving",      {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"embarrassed", {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"confident",   {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"delicious",   {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"sad",         {MMAP_EMOJI_SAD_LOOP_AAF,   true, 25}},
        {"crying",      {MMAP_EMOJI_SAD_LOOP_AAF,   true, 25}},
        {"sleepy",      {MMAP_EMOJI_SAD_LOOP_AAF,   true, 25}},
        {"silly",       {MMAP_EMOJI_SAD_LOOP_AAF,   true, 25}},
        {"angry",       {MMAP_EMOJI_ANGER_LOOP_AAF, true, 25}},
        {"surprised",   {MMAP_EMOJI_PANIC_LOOP_AAF, true, 25}},
        {"shocked",     {MMAP_EMOJI_PANIC_LOOP_AAF, true, 25}},
        {"thinking",    {MMAP_EMOJI_HAPPY_LOOP_AAF, true, 25}},
        {"winking",     {MMAP_EMOJI_BLINK_QUICK_AAF, true, 5}},
        {"relaxed",     {MMAP_EMOJI_SCORN_LOOP_AAF, true, 25}},
        {"confused",    {MMAP_EMOJI_SCORN_LOOP_AAF, true, 25}},
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
        if (strcmp(status, "聆听中...") == 0) {
            player_->StartPlayer(MMAP_EMOJI_ASKING_AAF, true, 15);
        } else if (strcmp(status, "待命") == 0) {
            player_->StartPlayer(MMAP_EMOJI_WAKE_AAF, true, 15);
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
