#include <freertos/FreeRTOS.h>  // 引入FreeRTOS操作系统库，用于任务管理和调度
#include <freertos/timers.h>    // 引入FreeRTOS定时器库，用于定时任务
#include <freertos/task.h>      // 引入FreeRTOS任务库，用于创建和管理任务
#include <esp_log.h>            // 引入ESP32日志库，用于记录日志信息

#include "board.h"              // 引入板级支持库，提供板级硬件接口
#include "boards/common/wifi_board.h"  // 引入WiFi板级支持库，提供WiFi相关功能
#include "boards/esp32-s3-touch-amoled-1.8/config.h"  // 引入特定板子的配置文件
#include "iot/thing.h"          // 引入IoT设备库，提供设备管理和控制功能

#define TAG "BoardControl"      // 定义日志标签，用于标识日志来源

namespace iot {

// BoardControl类继承自Thing类，用于管理和控制AI机器人
class BoardControl : public Thing {
public:
    // 构造函数，初始化BoardControl对象
    BoardControl() : Thing("BoardControl", "当前 AI 机器人管理和控制") {
        // 添加一个名为"ResetWifiConfiguration"的方法，用于重新配置WiFi
        methods_.AddMethod("ResetWifiConfiguration", "重新配网", ParameterList(), 
            [this](const ParameterList& parameters) {
                ESP_LOGI(TAG, "ResetWifiConfiguration");  // 记录日志，表示开始重新配网
                auto board = static_cast<WifiBoard*>(&Board::GetInstance());  // 获取WiFi板实例
                if (board && board->GetBoardType() == "wifi") {  // 检查板子类型是否为WiFi
                    board->ResetWifiConfiguration();  // 调用重置WiFi配置的方法
                }
            });
    }
};

} // namespace iot

// 声明BoardControl类为一个Thing，使其可以被IoT系统识别和管理
DECLARE_THING(BoardControl); 