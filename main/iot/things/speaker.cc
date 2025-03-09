#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <esp_log.h>

#define TAG "Speaker"

namespace iot {

// 这里仅定义 Speaker 的属性和方法，不包含具体的实现
class Speaker : public Thing {
public:
    Speaker() : Thing("Speaker", "扬声器") {
        // 定义设备的属性
        properties_.AddNumberProperty("volume", "当前音量值", [this]() -> int {
            auto codec = Board::GetInstance().GetAudioCodec();
            return codec->output_volume();
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetVolume", "设置音量", ParameterList({
            Parameter("volume", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            auto codec = Board::GetInstance().GetAudioCodec();
            codec->SetOutputVolume(static_cast<uint8_t>(parameters["volume"].number()));
        });
    }
};

} // namespace iot

DECLARE_THING(Speaker);
