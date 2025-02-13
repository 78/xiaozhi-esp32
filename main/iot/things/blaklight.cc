#include "iot/thing.h"
#include "board.h"
#include "display/lcd_display.h"

#include <esp_log.h>

#define TAG "Backlight"

namespace iot {

// 这里仅定义 Backlight 的属性和方法，不包含具体的实现
class Backlight : public Thing {
public:
    Backlight() : Thing("Backlight", "当前 AI 机器人屏幕的亮度") {
        // 定义设备的属性
        properties_.AddNumberProperty("light", "当前亮度值", [this]() -> int {
            // 这里可以添加获取当前亮度的逻辑
            return current_brightness_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetLight", "设置亮度", ParameterList({
            Parameter("light", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            auto display = Board::GetInstance().GetDisplay();
            uint8_t target_brightness = static_cast<uint8_t>(parameters["light"].number());
            int step = (target_brightness > current_brightness_) ? 1 : -1;
            for (int brightness = current_brightness_; brightness != target_brightness; brightness += step) {
                display->SetBacklight(static_cast<uint8_t>(brightness));
                // 可以根据需要调整渐变速度，这里假设每次调整间隔 10 毫秒
                vTaskDelay(pdMS_TO_TICKS(10)); 
            }
            display->SetBacklight(target_brightness);
            current_brightness_ = target_brightness;  // 保存当前亮度值
        });
    }

private:
    int current_brightness_ = 100;  // 保存当前亮度值
};

} // namespace iot

DECLARE_THING(Backlight);