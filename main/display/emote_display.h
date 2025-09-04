#ifndef EMOTE_DISPLAY_H
#define EMOTE_DISPLAY_H

#include "display.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include "esp_mmap_assets.h"
#include "gfx.h"
#include <unordered_map>
#include <tuple>
#include <string>

namespace anim {

enum class UIDisplayMode : uint8_t {
    SHOW_ANIM_LISTENING = 1,  // Show obj_anim_mic
    SHOW_CLOCK = 2,      // Show obj_label_time
    SHOW_TOAST = 3       // Show obj_label_tips
};

// 表情映射参数结构：(资源ID, 是否重复, 帧率)
using EmotionParam = std::tuple<int, bool, int>;
using EmotionMap = std::unordered_map<std::string, EmotionParam>;

// 图标映射参数结构
using IconMap = std::unordered_map<std::string, int>;

// UI元素布局配置结构
struct UILayoutConfig {
    // 眼部动画位置 (align, x_offset, y_offset)
    struct {
        int align = GFX_ALIGN_LEFT_MID;
        int x = 10;
        int y = 10;
    } eye_anim;
    
    // 状态图标位置
    struct {
        int align = GFX_ALIGN_TOP_MID;
        int x = -100;
        int y = 38;
    } status_icon;
    
    // 提示文本位置
    struct {
        int align = GFX_ALIGN_TOP_MID;
        int x = 0;
        int y = 40;
        int width = 160;
        int height = 40;
    } toast_label;
    
    // 时钟文本位置
    struct {
        int align = GFX_ALIGN_TOP_MID;
        int x = 0;
        int y = 35;
        int width = 160;
        int height = 50;
    } clock_label;
    
    // 听取动画位置
    struct {
        int align = GFX_ALIGN_TOP_MID;
        int x = 0;
        int y = 25;
    } listen_anim;
};

// GFX 引擎配置结构
struct GFXEngineConfig {
    size_t buf_pixels = 0;          // 缓冲区像素数 (0 表示自动计算)
    int task_stack_size = 6 * 1024; // 任务栈大小
};

// 统一的显示配置结构
struct EmoteDisplayConfig {
    EmotionMap emotion_map;     // 表情映射
    IconMap icon_map;           // 图标映射
    UILayoutConfig layout;      // UI布局配置
    GFXEngineConfig gfx_config; // GFX 引擎配置
};

class EmoteDisplay : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    gfx_handle_t engine_handle_ = nullptr;
    mmap_assets_handle_t assets_handle_ = nullptr;
    
    // GFX UI 元素
    gfx_obj_t* obj_label_toast_ = nullptr;
    gfx_obj_t* obj_label_clock_ = nullptr;
    gfx_obj_t* obj_anim_eye_ = nullptr;
    gfx_obj_t* obj_anim_listen_ = nullptr;
    gfx_obj_t* obj_img_status_ = nullptr;
    
    // 图标相关
    gfx_image_dsc_t icon_img_dsc_;
    int current_icon_type_;
    
    // 字体配置
    DisplayFonts fonts_;
    
    // 主题相关
    std::string current_theme_name_;
    
    // 显示配置
    EmoteDisplayConfig config_;

    void SetupUI(int width, int height);
    void SetUIDisplayMode(UIDisplayMode mode);
    void SetEyes(int aaf, bool repeat, int fps);
    void SetIcon(int asset_id);
    void SetImageDesc(gfx_image_dsc_t* img_dsc, int asset_id);
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;
    
    // 辅助函数：通过字符串查找资源ID
    int GetIconId(const char* icon_name) const;
    int GetEmotionId(const char* emotion_name) const;

public:
    // Callback functions (public to be accessible from static helper functions)
    static bool OnFlushIoReady(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
    static void OnFlush(gfx_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data);

protected:
    // 添加protected构造函数
    EmoteDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, DisplayFonts fonts, mmap_assets_handle_t assets_handle, const EmoteDisplayConfig& config);
    
    
public:
    virtual ~EmoteDisplay();

    // 实现 Display 接口的所有方法
    virtual void SetEmotion(const char* emotion) override;      
    virtual void SetStatus(const char* status) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetIcon(const char* icon) override;
    virtual void SetPreviewImage(const void* image) override;
    virtual void SetTheme(const std::string& theme_name) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void UpdateStatusBar(bool update_all = false) override;
    virtual void SetPowerSaveMode(bool on) override;
};

class SPIEmoteDisplay : public EmoteDisplay {
    public:
        SPIEmoteDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height,
                      DisplayFonts fonts, mmap_assets_handle_t assets_handle,
                      const EmoteDisplayConfig& config);
};

} // namespace anim

#endif // EMOTE_DISPLAY_H