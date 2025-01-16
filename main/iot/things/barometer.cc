#include "iot/thing.h"
#include "board.h"
#include "bmp280.h"
#include <cmath>

#include <esp_log.h>
#include <settings.h>

#define TAG "Barometer"

namespace iot
{

#define CONST_PF 0.1902630958 //(1/5.25588f) Pressure factor

    float pressureToAltitude(float pressure, float temperature)
    {
        return ((pow((1015.7f / pressure), CONST_PF) - 1.0f) * (temperature + 273.15f)) / 0.0065f;
    }

    // 这里仅定义 Barometer 的属性和方法，不包含具体的实现
    class Barometer : public Thing
    {
    private:
        float calcTemperature_ = 0;
        int deltaAltitude_ = 0;

    public:
        Barometer() : Thing("Barometer", "当前 AI 机器人的气压计")
        {
            Settings settings("barometer", false);
            deltaAltitude_ = settings.GetInt("delta", 0);

            // 定义设备的属性
            properties_.AddNumberProperty("calialtitude", "校准海拔", [this]() -> int
                                          { return deltaAltitude_; });

            // 定义设备可以被远程执行的指令
            methods_.AddMethod("CaliAltitude", "设置校准海拔", ParameterList({Parameter("calialtitude", "输入当前校准高度", kValueTypeNumber, true)}), [this](const ParameterList &parameters)
                               {
            auto pressure = Board::GetInstance().GetBarometer();
               deltaAltitude_ = static_cast<uint8_t>(parameters["calialtitude"].number()) - pressureToAltitude(pressure, calcTemperature_); 
            Settings settings("barometer", true);
            settings.SetInt("delta", deltaAltitude_); });

            // 定义设备的属性
            properties_.AddNumberProperty("pressure", "当前气压值", [this]() -> int
                                          {
            auto pressure = Board::GetInstance().GetBarometer();
            return (int)pressure; });
            // 定义设备的属性
            properties_.AddNumberProperty("altitude", "当前海拔", [this]() -> int
                                          {
            auto pressure = Board::GetInstance().GetBarometer();
            if(calcTemperature_ == 0)
            calcTemperature_ = Board::GetInstance().GetTemperature();
            return (int)(pressureToAltitude(pressure, calcTemperature_) + deltaAltitude_); });
            // 定义设备的属性
            properties_.AddNumberProperty("temperature", "当前温度", [this]() -> int
                                          {
            auto temperature = Board::GetInstance().GetTemperature();
            return (int)temperature; });
        }
    };

} // namespace iot

DECLARE_THING(Barometer);
