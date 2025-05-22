#include "iot/thing.h"
#include "board.h"

#include <esp_log.h>

#define TAG "Battery"

namespace iot {

// 这里仅定义 Battery 的属性和方法，不包含具体的实现
class Battery : public Thing {
private:
    int level_ = 0;
    bool charging_ = false;
    bool discharging_ = false;
    float voltage_ = 0.0f;

public:
    Battery() : Thing("Battery", "The battery of the device") {
        // 定义设备的属性
        properties_.AddNumberProperty("level", "Current battery level", [this]() -> int {
            auto& board = Board::GetInstance();
            if (board.GetBatteryLevel(level_, charging_, discharging_)) {
                return level_;
            }
            return 0;
        });
        properties_.AddBooleanProperty("charging", "Whether the battery is charging", [this]() -> int {
            return charging_;
        });
        properties_.AddStringProperty("voltage", "Current battery voltage", [this]() -> std::string {
            auto& board = Board::GetInstance();
            if (board.GetBatteryVoltage(voltage_)) {
                // 加上单位 V，去掉小数点后面的0
                char buffer[10];
                sprintf(buffer, "%.2fV", voltage_);
                return std::string(buffer);
            }
            return "0V";
        });
    }
};

} // namespace iot

DECLARE_THING(Battery);