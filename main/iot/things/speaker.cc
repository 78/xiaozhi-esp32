#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"
#include "display/lcd_display.h"
#include <string>

#include <esp_log.h>

#define TAG "Speaker"

namespace iot
{

    // 这里仅定义 Speaker 的属性和方法，不包含具体的实现
    class Speaker : public Thing
    {
    public:
        Speaker() : Thing("Speaker", "当前 AI 机器人的扬声器")
        {
            // 定义设备的属性
            properties_.AddNumberProperty("volume", "当前音量值", [this]() -> int
                                          {
            auto codec = Board::GetInstance().GetAudioCodec();
            return codec->output_volume(); });

            // 定义设备可以被远程执行的指令
            methods_.AddMethod("SetVolume", "设置音量", ParameterList({Parameter("volume", "10到100之间的整数", kValueTypeNumber, true)}), [this](const ParameterList &parameters)
                               {
            auto display = Board::GetInstance().GetDisplay();
            auto codec = Board::GetInstance().GetAudioCodec();
            auto volume = static_cast<uint8_t>(parameters["volume"].number());
            if(volume<10) volume = 10;
            codec->SetOutputVolume(volume);
            
            
            char tempstr[11] = {0};
            sprintf(tempstr, "SOUND:%d", volume);
            display->Notification((std::string)tempstr,2000); });
        }
    };

} // namespace iot

DECLARE_THING(Speaker);
