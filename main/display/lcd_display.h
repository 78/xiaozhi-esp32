#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "gif/lvgl_gif.h"
#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <sdkconfig.h>
#include <atomic>
#include <memory>

#if CONFIG_USE_READING_MESSAGE_STYLE
#include "reading_transcript.h"

#include <vector>

class LvglTheme;
#endif

#define PREVIEW_IMAGE_DURATION_MS 5000

class LcdDisplay : public LvglDisplay {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_draw_buf_t draw_buf_;
    lv_obj_t* top_bar_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* bottom_bar_ = nullptr;
    lv_obj_t* preview_image_ = nullptr;
    lv_obj_t* emoji_label_ = nullptr;
    lv_obj_t* emoji_image_ = nullptr;
    std::unique_ptr<LvglGif> gif_controller_ = nullptr;
    lv_obj_t* emoji_box_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    esp_timer_handle_t preview_timer_ = nullptr;
    std::unique_ptr<LvglImage> preview_image_cached_ = nullptr;
    bool hide_subtitle_ = false;  // Control whether to hide chat messages/subtitles

    void InitializeLcdThemes();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

#if CONFIG_USE_READING_MESSAGE_STYLE
    // Reading message style (lcd_display_reading.cc): one view per transcript turn.
    struct ReadingTurnView {
        uint32_t id = 0;
        lv_obj_t* box = nullptr;
        lv_obj_t* user = nullptr;
        lv_obj_t* assistant = nullptr;
    };
    static constexpr size_t kReadingMaxTurns = 2;
    static constexpr size_t kReadingMaxBytes = 1800;
    ReadingTranscript transcript_{kReadingMaxTurns, kReadingMaxBytes};
    std::vector<ReadingTurnView> turn_views_;
    lv_obj_t* system_label_ = nullptr;
    lv_obj_t* more_below_label_ = nullptr;
    bool follow_latest_ = true;

    // What the line after the newest turn shows while no answer text is on screen.
    enum class ReadingPending { kNone, kListening, kThinking };
    ReadingPending pending_ = ReadingPending::kNone;
    lv_obj_t* pending_label_ = nullptr;
    lv_timer_t* thinking_timer_ = nullptr;
    int64_t thinking_since_us_ = 0;
    DeviceState device_state_ = kDeviceStateUnknown;
    bool conversation_ended_ = false;

    // Following the spoken answer. The audio task only writes the two atomics; the
    // speech timer applies them under the LVGL lock.
    SpeechFollower speech_;
    std::atomic<uint64_t> speech_progress_{0};  // (speech id << 32) | position in ms
    std::atomic<bool> speech_interrupted_{false};
    lv_timer_t* speech_timer_ = nullptr;
    uint64_t applied_progress_ = 0;
    int64_t last_progress_us_ = 0;

    void ApplyTranscriptUpdate(const ReadingTranscript::Update& update);
    ReadingTurnView* FindTurnView(uint32_t id);
    void CreateTurnView(uint32_t id);
    void DeleteTurnView(uint32_t id);
    void SyncTurnView(ReadingTurnView& view);
    void ResizeNewestTurn();
    void AttachTrailingLabels();
    void ParkTrailingLabels();
    void ApplyTurnColors(LvglTheme* theme);
    void ApplyReadingColors(LvglTheme* theme);
    void ScrollToLatest(bool animate);
    void UpdateMoreBelow();
    void SetPending(ReadingPending pending);
    void StylePending(LvglTheme* theme);
    void StartThinking();
    void StopThinking();
    void UpdateThinkingStatus();
    void ApplyStatusTint(LvglTheme* theme);
    void EndConversation();
    void ClearTranscript();
    void AppendAssistant(const char* text, uint32_t speech_id);
    void FollowSpeech();
    void UpdateSpokenSelection();
#endif

protected:
    // Add protected constructor
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
               int height);

public:
    ~LcdDisplay();
    virtual bool SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void RemoveChatMessage(const char* role, const char* content) override;
    virtual void ClearChatMessages() override;
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    virtual void SetupUI() override;
    // Add theme switching function
    virtual void SetTheme(Theme* theme) override;

    // Set whether to hide chat messages/subtitles
    virtual void SetHideSubtitle(bool hide) override;

#if CONFIG_USE_READING_MESSAGE_STYLE
    virtual bool ScrollChat(int direction) override;
    virtual void OnDeviceStateChanged(DeviceState state) override;
    virtual void AddSpokenSentence(uint32_t speech_id, const char* text) override;
    virtual void SetSpeechProgress(uint32_t speech_id, uint32_t position_ms) override;
    virtual void OnSpeechInterrupted() override;
    virtual bool SetSpeechFollowWords(bool words) override;
#endif
};

// SPI LCD display
class SpiLcdDisplay : public LcdDisplay {
public:
    SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                  int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                  bool swap_xy);
};

// RGB LCD display
class RgbLcdDisplay : public LcdDisplay {
public:
    RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                  int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                  bool swap_xy);
};

// MIPI LCD display
class MipiLcdDisplay : public LcdDisplay {
public:
    MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                   int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                   bool swap_xy);
};

#endif  // LCD_DISPLAY_H
