#pragma once

#include "display/lcd_display.h"
#include <memory>
#include <functional>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include "mmap_generate_emoji_normal.h"
#include "gfx.h"

namespace anim {

// Helper function for setting up image descriptors
void SetupImageDescriptor(mmap_assets_handle_t assets_handle, gfx_image_dsc_t* img_dsc, int asset_id);

class EmoteEngine;

using FlushIoReadyCallback = std::function<bool(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*)>;
using FlushCallback = std::function<void(gfx_handle_t, int, int, int, int, const void*)>;

class EmoteEngine {
public:
    EmoteEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io);
    ~EmoteEngine();

    void setEyes(int aaf, bool repeat, int fps);
    void stopEyes();
    
    void Lock();
    void Unlock();
    
    void SetIcon(int asset_id);
    mmap_assets_handle_t GetAssetsHandle() const { return assets_handle_; }

    // Callback functions (public to be accessible from static helper functions)
    static bool OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
    static void OnFlush(gfx_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data);

private:
    gfx_handle_t engine_handle_;
    mmap_assets_handle_t assets_handle_;
};

class EmoteDisplay : public Display {
public:
    EmoteDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io);
    virtual ~EmoteDisplay();

    virtual void SetEmotion(const char* emotion) override;
    virtual void SetStatus(const char* status) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    
    anim::EmoteEngine* GetEngine()
    {
        return engine_.get();
    }

private:
    void InitializeEngine(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io);
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    std::unique_ptr<anim::EmoteEngine> engine_;
};

} // namespace anim
