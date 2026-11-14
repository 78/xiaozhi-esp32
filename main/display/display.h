#ifndef DISPLAY_H
#define DISPLAY_H

#include "device_state.h"
#include "emoji_collection.h"
#include "text_glyph.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#define HAVE_LVGL 1
#include <lvgl.h>
#endif

#include <esp_log.h>
#include <esp_pm.h>
#include <esp_timer.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class LvglFont;
class LvglImage;

class Theme {
public:
    Theme(const std::string& name) : name_(name) {}
    virtual ~Theme() = default;

    inline std::string name() const { return name_; }
    virtual std::shared_ptr<LvglFont> GetTextFont() const { return nullptr; }

private:
    std::string name_;
};

class Display {
public:
    Display();
    virtual ~Display();

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string& notification, int duration_ms = 3000);
    // Returns true when the requested emotion was accepted and set in the rendering
    // system. False means invalid input, a missing resource, an uninitialised display, or
    // an unsupported operation. For asynchronous displays, true does not promise that the
    // physical refresh has already completed. Callers are free to ignore the result.
    virtual bool SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    // Remove a matching message without clearing newer content, when supported.
    virtual void RemoveChatMessage(const char* role, const char* content) {}
    virtual void ClearChatMessages();
    virtual void SetTheme(Theme* theme);
    virtual Theme* GetTheme() { return current_theme_; }
    virtual void UpdateStatusBar(bool update_all = false);
    virtual void SetPowerSaveMode(bool on);
    virtual bool AddTextGlyphs(const std::vector<TextGlyph>& glyphs, uint8_t bpp) { return false; }
    virtual void ClearTextGlyphs() {}
    virtual void SetEmojiCollection(std::shared_ptr<EmojiCollection>) {}
    virtual void SetupUI() { setup_ui_called_ = true; }
    virtual bool IsMonochrome() const { return false; }
    virtual bool SupportsGuiOperations() const { return false; }
    virtual void SetHideSubtitle(bool hide) { (void)hide; }
    // Scrolls the chat transcript by a step; direction < 0 shows earlier messages.
    // Returns false when the display has no transcript or nothing left to scroll to.
    virtual bool ScrollChat(int direction) {
        (void)direction;
        return false;
    }
    // Called on every device state change, after the status and emotion for the new
    // state are set, so a display style can follow the conversation (listening, the
    // end of a conversation, the start of the next one).
    virtual void OnDeviceStateChanged(DeviceState state) { (void)state; }
    // One sentence of a spoken answer. speech_id is non-zero and grows with every
    // sentence; the audio of the sentence is reported through SetSpeechProgress() with the
    // same id. Displays that do not follow the speech show it as an assistant message.
    virtual void AddSpokenSentence(uint32_t speech_id, const char* text) {
        (void)speech_id;
        SetChatMessage("assistant", text);
    }
    // A frame of the spoken sentence speech_id started playing at position_ms within the
    // sentence. Called from the audio output task for every frame, so it must not block:
    // implementations only record the value.
    virtual void SetSpeechProgress(uint32_t speech_id, uint32_t position_ms) {
        (void)speech_id;
        (void)position_ms;
    }
    // The spoken answer was cut off, for example because the user interrupted it.
    // May be called from any task; must not block.
    virtual void OnSpeechInterrupted() {}
    // How a spoken answer is lit: word by word (true) or a whole sentence when its audio
    // starts (false). Returns false when the display does not follow the speech.
    virtual bool SetSpeechFollowWords(bool words) {
        (void)words;
        return false;
    }
    virtual bool InsertAnimDialog(const char* name, uint32_t duration_ms) {
        (void)name;
        (void)duration_ms;
        return false;
    }
    virtual bool MountAssets(const char* partition_label) {
        (void)partition_label;
        return false;
    }
    virtual void UnmountAssets() {}
    virtual bool GetAssetData(const std::string& name, const uint8_t*& data, size_t& size) {
        (void)name;
        (void)data;
        (void)size;
        return false;
    }
    virtual void LoadAssets() {}
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image);
    virtual bool SetTextFont(std::shared_ptr<LvglFont> text_font) {
        (void)text_font;
        return false;
    }
    virtual bool SnapshotToJpeg(std::string& jpeg_data, int quality = 80) {
        (void)jpeg_data;
        (void)quality;
        return false;
    }

    inline int width() const { return width_; }
    inline int height() const { return height_; }
    inline bool IsSetupUICalled() const { return setup_ui_called_; }

protected:
    int width_ = 0;
    int height_ = 0;
    bool setup_ui_called_ = false;  // Track if SetupUI() has been called

    Theme* current_theme_ = nullptr;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;
};

class DisplayLockGuard {
public:
    explicit DisplayLockGuard(Display* display)
        : display_(display), locked_(display_->Lock(30000)) {
        if (!locked_) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        if (locked_) {
            display_->Unlock();
        }
    }

    DisplayLockGuard(const DisplayLockGuard&) = delete;
    DisplayLockGuard& operator=(const DisplayLockGuard&) = delete;

    explicit operator bool() const { return locked_; }

private:
    Display* display_;
    bool locked_;
};

class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override { return true; }
    virtual void Unlock() override {}
};

#endif
