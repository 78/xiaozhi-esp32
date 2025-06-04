/*
 * @Date: 2025-04-10 10:00:00
 * @LastEditors: Claude
 * @LastEditTime: 2025-04-10 10:00:00
 * @FilePath: \xiaozhi-esp32\main\boards\moon\iot_image_display.cc
 */
#include "iot/thing.h"
#include "board.h"
#include "settings.h"
#include <esp_log.h>
#include "iot_image_display.h"  // 引入头文件
#include "image_manager.h"  // 引入图片资源管理器
#include <stdlib.h>
#include <string.h>

#define TAG "ImageDisplay"

namespace iot {

// 全局变量实现
extern "C" {
    // 默认是动画模式
    volatile ImageDisplayMode g_image_display_mode = MODE_ANIMATED;
    // 静态图片指针，初始为nullptr，将在运行时设置
    const unsigned char* g_static_image = nullptr;
}

// 图片显示控制类
class ImageDisplay : public Thing {
private:
    ImageDisplayMode display_mode_ = MODE_ANIMATED;

public:
    ImageDisplay() : Thing("ImageDisplay", "显示模式，可以切换动画或静态logo图片") {
        // 从图片资源管理器获取logo图片
        auto& image_manager = ImageResourceManager::GetInstance();
        g_static_image = image_manager.GetLogoImage();
        
        if (g_static_image) {
            ESP_LOGI(TAG, "成功获取网络下载的logo图片");
        } else {
            ESP_LOGW(TAG, "暂时无法获取logo图片，可能需要等待下载完成");
        }
    
        // 从系统配置中读取显示模式
        Settings settings("image_display");
        int mode = settings.GetInt("display_mode", MODE_ANIMATED);
        display_mode_ = static_cast<ImageDisplayMode>(mode);
        g_image_display_mode = display_mode_;

        ESP_LOGI(TAG, "当前图片显示模式: %d", display_mode_);
        
        // 定义设备的属性
        properties_.AddNumberProperty("display_mode", "显示模式(0=动画,1=静态logo)", [this]() -> int {
            return static_cast<int>(display_mode_);
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetAnimatedMode", "设置为动画模式（说话时播放动画）", ParameterList(), 
            [this](const ParameterList& parameters) {
                display_mode_ = MODE_ANIMATED;
                g_image_display_mode = MODE_ANIMATED;
                
                // 保存设置
                Settings settings("image_display", true);
                settings.SetInt("display_mode", MODE_ANIMATED);
                
                ESP_LOGI(TAG, "已设置为动画模式");
        });

        methods_.AddMethod("SetStaticMode", "设置为静态模式（固定显示logo图片）", ParameterList(), 
            [this](const ParameterList& parameters) {
                display_mode_ = MODE_STATIC;
                g_image_display_mode = MODE_STATIC;
                
                // 重新获取logo图片（可能在初始化后才下载完成）
                auto& image_manager = ImageResourceManager::GetInstance();
                const uint8_t* logo = image_manager.GetLogoImage();
                if (logo) {
                    g_static_image = logo;
                    ESP_LOGI(TAG, "已更新logo图片");
                }
                
                // 保存设置
                Settings settings("image_display", true);
                settings.SetInt("display_mode", MODE_STATIC);
                
                ESP_LOGI(TAG, "已设置为静态logo模式");
        });

        methods_.AddMethod("ToggleDisplayMode", "切换图片显示模式", ParameterList(), 
            [this](const ParameterList& parameters) {
                // 在动画和静态模式之间切换
                if (display_mode_ == MODE_ANIMATED) {
                    display_mode_ = MODE_STATIC;
                    g_image_display_mode = MODE_STATIC;
                    
                    // 重新获取logo图片
                    auto& image_manager = ImageResourceManager::GetInstance();
                    const uint8_t* logo = image_manager.GetLogoImage();
                    if (logo) {
                        g_static_image = logo;
                    }
                } else {
                    display_mode_ = MODE_ANIMATED;
                    g_image_display_mode = MODE_ANIMATED;
                }
                
                // 保存设置
                Settings settings("image_display", true);
                settings.SetInt("display_mode", static_cast<int>(display_mode_));
                
                ESP_LOGI(TAG, "已切换显示模式为: %d", display_mode_);
        });
    }
    
    // 提供方法在图片下载完成后更新logo图片
    void UpdateLogoImage() {
        auto& image_manager = ImageResourceManager::GetInstance();
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            g_static_image = logo;
            ESP_LOGI(TAG, "logo图片已更新");
        }
    }
};

} // namespace iot

DECLARE_THING(ImageDisplay);
