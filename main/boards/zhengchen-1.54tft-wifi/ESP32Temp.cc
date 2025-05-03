#include "iot/thing.h"
#include "board.h"

#include <esp_log.h>

#define TAG "ESP32Temp"

namespace iot {

class ESP32Temp : public Thing {
private:
    float esp32temp = 0.0f;
public:
    ESP32Temp() : Thing("ESP32Temp", "芯片温度管理") {
        // 定义设备的属性
        properties_.AddNumberProperty("esp32temp", "当前芯片温度", [this]() -> float {
            auto& board = Board::GetInstance();
            if (board.GetESP32Temp(esp32temp)) {
                return esp32temp;
            }
            return 0;
        });
       
    }
};

} // namespace iot

DECLARE_THING(ESP32Temp);