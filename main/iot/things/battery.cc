#include "iot/thing.h"
#include "board.h"
#include "display/lcd_display.h"

#include <esp_log.h>

#define TAG "Battery"

namespace iot
{

    // 这里仅定义 Battery 的属性和方法，不包含具体的实现
    class Battery : public Thing
    {
    private:
        int level_ = 0;
        bool charging_ = false;

    public:
        Battery() : Thing("Battery", "电池管理")
        {
            // 定义设备的属性
            properties_.AddNumberProperty("level", "当前电量百分比（0-100）", [this]() -> int
                                          {
            auto& board = Board::GetInstance();
            auto display = board.GetDisplay();
            char tempstr[11] = {0};
            if (board.GetBatteryLevel(level_, charging_)) {
                sprintf(tempstr, "BATTERY%d", level_);
                display->Notification((std::string)tempstr,2000); 
                return level_;
            }
            return 0; });
            properties_.AddBooleanProperty("charging", "是否充电中", [this]() -> int
                                           { return charging_; });
        }
    };

} // namespace iot

DECLARE_THING(Battery);