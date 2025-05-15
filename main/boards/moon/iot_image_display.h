/*
 * @FilePath: \xiaozhi-esp32\main\boards\moon\iot_image_display.h
 */
#pragma once

namespace iot {

// 定义图片显示模式
enum ImageDisplayMode {
    MODE_ANIMATED = 0,  // 动画模式（根据音频状态自动播放动画）
    MODE_STATIC = 1     // 静态模式（显示logo.h中的图片）
};

// 声明全局变量，以便在其他文件中使用
extern "C" {
    extern volatile ImageDisplayMode g_image_display_mode;
    extern const unsigned char* g_static_image;
}

} // namespace iot
