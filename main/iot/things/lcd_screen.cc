#include "board.h"
#include "iot/thing.h"
#include "display/display.h"
#include "display/lcd_display.h"

#include <esp_log.h>

#define TAG "LCDScreen"

namespace iot {

// 这里仅定义 LCDScreen 的属性和方法，不包含具体的实现
class LCDScreen : public Thing {
public:
    LCDScreen() : Thing("LCDScreen", "当前 AI 机器人的屏幕") {
        // 定义亮度属性
        properties_.AddNumberProperty("brightness", "当前屏幕背光亮度百分比", [this]() -> int {
            auto display = static_cast<LcdDisplay*>(Board::GetInstance().GetDisplay());
            if (display) {
                ESP_LOGD(TAG, "当前背光亮度: %d%%", display->backlight());
                return display->backlight();
            }
            return 0;
        });

        // 定义设置背光亮度方法
        methods_.AddMethod("SetBrightness", "设置屏幕背光亮度", ParameterList({
            Parameter("brightness", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            auto display = static_cast<LcdDisplay*>(Board::GetInstance().GetDisplay());
            if (display) {
                display->SetBacklight(static_cast<uint8_t>(parameters["brightness"].number()));
            }
        });
        // 定义切换帮助页面方法
        methods_.AddMethod("ShowHelpPage", "显示帮助/配置页面", ParameterList(), [this](const ParameterList& parameters) {
            auto display = static_cast<LcdDisplay*>(Board::GetInstance().GetDisplay());
            if (display) {
                display->lv_config_page();
            }
        });
        // 定义切换聊天页面方法
        methods_.AddMethod("ShowChatPage", "显示聊天页面", ParameterList(), [this](const ParameterList& parameters) {
            auto display = static_cast<LcdDisplay*>(Board::GetInstance().GetDisplay());
            if (display) {
                display->lv_chat_page();
            }
        });
    }
};

} // namespace iot

DECLARE_THING(LCDScreen);




