/*
    机器人控制器
*/

#include <esp_log.h>

#include <cstring>

#include "application.h"
#include "board.h"
#include "config.h"
#include "iot/thing.h"
#include "movements.h"
#include "sdkconfig.h"

#define TAG "ElectronBotController"

namespace iot {

struct ElectronBotActionParams {
    int action_type;
    int steps;
    int speed;
    int direction;
    int amount;
};

class ElectronBotController : public Thing {
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

public:
    ElectronBotController() : Thing("ElectronBotController", "电子机器人的控制器") {
        electron_bot_.Init(Right_Pitch_Pin, Right_Roll_Pin, Left_Pitch_Pin, Left_Roll_Pin, Body_Pin,
                           Head_Pin);

        electron_bot_.Home(true);
        action_queue_ = xQueueCreate(10, sizeof(ElectronBotActionParams));

        methods_.AddMethod("suspend", "清空动作队列,中断电子机器人动作", ParameterList(),
                           [this](const ParameterList& parameters) {
                               ESP_LOGI(TAG, "停止电子机器人动作");
                               if (action_task_handle_ != nullptr) {
                                   vTaskDelete(action_task_handle_);
                                   action_task_handle_ = nullptr;
                               }
                               is_action_in_progress_ = false;
                               xQueueReset(action_queue_);
                               electron_bot_.Home(true);
                           });

        methods_.AddMethod(
            "AIControl", "AI把机器人待执行动作加入队列,动作需要时间，退下时挥挥手",
            ParameterList(
                {Parameter(
                     "action_type",
                     "动作类型: 1=举左手, 2=举右手, 3=举双手, 4=放左手, 5=放右手, 6=放双手, "
                     "7=挥左手, 8=挥右手, 9=挥双手, 10=拍打左手, 11=拍打右手, 12=拍打双手, "
                     "13=左转, 14=右转, 15=抬头, 16=低头, 17=点头一次, 18=回中心, 19=连续点头",
                     kValueTypeNumber, false),
                 Parameter("steps", "重复次数/步数 (1-100)", kValueTypeNumber, false),
                 Parameter("speed", "动作速度 (500-3000，数值越小越快)", kValueTypeNumber, false),
                 Parameter("direction", "保留参数，暂未使用", kValueTypeNumber, true),
                 Parameter("amount", "动作幅度: 手部动作10-50, 身体转向0-90, 头部动作1-15度",
                           kValueTypeNumber, true)}),
            [this](const ParameterList& parameters) {
                int action_type = parameters["action_type"].number();
                int steps = parameters["steps"].number();
                int speed = parameters["speed"].number();
                int direction = parameters["direction"].number();
                int amount = parameters["amount"].number();

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
                        amount = Limit(amount, 1, 15);
                        break;
                    case ACTION_HEAD_NOD_ONCE:
                    case ACTION_HEAD_CENTER:
                    case ACTION_HEAD_NOD_REPEAT:
                        amount = Limit(amount, 1, 15);
                        break;
                    default:
                        amount = Limit(amount, 10, 50);
                }

                ESP_LOGI(TAG, "AI控制: 动作类型=%d, 步数=%d, 速度=%d, 方向=%d, 幅度=%d",
                         action_type, steps, speed, direction, amount);

                ElectronBotActionParams params;
                params.action_type = action_type;
                params.steps = steps;
                params.speed = speed;
                params.direction = direction;
                params.amount = amount;

                xQueueSend(action_queue_, &params, portMAX_DELAY);

                StartActionTaskIfNeeded();
            });
    }

    void StartActionTaskIfNeeded() {
        if (action_task_handle_ == nullptr) {
            xTaskCreate(ActionTask, "electron_bot_action", 1024 * 4, this, 4, &action_task_handle_);
        }
    }

    ~ElectronBotController() {
        if (action_task_handle_ != nullptr) {
            vTaskDelete(action_task_handle_);
            action_task_handle_ = nullptr;
        }
        vQueueDelete(action_queue_);
    }
};

}  // namespace iot

DECLARE_THING(ElectronBotController);
