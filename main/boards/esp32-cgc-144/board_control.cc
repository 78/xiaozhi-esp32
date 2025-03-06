#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "board.h"
#include "boards/common/wifi_board.h"
#include "boards/esp32-cgc-144/config.h"
#include "iot/thing.h"

#define TAG "BoardControl"

namespace iot {

class BoardControl : public Thing {
public:
    BoardControl() : Thing("BoardControl", "当前 AI 机器人管理和控制") {
	
        // 修改背光控制
        properties_.AddNumberProperty("brightness", "当前亮度百分比", [this]() -> int {
            auto backlight = Board::GetInstance().GetBacklight();
            // 获取原始亮度
            int originalBrightness = backlight ? backlight->brightness() : 0;
            // 将亮度反转
            return 100 - originalBrightness;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetBrightness", "设置亮度", ParameterList({
            Parameter("brightness", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            uint8_t brightness = static_cast<uint8_t>(parameters["brightness"].number());
            // 将亮度反转
            uint8_t reversedBrightness = 100 - brightness;
            auto backlight = Board::GetInstance().GetBacklight();
            if (backlight) {
                backlight->SetBrightness(reversedBrightness, true);
            }
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
