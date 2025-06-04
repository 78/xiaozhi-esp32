/*
    Otto机器人控制器 - MCP协议版本
*/

#include <cJSON.h>
#include <esp_log.h>

#include <cstring>

#include "application.h"
#include "board.h"
#include "config.h"
#include "mcp_server.h"
#include "otto_movements.h"
#include "sdkconfig.h"

#define TAG "OttoController"

class OttoController {
private:
    Otto otto_;
    TaskHandle_t action_task_handle_ = nullptr;
    QueueHandle_t action_queue_;
    bool has_hands_ = false;
    bool is_action_in_progress_ = false;

    struct OttoActionParams {
        int action_type;
        int steps;
        int speed;
        int direction;
        int amount;
    };

    enum ActionType {
        ACTION_WALK = 1,
        ACTION_TURN = 2,
        ACTION_JUMP = 3,
        ACTION_SWING = 4,
        ACTION_MOONWALK = 5,
        ACTION_BEND = 6,
        ACTION_SHAKE_LEG = 7,
        ACTION_UPDOWN = 8,
        ACTION_TIPTOE_SWING = 9,
        ACTION_JITTER = 10,
        ACTION_ASCENDING_TURN = 11,
        ACTION_CRUSAITO = 12,
        ACTION_FLAPPING = 13,
        ACTION_HANDS_UP = 14,
        ACTION_HANDS_DOWN = 15,
        ACTION_HAND_WAVE = 16
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

