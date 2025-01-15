#include "iot/thing.h"
#include "board.h"
#include "bmp280.h"
#include <cmath>

#include <esp_log.h>

#define TAG "Barometer"

namespace iot {

#define CONST_PF 0.1902630958	                                               //(1/5.25588f) Pressure factor
#define FIX_TEMP 10				                                               // Fixed Temperature. ASL is a function of pressure and temperature, but as the temperature changes so much (blow a little towards the flie and watch it drop 5 degrees) it corrupts the ASL estimates.

double pressureToAltitude(double pressure) {
    // 海拔计算公式
    double altitude = ((std::pow((1015.7f / pressure), CONST_PF) - 1.0f) * (FIX_TEMP + 273.15f)) / 0.0065f;
    return altitude;
}

// 这里仅定义 Barometer 的属性和方法，不包含具体的实现
class Barometer : public Thing {
public:
    Barometer() : Thing("Barometer", "当前 AI 机器人的气压计") {
        // 定义设备的属性
        properties_.AddNumberProperty("pressure", "当前气压值", [this]() -> int {
            auto pressure = Board::GetInstance().GetBarometer();
            return (int)pressure;
        });
        // 定义设备的属性
        properties_.AddNumberProperty("altitude", "当前海拔", [this]() -> int {
            auto pressure = Board::GetInstance().GetBarometer();
            return (int)pressureToAltitude(pressure);
        });
    }
};

} // namespace iot

DECLARE_THING(Barometer);
