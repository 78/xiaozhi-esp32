#include "iot/thing.h"
#include "board.h"

#include <esp_log.h>

#define TAG "Temperature"

namespace iot {

class Temperature : public Thing {
private:
    float esp32temp = 0.0f;
public:
    Temperature() : Thing("Temperature", "芯片温度管理") {
        // 定义设备的属性
        properties_.AddNumberProperty("temp", "当前芯片温度", [this]() -> float {
            auto& board = Board::GetInstance();
            if (board.GetTemperature(esp32temp)) {
                return esp32temp;
            }
            return 0;
        });
       
    }
};

} // namespace iot

DECLARE_THING(Temperature);