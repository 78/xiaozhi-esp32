#include "iot/thing.h"
#include "board.h"
#include "display/lcd_display.h"
#include "settings.h"

#include <esp_log.h>
#include <string>

#define TAG "Screen"

namespace iot {

// 这里仅定义 Screen 的属性和方法，不包含具体的实现
class Screen : public Thing {
public:
    Screen() : Thing("Screen", "这是一个屏幕，可设置主题") {
        // 定义设备的属性
        properties_.AddStringProperty("theme", "主题", [this]() -> std::string {
            auto theme = Board::GetInstance().GetDisplay()->GetTheme();
            return theme;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetTheme", "设置屏幕主题", ParameterList({
            Parameter("theme_name", "主题模式, light 或 dark", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            std::string theme_name = static_cast<std::string>(parameters["theme_name"].string());
            auto display = Board::GetInstance().GetDisplay();
            if (display) {
                display->SetTheme(theme_name);
            }
        });
    }
};

} // namespace iot

DECLARE_THING(Screen);