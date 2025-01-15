#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <esp_log.h>
#include "rm67162_display.h"
#define TAG "Displayer"

namespace iot
{

    // 这里仅定义 Speaker 的属性和方法，不包含具体的实现
    class Displayer : public Thing
    {
    public:
        Displayer() : Thing("Displayer", "当前 AI 机器人的显示器")
        {
            // 定义设备的属性
            properties_.AddNumberProperty("Brightness", "当前亮度值", [this]() -> int {
                auto display = Board::GetInstance().GetDisplay();
            return display->GetBacklight(); });

            // 定义设备可以被远程执行的指令
            methods_.AddMethod("SetBrightness", "设置亮度", ParameterList({Parameter("brightness", "0到100之间的整数", kValueTypeNumber, true)}), [this](const ParameterList &parameters)
                               {
                auto display = Board::GetInstance().GetDisplay();
            display->SetBacklight(static_cast<uint8_t>(parameters["brightness"].number())); });
        }
    };

} // namespace iot

DECLARE_THING(Displayer);
