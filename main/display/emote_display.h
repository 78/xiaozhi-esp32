#pragma once

#include "display.h"
#include "lvgl_font.h"
#include <memory>
#include <functional>
#include <map>
#include <string>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

namespace emote {

// Simple data structure for storing asset data without LVGL dependency
struct AssetData {
    const void* data;
    size_t size;
    union {
        uint8_t flags;  // 1 byte for all animation flags
        struct {
            uint8_t fps : 6;    // FPS (0-63) - 6 bits
            uint8_t loop : 1;   // Loop animation - 1 bit
            uint8_t lack : 1;   // Lack animation - 1 bit
        };
    };

    AssetData() : data(nullptr), size(0), flags(0) {}
    AssetData(const void* d, size_t s) : data(d), size(s), flags(0) {}
    AssetData(const void* d, size_t s, uint8_t f, bool l, bool k)
        : data(d), size(s)
    {
        fps = f > 63 ? 63 : f;  // 限制 FPS 到 6 位范围
        loop = l;
        lack = k;
    }
};

// Layout element data structure
struct LayoutData {
    char align;  // Store as char instead of string
    int x;
    int y;
    int width;
    int height;
    bool has_size;

    LayoutData() : align(0), x(0), y(0), width(0), height(0), has_size(false) {}
    LayoutData(char a, int x_pos, int y_pos, int w = 0, int h = 0)
        : align(a), x(x_pos), y(y_pos), width(w), height(h), has_size(w > 0 && h > 0) {}
};

// Function to convert align string to GFX_ALIGN enum value
char StringToGfxAlign(const std::string &align_str);

class EmoteEngine;

class EmoteDisplay : public Display {
public:
    EmoteDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io, int width, int height);
    virtual ~EmoteDisplay();

    virtual void SetEmotion(const char* emotion) override;
    virtual void SetStatus(const char* status) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetTheme(Theme* theme) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void UpdateStatusBar(bool update_all = false) override;
    virtual void SetPowerSaveMode(bool on) override;
    virtual void SetPreviewImage(const void* image);

    void AddEmojiData(const std::string &name, const void* data, size_t size, uint8_t fps = 0, bool loop = false, bool lack = false);
    void AddIconData(const std::string &name, const void* data, size_t size);
    void AddLayoutData(const std::string &name, const std::string &align_str, int x, int y, int width = 0, int height = 0);
    void AddTextFont(std::shared_ptr<LvglFont> text_font);
    AssetData GetEmojiData(const std::string &name) const;
    AssetData GetIconData(const std::string &name) const;

    EmoteEngine* GetEngine() const;
    void* GetEngineHandle() const;

    inline std::shared_ptr<LvglFont> text_font() const
    {
        return text_font_;
    }

private:
    void InitializeEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io, int width, int height);
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    std::unique_ptr<EmoteEngine> engine_;

    // Font management
    std::shared_ptr<LvglFont> text_font_ = nullptr;

    // Non-LVGL asset data storage
    std::map<std::string, AssetData> emoji_data_map_;
    std::map<std::string, AssetData> icon_data_map_;

};

} // namespace emote
