#pragma once

#include "anim_player.h"
#include "display/lcd_display.h"
#include <memory>
#include <functional>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

class LcdPanel {
public:
    static LcdPanel &GetInstance();
    esp_lcd_panel_handle_t GetHandle() const;
    void SetPanel(esp_lcd_panel_handle_t panel);
    esp_err_t Flush(int x_start, int y_start, int x_end, int y_end, const void *color_data);

private:
    LcdPanel();
    ~LcdPanel();

    // Delete copy constructor and assignment operator
    LcdPanel(const LcdPanel &) = delete;
    LcdPanel &operator=(const LcdPanel &) = delete;

    esp_lcd_panel_handle_t panel_;
};

namespace anim {

class EmojiPlayer {
public:
    flush_cb_t flush_cb;

    EmojiPlayer(flush_cb_t flush_cb);
    ~EmojiPlayer();

    void StartPlayer(int start_index, int end_index, bool repeat, int fps);
    void StopPlayer();
    anim_player_handle_t GetPlayerHandle() const
    {
        return handle_;
    }

private:
    anim_player_handle_t handle_;
};

class EmojiWidget : public Display {
public:
    EmojiWidget();
    virtual ~EmojiWidget();

    virtual void SetEmotion(const char* emotion) override;
    virtual void SetStatus(const char* status) override;
    anim::EmojiPlayer* GetPlayer()
    {
        return player_.get();
    }
    void RegisterPanelCallback(esp_lcd_panel_io_handle_t panel_io);

private:
    void InitializePlayer();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    std::unique_ptr<anim::EmojiPlayer> player_;
};

} // namespace anim