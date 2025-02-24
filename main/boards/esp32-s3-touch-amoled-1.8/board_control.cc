#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "board.h"
#include "boards/common/wifi_board.h"
#include "boards/esp32-s3-touch-amoled-1.8/config.h"
#include "iot/thing.h"

#define TAG "BoardControl"

namespace iot {

class BoardControl : public Thing {
public:
    BoardControl() : Thing("BoardControl", "当前 AI 机器人管理和控制") {
        // 添加电池电量属性
        properties_.AddNumberProperty("BatteryLevel", "当前电池电量百分比", [this]() -> int {
            int level = 0;
            bool charging = false;
            Board::GetInstance().GetBatteryLevel(level, charging);
            ESP_LOGI(TAG, "当前电池电量: %d%%, 充电状态: %s", level, charging ? "充电中" : "未充电");
            return level;
        });

        // 添加充电状态属性
        properties_.AddBooleanProperty("Charging", "是否正在充电", [this]() -> bool {
            int level = 0;
            bool charging = false;
            Board::GetInstance().GetBatteryLevel(level, charging);
            ESP_LOGI(TAG, "当前电池电量: %d%%, 充电状态: %s", level, charging ? "充电中" : "未充电");
            return charging;
        });

        // 修改重新配网
        methods_.AddMethod("ResetWifiConfiguration", "重新配网", ParameterList(), 
            [this](const ParameterList& parameters) {
                ESP_LOGI(TAG, "ResetWifiConfiguration");
                auto board = static_cast<WifiBoard*>(&Board::GetInstance());
                if (board && board->GetBoardType() == "wifi") {
                    board->ResetWifiConfiguration();
                }
            });
    }
};

} // namespace iot

DECLARE_THING(BoardControl); 