    static void ActionTask(void* arg) {
        OttoController* controller = static_cast<OttoController*>(arg);
        OttoActionParams params;
        controller->otto_.AttachServos();

        while (true) {
            if (xQueueReceive(controller->action_queue_, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
                ESP_LOGI(TAG, "执行动作: %d", params.action_type);
                controller->is_action_in_progress_ = true;

                switch (params.action_type) {
                    case ACTION_WALK:
                        controller->otto_.Walk(params.steps, params.speed, params.direction,
                                               params.amount);
                        break;
                    case ACTION_TURN:
                        controller->otto_.Turn(params.steps, params.speed, params.direction,
                                               params.amount);
                        break;
                    case ACTION_JUMP:
                        controller->otto_.Jump(params.steps, params.speed);
                        break;
                    case ACTION_SWING:
                        controller->otto_.Swing(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_MOONWALK:
                        controller->otto_.Moonwalker(params.steps, params.speed, params.amount,
                                                     params.direction);
                        break;
                    case ACTION_BEND:
                        controller->otto_.Bend(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_SHAKE_LEG:
                        controller->otto_.ShakeLeg(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_UPDOWN:
                        controller->otto_.UpDown(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_TIPTOE_SWING:
                        controller->otto_.TiptoeSwing(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_JITTER:
                        controller->otto_.Jitter(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_ASCENDING_TURN:
                        controller->otto_.AscendingTurn(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_CRUSAITO:
                        controller->otto_.Crusaito(params.steps, params.speed, params.amount,
                                                   params.direction);
                        break;
                    case ACTION_FLAPPING:
                        controller->otto_.Flapping(params.steps, params.speed, params.amount,
                                                   params.direction);
                        break;
                    case ACTION_HANDS_UP:
                        if (controller->has_hands_) {
                            controller->otto_.HandsUp(params.speed, params.direction);
                        }
                        break;
                    case ACTION_HANDS_DOWN:
                        if (controller->has_hands_) {
                            controller->otto_.HandsDown(params.speed, params.direction);
                        }
                        break;
                    case ACTION_HAND_WAVE:
                        if (controller->has_hands_) {
                            controller->otto_.HandWave(params.speed, params.direction);
                        }
                        break;
                }
                controller->otto_.Home(params.action_type < ACTION_HANDS_UP);
                controller->is_action_in_progress_ = false;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

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

    void StartActionTaskIfNeeded() {
        if (action_task_handle_ == nullptr) {
            xTaskCreate(ActionTask, "otto_action", 1024 * 3, this, configMAX_PRIORITIES - 1,
                        &action_task_handle_);
        }
    }

    void QueueAction(int action_type, int steps, int speed, int direction, int amount) {
        // 参数限制
        action_type = Limit(action_type, ACTION_WALK, ACTION_HAND_WAVE);
        steps = Limit(steps, 1, 100);
        speed = Limit(speed, 500, 3000);
        direction = Limit(direction, -1, 1);

        // 根据动作类型限制幅度参数
        switch (action_type) {
            case ACTION_WALK:
            case ACTION_TURN:
                amount = Limit(amount, 0, 170);
                break;
            case ACTION_SWING:
                amount = Limit(amount, 10, 50);
                break;
            case ACTION_MOONWALK:
                amount = Limit(amount, 15, 40);
                break;
            case ACTION_UPDOWN:
                amount = Limit(amount, 10, 40);
                break;
            case ACTION_TIPTOE_SWING:
                amount = Limit(amount, 10, 50);
                break;
            case ACTION_JITTER:
                amount = Limit(amount, 5, 25);
                break;
            case ACTION_ASCENDING_TURN:
                amount = Limit(amount, 5, 15);
                break;
            case ACTION_CRUSAITO:
                amount = Limit(amount, 20, 50);
                break;
            case ACTION_FLAPPING:
                amount = Limit(amount, 10, 30);
                break;
            default:
                amount = Limit(amount, 10, 50);
        }

        // 检查手部动作
        if ((action_type >= ACTION_HANDS_UP && action_type <= ACTION_HAND_WAVE) && !has_hands_) {
            ESP_LOGW(TAG, "尝试执行手部动作，但机器人没有配置手部舵机");
            return;
        }

        ESP_LOGI(TAG, "动作控制: 类型=%d, 步数=%d, 速度=%d, 方向=%d, 幅度=%d", action_type, steps,
                 speed, direction, amount);

        OttoActionParams params;
        params.action_type = action_type;
        params.steps = steps;
        params.speed = speed;
        params.direction = direction;
        params.amount = amount;

        xQueueSend(action_queue_, &params, portMAX_DELAY);
        StartActionTaskIfNeeded();
    }

public:
    OttoController() {
        otto_.Init(LEFT_LEG_PIN, RIGHT_LEG_PIN, LEFT_FOOT_PIN, RIGHT_FOOT_PIN, LEFT_HAND_PIN,
                   RIGHT_HAND_PIN);

        has_hands_ = (LEFT_HAND_PIN != -1 && RIGHT_HAND_PIN != -1);
        ESP_LOGI(TAG, "Otto机器人初始化%s手部舵机", has_hands_ ? "带" : "不带");

        otto_.Home(true);
        action_queue_ = xQueueCreate(10, sizeof(OttoActionParams));

        RegisterMcpTools();
    }

    void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        ESP_LOGI(TAG, "开始注册MCP工具...");

        // 基础移动动作
        mcp_server.AddTool("self.otto.walk_forward", "行走",
                           PropertyList({Property("步数", kPropertyTypeInteger, 1, 100),
                                         Property("速度", kPropertyTypeInteger, 500, 1500),
                                         Property("手臂摆动幅度", kPropertyTypeInteger, 0, 170),
                                         Property("方向:-1后,1前", kPropertyTypeInteger, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = GetPropertyValue(properties, "步数", 3);
                               int speed = GetPropertyValue(properties, "速度", 1000);
                               int arm_swing = GetPropertyValue(properties, "手臂摆动幅度", 50);
                               int direction = GetPropertyValue(properties, "方向:-1后,1前", 1);
                               QueueAction(ACTION_WALK, steps, 2000 - speed, direction, arm_swing);
                               return true;
                           });

        mcp_server.AddTool("self.otto.turn_left", "转身",
                           PropertyList({Property("步数", kPropertyTypeInteger, 1, 100),
                                         Property("速度", kPropertyTypeInteger, 500, 1500),
                                         Property("手臂摆动幅度", kPropertyTypeInteger, 0, 170),
                                         Property("方向:1左,-1右", kPropertyTypeInteger, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = GetPropertyValue(properties, "步数", 3);
                               int speed = GetPropertyValue(properties, "速度", 1000);
                               int arm_swing = GetPropertyValue(properties, "手臂摆动幅度", 50);
                               int direction = GetPropertyValue(properties, "方向:1左,-1右", 1);
                               QueueAction(ACTION_TURN, steps, 2000 - speed, direction, arm_swing);
                               return true;
                           });

        mcp_server.AddTool("self.otto.jump", "跳跃",
                           PropertyList({Property("跳跃次数", kPropertyTypeInteger, 1, 100),
                                         Property("速度", kPropertyTypeInteger, 500, 1500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = GetPropertyValue(properties, "跳跃次数", 1);
                               int speed = GetPropertyValue(properties, "速度", 1000);
                               QueueAction(ACTION_JUMP, steps, 2000 - speed, 0, 0);
                               return true;
                           });

        // 特殊动作
        mcp_server.AddTool("self.otto.swing", "左右摇摆",
                           PropertyList({Property("次数", kPropertyTypeInteger, 1, 100),
                                         Property("速度", kPropertyTypeInteger, 500, 1500),
                                         Property("摇摆幅度", kPropertyTypeInteger, 0, 170)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = GetPropertyValue(properties, "次数", 3);
                               int speed = GetPropertyValue(properties, "速度", 1000);
                               int amount = GetPropertyValue(properties, "摇摆幅度", 30);
                               QueueAction(ACTION_SWING, steps, 2000 - speed, 0, amount);
                               return true;
                           });

        mcp_server.AddTool("self.otto.moonwalk", "太空步",
                           PropertyList({Property("步数", kPropertyTypeInteger, 1, 100),
                                         Property("速度", kPropertyTypeInteger, 500, 1500),
                                         Property("方向:1左,-1右", kPropertyTypeInteger, -1, 1),
                                         Property("幅度", kPropertyTypeInteger, 0, 170)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = GetPropertyValue(properties, "步数", 3);
                               int speed = GetPropertyValue(properties, "速度", 1000);
                               int direction = GetPropertyValue(properties, "方向:1左,-1右", 1);
                               int amount = GetPropertyValue(properties, "幅度", 25);
                               QueueAction(ACTION_MOONWALK, steps, 2000 - speed, direction, amount);
                               return true;
                           });

        mcp_server.AddTool("self.otto.bend", "弯曲身体",
                           PropertyList({Property("次数", kPropertyTypeInteger, 1, 100),
                                         Property("速度", kPropertyTypeInteger, 500, 1500),
                                         Property("方向:1左,-1右", kPropertyTypeInteger, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = GetPropertyValue(properties, "次数", 1);
                               int speed = GetPropertyValue(properties, "速度", 1000);
                               int direction = GetPropertyValue(properties, "方向:1左,-1右", 1);
                               QueueAction(ACTION_BEND, steps, 2000 - speed, direction, 0);
                               return true;
                           });

        mcp_server.AddTool(
            "self.otto.shake_leg", "摇腿",
            PropertyList({Property("次数", kPropertyTypeInteger, 1, 100),
                          Property("速度", kPropertyTypeInteger, 500, 1500),
                          Property("方向:1左腿,-1右腿", kPropertyTypeInteger, -1, 1)}),
            [this](const PropertyList& properties) -> ReturnValue {
                int steps = GetPropertyValue(properties, "次数", 1);
                int speed = GetPropertyValue(properties, "速度", 1000);
                int direction = GetPropertyValue(properties, "方向:1左腿,-1右腿", 1);
                QueueAction(ACTION_SHAKE_LEG, steps, 2000 - speed, direction, 0);
                return true;
            });

        mcp_server.AddTool("self.otto.updown", "上下运动",
                           PropertyList({Property("次数", kPropertyTypeInteger, 1, 100),
                                         Property("速度", kPropertyTypeInteger, 500, 1500),
                                         Property("幅度", kPropertyTypeInteger, 0, 170)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = GetPropertyValue(properties, "次数", 3);
                               int speed = GetPropertyValue(properties, "速度", 1000);
                               int amount = GetPropertyValue(properties, "幅度", 20);
                               QueueAction(ACTION_UPDOWN, steps, 2000 - speed, 0, amount);
                               return true;
                           });

        // 手部动作（仅在有手部舵机时可用）
        if (has_hands_) {
            mcp_server.AddTool(
                "self.otto.hands_up", "举手",
                PropertyList({Property("速度", kPropertyTypeInteger, 500, 1500),
                              Property("方向:1左,-1右,0双手", kPropertyTypeInteger, -1, 1)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int speed = GetPropertyValue(properties, "速度", 1000);
                    int direction = GetPropertyValue(properties, "方向:1左,-1右,0双手", 1);
                    QueueAction(ACTION_HANDS_UP, 1, 2000 - speed, direction, 0);
                    return true;
                });

            mcp_server.AddTool(
                "self.otto.hands_down", "放手",
                PropertyList({Property("速度", kPropertyTypeInteger, 500, 1500),
                              Property("方向:1左,-1右,0双手", kPropertyTypeInteger, -1, 1)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int speed = GetPropertyValue(properties, "速度", 1000);
                    int direction = GetPropertyValue(properties, "方向:1左,-1右,0双手", 1);
                    QueueAction(ACTION_HANDS_DOWN, 1, 2000 - speed, direction, 0);
                    return true;
                });

            mcp_server.AddTool(
                "self.otto.hand_wave", "挥手",
                PropertyList({Property("速度", kPropertyTypeInteger, 500, 1500),
                              Property("方向:1左,-1右,0双手", kPropertyTypeInteger, -1, 1)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int speed = GetPropertyValue(properties, "速度", 1000);
                    int direction = GetPropertyValue(properties, "方向:1左,-1右,0双手", 1);
                    QueueAction(ACTION_HAND_WAVE, 1, 2000 - speed, direction, 0);
                    return true;
                });
        }

        // 系统工具
        mcp_server.AddTool("self.otto.stop", "立即停止", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               if (action_task_handle_ != nullptr) {
                                   vTaskDelete(action_task_handle_);
                                   action_task_handle_ = nullptr;
                               }
                               is_action_in_progress_ = false;
                               xQueueReset(action_queue_);
                               otto_.Home(true);
                               return true;
                           });

        mcp_server.AddTool("self.otto.get_status", "获取机器人状态", PropertyList(),
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

        ESP_LOGI(TAG, "MCP工具注册完成");
    }

    ~OttoController() {
        if (action_task_handle_ != nullptr) {
            vTaskDelete(action_task_handle_);
            action_task_handle_ = nullptr;
        }
        vQueueDelete(action_queue_);
    }
};

static OttoController* g_otto_controller = nullptr;

void InitializeOttoController() {
    if (g_otto_controller == nullptr) {
        g_otto_controller = new OttoController();
        ESP_LOGI(TAG, "Otto控制器已初始化并注册MCP工具");
    }
}
