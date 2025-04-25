#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"
#include "settings.h"

#include <esp_log.h>

#define TAG "Aircon"

namespace iot {

// 这里仅定义 Aircon 的属性和方法，包含具体的实现
class Aircon : public Thing {
public:
    Aircon() : Thing("Aircon", "空调") {
        // 定义设备的属性
        properties_.AddNumberProperty("temprature", "当前温度值", [this]() -> int {
            return output_temprature_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetTemprature", "设置温度", ParameterList({
            Parameter("temprature", "16到30之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            output_temprature_ = static_cast<uint8_t>(parameters["volume"].number());
            ESP_LOGI(TAG, "Set output temprature to %d", output_temprature_);
            
            Settings settings("audio", true);
            settings.SetInt("output_temprature", output_temprature_);
        
        });
    }
protected:
    int output_temprature_ = 23;

};

} // namespace iot

DECLARE_THING(Aircon);
