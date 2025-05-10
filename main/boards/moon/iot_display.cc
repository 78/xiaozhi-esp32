/*
 * @Date: 2025-04-08 22:17:39
 * @LastEditors: zhouke
 * @LastEditTime: 2025-04-10 00:00:44
 * @FilePath: \xiaozhi-esp32\main\boards\moon\iot_display.cc
 */
#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <lvgl.h>

#define TAG "RotateDisplay"
 
namespace iot {

// 这里仅定义 Lamp 的属性和方法，不包含具体的实现
class RotateDisplay : public Thing {
private:
    // 当前旋转角度
    int current_rotation_ = 0;
 
public:
    RotateDisplay() : Thing("RotateDisplay", "显示屏幕，可旋转")  {
        // 定义设备可以被远程执行的指令
        methods_.AddMethod("RotateDisplay", "翻转屏幕", ParameterList(), [this](const ParameterList& parameters) {
            // 获取 LVGL 显示器对象
            lv_display_t* lv_display = lv_display_get_default();
            if (!lv_display) {
                ESP_LOGE(TAG, "无法获取 LVGL 显示器对象");
                return;
            }
            
            // 旋转屏幕 (0: 0度, 1: 90度, 2: 180度, 3: 270度)
            current_rotation_ = (current_rotation_ + 1) % 4;
            lv_display_set_rotation(lv_display, (lv_display_rotation_t)current_rotation_);
            
            ESP_LOGI(TAG, "屏幕已旋转到 %d 度", current_rotation_ * 90);
        });
    }
};

} // namespace iot

DECLARE_THING(RotateDisplay);
