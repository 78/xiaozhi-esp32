/*
    Electron Bot机器人控制器 - MCP协议版本
*/

#include <cJSON.h>
#include <esp_log.h>

#include <cstring>

#include "application.h"
#include "board.h"
#include "config.h"
#include "mcp_server.h"
#include "movements.h"
#include "sdkconfig.h"

#define TAG "ElectronBotController"

struct ElectronBotActionParams {
    int action_type;
    int steps;
    int speed;
    int direction;
    int amount;
};

class ElectronBotController {
private:
    Otto electron_bot_;
    TaskHandle_t action_task_handle_ = nullptr;
    QueueHandle_t action_queue_;
    bool is_action_in_progress_ = false;

    enum ActionType {
        // 手部动作 1-12
        ACTION_HAND_LEFT_UP = 1,      // 举左手
        ACTION_HAND_RIGHT_UP = 2,     // 举右手
        ACTION_HAND_BOTH_UP = 3,      // 举双手
        ACTION_HAND_LEFT_DOWN = 4,    // 放左手
        ACTION_HAND_RIGHT_DOWN = 5,   // 放右手
        ACTION_HAND_BOTH_DOWN = 6,    // 放双手
        ACTION_HAND_LEFT_WAVE = 7,    // 挥左手
        ACTION_HAND_RIGHT_WAVE = 8,   // 挥右手
        ACTION_HAND_BOTH_WAVE = 9,    // 挥双手
        ACTION_HAND_LEFT_FLAP = 10,   // 拍打左手
        ACTION_HAND_RIGHT_FLAP = 11,  // 拍打右手
        ACTION_HAND_BOTH_FLAP = 12,   // 拍打双手

        // 身体动作 13-14
        ACTION_BODY_TURN_LEFT = 13,   // 左转
        ACTION_BODY_TURN_RIGHT = 14,  // 右转

        // 头部动作 15-19
        ACTION_HEAD_UP = 15,         // 抬头
        ACTION_HEAD_DOWN = 16,       // 低头
        ACTION_HEAD_NOD_ONCE = 17,   // 点头一次
        ACTION_HEAD_CENTER = 18,     // 回中心
        ACTION_HEAD_NOD_REPEAT = 19  // 连续点头
    };

    template <typename T>
    T GetPropertyValue(const PropertyList& properties, const std::string& name,
                       const T& default_value) const {
        try {
            return properties[name].value<T>();
        } catch (const std::runtime_error&) {
            return default_value;
        }
    }

    // 限制数值在指定范围内
    static int Limit(int value, int min, int max) {
        if (value < min) {
            ESP_LOGW(TAG, "参数 %d 小于最小值 %d，设置为最小值", value, min);
            return min;
        }
        if (value > max) {
            ESP_LOGW(TAG, "参数 %d 大于最大值 %d，设置为最大值", value, max);
            return max;
        }
        return value;
    }

