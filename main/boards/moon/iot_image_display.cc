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
#include "logo.h"  // 引入logo图片
#include "iot_image_display.h"  // 引入头文件
#include <stdlib.h>
#include <string.h>

#define TAG "ImageDisplay"

namespace iot {

// 声明处理过的图片数据指针
static unsigned char* processed_logo_image = nullptr;

// 处理图片数据，交换字节顺序
static const unsigned char* process_logo_image() {
    const int logo_size = 115200; // 从logo.h中获取的大小

    // 如果已经处理过，直接返回
    if (processed_logo_image != nullptr) {
        return processed_logo_image;
    }
    
    // 分配内存
    processed_logo_image = (unsigned char*)malloc(logo_size);
    if (!processed_logo_image) {
        ESP_LOGE(TAG, "无法为处理的logo图片分配内存");
        return gImage_logo; // 失败则返回原始图片
    }
    
    // 拷贝原始数据
    memcpy(processed_logo_image, gImage_logo, logo_size);
    
    // 交换字节顺序 - 和ImageResourceManager::LoadImageFile方法中一样
    for (int i = 0; i < logo_size; i += 2) {
        if (i + 1 < logo_size) {
            unsigned char temp = processed_logo_image[i];
            processed_logo_image[i] = processed_logo_image[i+1];
            processed_logo_image[i+1] = temp;
        }
    }
    
    ESP_LOGI(TAG, "Logo图片数据处理完成");
    return processed_logo_image;
}

// 全局变量实现
extern "C" {
    // 默认是动画模式
    volatile ImageDisplayMode g_image_display_mode = MODE_ANIMATED;
    // 设置静态图片为处理过的logo图片
    const unsigned char* g_static_image = nullptr;
}

// 图片显示控制类
class ImageDisplay : public Thing {
private:
    ImageDisplayMode display_mode_ = MODE_ANIMATED;

public:
    ImageDisplay() : Thing("ImageDisplay", "显示模式，可以切换动画或静态logo图片") {
        // 处理logo图片数据
        g_static_image = process_logo_image();
    
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
    
    ~ImageDisplay() {
        // 释放处理后的图片内存
        if (processed_logo_image) {
            free(processed_logo_image);
            processed_logo_image = nullptr;
        }
    }
};

} // namespace iot

DECLARE_THING(ImageDisplay);
