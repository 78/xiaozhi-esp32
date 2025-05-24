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

LcdPanel &LcdPanel::GetInstance()
{
    static LcdPanel instance;
    return instance;
}

esp_lcd_panel_handle_t LcdPanel::GetHandle() const
{
    return panel_;
}

void LcdPanel::SetPanel(esp_lcd_panel_handle_t panel)
{
    panel_ = panel;
}

esp_err_t LcdPanel::Flush(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_, x_start, y_start, x_end, y_end, color_data);
    return ret;
}

LcdPanel::LcdPanel() : panel_(nullptr) {}

LcdPanel::~LcdPanel() {}

static bool flush_io_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    anim_player_handle_t *disp_drv = (anim_player_handle_t *)user_ctx;
    anim_player_flush_ready(disp_drv);
    return true;
}

namespace anim {

EmojiPlayer::EmojiPlayer(flush_cb_t flush_cb)
{
    anim_player_config_t cfg = {};
    cfg.partition_label = "assets_A";
    cfg.max_files = MMAP_EMOJI_FILES;
    cfg.checksum = MMAP_EMOJI_CHECKSUM;
    cfg.flush_cb = flush_cb;
    handle_ = anim_player_init(&cfg);
}

EmojiPlayer::~EmojiPlayer()
{
    if (handle_) {
        anim_player_update(handle_, PLAYER_ACTION_STOP, 0, 0, false, 0);
        anim_player_deinit(handle_);
        handle_ = nullptr;
    }
}

void EmojiPlayer::StartPlayer(int start_index, int end_index, bool repeat, int fps)
{
    if (handle_) {
        anim_player_update(handle_, PLAYER_ACTION_START, start_index, end_index, repeat, fps);
    }
}

void EmojiPlayer::StopPlayer()
{
    if (handle_) {
        anim_player_update(handle_, PLAYER_ACTION_STOP, 0, 0, false, 0);
    }
}

EmojiWidget::EmojiWidget()
{
    InitializePlayer();
}

EmojiWidget::~EmojiWidget()
{

}

void EmojiWidget::SetEmotion(const char* emotion)
{
    if (!player_) {
        return;
    }

    using Param = std::tuple<int, int, bool, int>;
    static const std::unordered_map<std::string, Param> emotion_map = {
        {"happy",       {MMAP_EMOJI_HAPPY_ENTER_0000_SBMP, MMAP_EMOJI_HAPPY_LOOP_0018_SBMP, true, 25}},
        {"laughing",    {MMAP_EMOJI_HAPPY_ENTER_0000_SBMP, MMAP_EMOJI_HAPPY_LOOP_0018_SBMP, true, 25}},
        {"funny",       {MMAP_EMOJI_HAPPY_ENTER_0000_SBMP, MMAP_EMOJI_HAPPY_LOOP_0018_SBMP, true, 25}},
        {"loving",      {MMAP_EMOJI_HAPPY_ENTER_0000_SBMP, MMAP_EMOJI_HAPPY_LOOP_0018_SBMP, true, 25}},
        {"embarrassed", {MMAP_EMOJI_HAPPY_ENTER_0000_SBMP, MMAP_EMOJI_HAPPY_LOOP_0018_SBMP, true, 25}},
        {"confident",   {MMAP_EMOJI_HAPPY_ENTER_0000_SBMP, MMAP_EMOJI_HAPPY_LOOP_0018_SBMP, true, 25}},
        {"delicious",   {MMAP_EMOJI_HAPPY_ENTER_0000_SBMP, MMAP_EMOJI_HAPPY_LOOP_0018_SBMP, true, 25}},
        {"sad",         {MMAP_EMOJI_SAD_ENTER_0000_SBMP,   MMAP_EMOJI_SAD_LOOP_0018_SBMP,   true, 25}},
        {"crying",      {MMAP_EMOJI_SAD_ENTER_0000_SBMP,   MMAP_EMOJI_SAD_LOOP_0018_SBMP,   true, 25}},
        {"sleepy",      {MMAP_EMOJI_SAD_ENTER_0000_SBMP,   MMAP_EMOJI_SAD_LOOP_0018_SBMP,   true, 25}},
        {"silly",       {MMAP_EMOJI_SAD_ENTER_0000_SBMP,   MMAP_EMOJI_SAD_LOOP_0018_SBMP,   true, 25}},
        {"angry",       {MMAP_EMOJI_ANGER_ENTER_0000_SBMP, MMAP_EMOJI_ANGER_LOOP_0018_SBMP, true, 25}},
        {"surprised",   {MMAP_EMOJI_PANIC_ENTER_0000_SBMP, MMAP_EMOJI_PANIC_LOOP_0018_SBMP, true, 25}},
        {"shocked",     {MMAP_EMOJI_PANIC_ENTER_0000_SBMP, MMAP_EMOJI_PANIC_LOOP_0018_SBMP, true, 25}},
        {"thinking",    {MMAP_EMOJI_HAPPY_ENTER_0000_SBMP, MMAP_EMOJI_HAPPY_LOOP_0018_SBMP, true, 25}},
        {"winking",     {MMAP_EMOJI_BLINK_QUICK_0000_SBMP, MMAP_EMOJI_BLINK_QUICK_0007_SBMP, true, 5}},
        {"relaxed",     {MMAP_EMOJI_SCORN_ENTER_0000_SBMP, MMAP_EMOJI_SCORN_LOOP_0018_SBMP, true, 25}},
        {"confused",    {MMAP_EMOJI_SCORN_ENTER_0000_SBMP, MMAP_EMOJI_SCORN_LOOP_0018_SBMP, true, 25}},
    };

    auto it = emotion_map.find(emotion);
    if (it != emotion_map.end()) {
        const auto& [start, end, repeat, fps] = it->second;
        player_->StartPlayer(start, end, repeat, fps);
    } else if (strcmp(emotion, "neutral") == 0) {
    }
}

void EmojiWidget::SetStatus(const char* status)
{
    if (player_) {
        if (strcmp(status, "聆听中...") == 0) {
            player_->StartPlayer(MMAP_EMOJI_ASKING_0000_SBMP, MMAP_EMOJI_ASKING_0074_SBMP, true, 20);
        } else if (strcmp(status, "待命") == 0) {
            player_->StartPlayer(MMAP_EMOJI_WAKE_0007_SBMP, MMAP_EMOJI_WAKE_0074_SBMP, true, 20);
        }
    }
}

void EmojiWidget::InitializePlayer()
{
    auto flush_cb = [](int x_start, int y_start, int x_end, int y_end, const void *color_data) {
        LcdPanel::GetInstance().Flush(x_start, y_start, x_end, y_end, color_data);
    };

    player_ = std::make_unique<EmojiPlayer>(flush_cb);
}

bool EmojiWidget::Lock(int timeout_ms)
{
    return true;
}

void EmojiWidget::Unlock()
{
}

void EmojiWidget::RegisterPanelCallback(esp_lcd_panel_io_handle_t panel_io)
{
    if (!player_) {
        return;
    }

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = flush_io_ready_callback,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, player_->GetPlayerHandle());
    player_->StartPlayer(MMAP_EMOJI_CONNECTING_0000_SBMP, MMAP_EMOJI_CONNECTING_0074_SBMP, true, 25);
}

} // namespace anim