    static void ActionTask(void* arg) {
        ElectronBotController* controller = static_cast<ElectronBotController*>(arg);
        ElectronBotActionParams params;

        while (true) {
            if (xQueueReceive(controller->action_queue_, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
                ESP_LOGI(TAG, "执行动作: %d", params.action_type);
                controller->is_action_in_progress_ = true;  // 开始执行动作
                controller->electron_bot_.AttachServos();

                switch (params.action_type) {
                    case ACTION_HAND_LEFT_UP:
                    case ACTION_HAND_RIGHT_UP:
                    case ACTION_HAND_BOTH_UP:
                    case ACTION_HAND_LEFT_DOWN:
                    case ACTION_HAND_RIGHT_DOWN:
                    case ACTION_HAND_BOTH_DOWN:
                    case ACTION_HAND_LEFT_WAVE:
                    case ACTION_HAND_RIGHT_WAVE:
                    case ACTION_HAND_BOTH_WAVE:
                    case ACTION_HAND_LEFT_FLAP:
                    case ACTION_HAND_RIGHT_FLAP:
                    case ACTION_HAND_BOTH_FLAP:
                        controller->electron_bot_.HandAction(params.action_type, params.steps,
                                                             params.amount, params.speed);
                        break;
                    case ACTION_BODY_TURN_LEFT:
                        controller->electron_bot_.BodyAction(1, params.steps, params.amount,
                                                             params.speed);
                        break;
                    case ACTION_BODY_TURN_RIGHT:
                        controller->electron_bot_.BodyAction(2, params.steps, params.amount,
                                                             params.speed);
                        break;
                    case ACTION_HEAD_UP:
                        controller->electron_bot_.HeadAction(1, params.steps, params.amount,
                                                             params.speed);
                        break;
                    case ACTION_HEAD_DOWN:
                        controller->electron_bot_.HeadAction(2, params.steps, params.amount,
                                                             params.speed);
                        break;
                    case ACTION_HEAD_NOD_ONCE:
                        controller->electron_bot_.HeadAction(3, params.steps, params.amount,
                                                             params.speed);
                        break;
                    case ACTION_HEAD_CENTER:
                        controller->electron_bot_.HeadAction(4, params.steps, params.amount,
                                                             params.speed);
                        break;
                    case ACTION_HEAD_NOD_REPEAT:
                        controller->electron_bot_.HeadAction(5, params.steps, params.amount,
                                                             params.speed);
                        break;
                }
                controller->electron_bot_.DetachServos();
                controller->is_action_in_progress_ = false;  // 动作执行完毕
            }

            // 检查是否可以退出任务：队列为空且没有动作正在执行
            if (uxQueueMessagesWaiting(controller->action_queue_) == 0 &&
                !controller->is_action_in_progress_) {
                controller->electron_bot_.Home(params.action_type < ACTION_HAND_BOTH_UP);
                ESP_LOGI(TAG, "动作队列为空且没有动作正在执行，任务退出");
                controller->action_task_handle_ = nullptr;
                vTaskDelete(NULL);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    void QueueAction(int action_type, int steps, int speed, int direction, int amount) {
        // 参数限制
        action_type = Limit(action_type, ACTION_HAND_LEFT_UP, ACTION_HEAD_NOD_REPEAT);
        steps = Limit(steps, 1, 100);
        speed = Limit(speed, 500, 3000);
        direction = Limit(direction, -1, 1);

        switch (action_type) {
            case ACTION_HAND_LEFT_UP:
            case ACTION_HAND_RIGHT_UP:
            case ACTION_HAND_BOTH_UP:
            case ACTION_HAND_LEFT_DOWN:
            case ACTION_HAND_RIGHT_DOWN:
            case ACTION_HAND_BOTH_DOWN:
            case ACTION_HAND_LEFT_WAVE:
            case ACTION_HAND_RIGHT_WAVE:
            case ACTION_HAND_BOTH_WAVE:
            case ACTION_HAND_LEFT_FLAP:
            case ACTION_HAND_RIGHT_FLAP:
            case ACTION_HAND_BOTH_FLAP:
                amount = Limit(amount, 10, 50);
                break;
            case ACTION_BODY_TURN_LEFT:
            case ACTION_BODY_TURN_RIGHT:
                amount = Limit(amount, 0, 90);
                break;
            case ACTION_HEAD_UP:
            case ACTION_HEAD_DOWN:
            case ACTION_HEAD_NOD_ONCE:
            case ACTION_HEAD_CENTER:
            case ACTION_HEAD_NOD_REPEAT:
                amount = Limit(amount, 1, 15);
                break;
            default:
                amount = Limit(amount, 10, 50);
        }

        ESP_LOGI(TAG, "动作控制: 类型=%d, 步数=%d, 速度=%d, 方向=%d, 幅度=%d", action_type, steps,
                 speed, direction, amount);

        ElectronBotActionParams params;
        params.action_type = action_type;
        params.steps = steps;
        params.speed = speed;
        params.direction = direction;
        params.amount = amount;

        xQueueSend(action_queue_, &params, portMAX_DELAY);
        StartActionTaskIfNeeded();
    }

    void StartActionTaskIfNeeded() {
        if (action_task_handle_ == nullptr) {
            xTaskCreate(ActionTask, "electron_bot_action", 1024 * 4, this, configMAX_PRIORITIES - 1,
                        &action_task_handle_);
        }
    }

public:
    ElectronBotController() {
        electron_bot_.Init(Right_Pitch_Pin, Right_Roll_Pin, Left_Pitch_Pin, Left_Roll_Pin, Body_Pin,
                           Head_Pin);

        electron_bot_.Home(true);
        action_queue_ = xQueueCreate(10, sizeof(ElectronBotActionParams));

        RegisterMcpTools();
        ESP_LOGI(TAG, "Electron Bot控制器已初始化并注册MCP工具");
    }

    void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        ESP_LOGI(TAG, "开始注册Electron Bot MCP工具...");

        // 手部动作统一工具
        mcp_server.AddTool(
            "self.electron.hand_action", "手部动作控制",
            PropertyList({Property("动作:1举手,2放手,3挥手,4拍打", kPropertyTypeInteger, 1, 4),
                          Property("手部:1左手,2右手,3双手", kPropertyTypeInteger, 1, 3),
                          Property("次数", kPropertyTypeInteger, 1, 10),
                          Property("速度", kPropertyTypeInteger, 500, 1500),
                          Property("幅度", kPropertyTypeInteger, 10, 50)}),
            [this](const PropertyList& properties) -> ReturnValue {
                int action_type = GetPropertyValue(properties, "动作:1举手,2放手,3挥手,4拍打", 1);
                int hand_type = GetPropertyValue(properties, "手部:1左手,2右手,3双手", 3);
                int steps = GetPropertyValue(properties, "次数", 1);
                int speed = GetPropertyValue(properties, "速度", 1000);
                int amount = GetPropertyValue(properties, "幅度", 30);

                // 根据动作类型和手部类型计算具体动作
                int action_id;
                switch (action_type) {
                    case 1:  // 举手
                        action_id = ACTION_HAND_LEFT_UP + (hand_type - 1);
                        break;
                    case 2:  // 放手
                        action_id = ACTION_HAND_LEFT_DOWN + (hand_type - 1);
                        amount = 0;  // 放手动作不需要幅度
                        break;
                    case 3:  // 挥手
                        action_id = ACTION_HAND_LEFT_WAVE + (hand_type - 1);
                        amount = 0;  // 挥手动作不需要幅度
                        break;
                    case 4:  // 拍打
                        action_id = ACTION_HAND_LEFT_FLAP + (hand_type - 1);
                        amount = 0;  // 拍打动作不需要幅度
                        break;
                    default:
                        action_id = ACTION_HAND_BOTH_UP;
                }

                QueueAction(action_id, steps, 2000 - speed, 0, amount);
                return true;
            });

        // 身体动作
        mcp_server.AddTool("self.electron.body_turn", "身体转向",
                           PropertyList({Property("步数", kPropertyTypeInteger, 1, 10),
                                         Property("速度", kPropertyTypeInteger, 500, 1500),
                                         Property("方向:1左转,2右转", kPropertyTypeInteger, 1, 2),
                                         Property("角度", kPropertyTypeInteger, 0, 90)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = GetPropertyValue(properties, "步数", 1);
                               int speed = GetPropertyValue(properties, "速度", 1000);
                               int direction = GetPropertyValue(properties, "方向:1左转,2右转", 1);
                               int amount = GetPropertyValue(properties, "角度", 45);
                               int action = (direction == 1) ? ACTION_BODY_TURN_LEFT
                                                             : ACTION_BODY_TURN_RIGHT;
                               QueueAction(action, steps, 2000 - speed, 0, amount);
                               return true;
                           });

        // 头部动作
        mcp_server.AddTool("self.electron.head_move", "头部运动",
                           PropertyList({Property("动作:1抬头,2低头,3点头,4回中心,5连续点头",
                                                  kPropertyTypeInteger, 1, 5),
                                         Property("次数", kPropertyTypeInteger, 1, 10),
                                         Property("速度", kPropertyTypeInteger, 500, 1500),
                                         Property("角度", kPropertyTypeInteger, 1, 15)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int action_num = GetPropertyValue(
                                   properties, "动作:1抬头,2低头,3点头,4回中心,5连续点头", 3);
                               int steps = GetPropertyValue(properties, "次数", 1);
                               int speed = GetPropertyValue(properties, "速度", 1000);
                               int amount = GetPropertyValue(properties, "角度", 5);
                               int action = ACTION_HEAD_UP + (action_num - 1);
                               QueueAction(action, steps, 2000 - speed, 0, amount);
                               return true;
                           });

        // 系统工具
        mcp_server.AddTool("self.electron.stop", "立即停止", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               if (action_task_handle_ != nullptr) {
                                   vTaskDelete(action_task_handle_);
                                   action_task_handle_ = nullptr;
                               }
                               is_action_in_progress_ = false;
                               xQueueReset(action_queue_);
                               electron_bot_.Home(true);
                               return true;
                           });

        mcp_server.AddTool("self.electron.get_status", "获取机器人状态", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               return is_action_in_progress_ ? "moving" : "idle";
                           });

        mcp_server.AddTool("self.battery.get_level", "获取机器人电池电量和充电状态", PropertyList(),
                           [](const PropertyList& properties) -> ReturnValue {
                               auto& board = Board::GetInstance();
                               int level = 0;
                               bool charging = false;
                               bool discharging = false;
                               board.GetBatteryLevel(level, charging, discharging);

                               std::string status =
                                   "{\"level\":" + std::to_string(level) +
                                   ",\"charging\":" + (charging ? "true" : "false") + "}";
                               return status;
                           });

        ESP_LOGI(TAG, "Electron Bot MCP工具注册完成");
    }

    ~ElectronBotController() {
        if (action_task_handle_ != nullptr) {
            vTaskDelete(action_task_handle_);
            action_task_handle_ = nullptr;
        }
        vQueueDelete(action_queue_);
    }
};

static ElectronBotController* g_electron_controller = nullptr;

void InitializeElectronBotController() {
    if (g_electron_controller == nullptr) {
        g_electron_controller = new ElectronBotController();
        ESP_LOGI(TAG, "Electron Bot控制器已初始化并注册MCP工具");
    }
}
