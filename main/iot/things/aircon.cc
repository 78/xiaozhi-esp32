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
        properties_.AddNumberProperty("mode", "モード", [this]() -> int {
            return output_mode_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetTemprature", "设置温度", ParameterList({
            Parameter("temprature", "16到30之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            output_temprature_ = static_cast<uint8_t>(parameters["temprature"].number());
            ESP_LOGI(TAG, "Set output temprature to %d", output_temprature_);
            
            Settings settings("aircon", true);
            settings.SetInt("temprature", output_temprature_);
        
        });

        methods_.AddMethod("SetMode", "動作モードの設定", ParameterList({
            Parameter("mode", "AUTO、冷房、暖房、送風、OFF", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            output_mode_ = static_cast<uint8_t>(parameters["mode"].number());
            ESP_LOGI(TAG, "Set output mode to %d", output_mode_);
            
            Settings settings("aircon", true);
            settings.SetInt("mode", output_mode_);
        
        });
    }
protected:
    int output_temprature_ = 23;
    int output_mode_ = 0;

};

} // namespace iot

DECLARE_THING(Aircon);
