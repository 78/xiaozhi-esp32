/*
    Otto机器人控制器
*/

#include <esp_log.h>

#include <cstring>

#include "application.h"
#include "board.h"
#include "config.h"
#include "iot/thing.h"
#include "otto_movements.h"
#include "sdkconfig.h"

#define TAG "OttoController"

namespace iot {

struct OttoActionParams {
    int action_type;
    int steps;
    int speed;
    int direction;
    int amount;
};

class OttoController : public Thing {
private:
    Otto otto_;
    TaskHandle_t action_task_handle_ = nullptr;
    QueueHandle_t action_queue_;
    bool has_hands_ = false;
    bool is_action_in_progress_ = false;

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
                // controller->otto_.DetachServos();
                controller->is_action_in_progress_ = false;
            }

            // if (uxQueueMessagesWaiting(controller->action_queue_) == 0 &&
            //     !controller->is_action_in_progress_) {
            //     ESP_LOGI(TAG, "动作队列为空且没有动作正在执行，任务退出");
            //     controller->action_task_handle_ = nullptr;
            //     vTaskDelete(NULL);
            //     break;
            // }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

public:
    OttoController() : Thing("OttoController", "Otto机器人的控制器") {
        otto_.Init(LEFT_LEG_PIN, RIGHT_LEG_PIN, LEFT_FOOT_PIN, RIGHT_FOOT_PIN, LEFT_HAND_PIN,
                   RIGHT_HAND_PIN);

        has_hands_ = (LEFT_HAND_PIN != -1 && RIGHT_HAND_PIN != -1);
        ESP_LOGI(TAG, "Otto机器人初始化%s手部舵机", has_hands_ ? "带" : "不带");

        otto_.Home(true);
        action_queue_ = xQueueCreate(10, sizeof(OttoActionParams));

        methods_.AddMethod("suspend", "清空动作队列,中断Otto机器人动作", ParameterList(),
                           [this](const ParameterList& parameters) {
                               ESP_LOGI(TAG, "停止Otto机器人动作");
                               if (action_task_handle_ != nullptr) {
                                   vTaskDelete(action_task_handle_);
                                   action_task_handle_ = nullptr;
                               }
                               is_action_in_progress_ = false;
                               xQueueReset(action_queue_);
                               otto_.Home(true);
                           });

        methods_.AddMethod(
            "AIControl", "AI把机器人待执行动作加入队列,动作需要时间，退下时挥挥手",
            ParameterList(
                {Parameter("action_type",
                           "动作类型: 1=行走(前后), 2=转向（左右）, 3=跳跃, 4=摇摆, 5=太空步, "
                           "6=弯曲, 7=摇腿, 8=上下运动, 9=脚尖摇摆, 10=抖动, 11=上升转弯, "
                           "12=十字步, 13=拍打, 14=举手(左、右、同时), 15=放手(左、右、同时), "
                           "16=挥手(左、右、同时)",
                           kValueTypeNumber, false),
                 Parameter("steps", "步数", kValueTypeNumber, false),
                 Parameter("speed", "速度 (越小越快500-3000)默认1000", kValueTypeNumber, false),
                 Parameter("direction", "方向 (1=左/前, -1=右/后, 0=同时)", kValueTypeNumber, true),
                 Parameter("amount",
                           "动作幅度(除手臂摆动最小10),"
                           "行走时amount=0表示不摆动双手否则幅度50-170,转向时同理,"
                           "其他动作限制不一样:摇摆10-"
                           "50, 太空步15-40"
                           "上下运动10-40, 脚尖摇摆10-50, 抖动5-25, 上升转弯5-15, 十字步20-50, "
                           "拍打10-30",
                           kValueTypeNumber, true)}),
            [this](const ParameterList& parameters) {
                int action_type = parameters["action_type"].number();
                int steps = parameters["steps"].number();
                int speed = parameters["speed"].number();
                int direction = parameters["direction"].number();
                int amount = parameters["amount"].number();

                action_type = Limit(action_type, ACTION_WALK, ACTION_HAND_WAVE);
                steps = Limit(steps, 1, 100);
                speed = Limit(speed, 500, 3000);
                direction = Limit(direction, -1, 1);

                switch (action_type) {
                    case ACTION_WALK | ACTION_TURN:
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

                if ((action_type >= ACTION_HANDS_UP && action_type <= ACTION_HAND_WAVE) &&
                    !has_hands_) {
                    ESP_LOGW(TAG, "尝试执行手部动作，但机器人没有配置手部舵机");
                    return;
                }

                ESP_LOGI(TAG, "AI控制: 动作类型=%d, 步数=%d, 速度=%d, 方向=%d, 幅度=%d",
                         action_type, steps, speed, direction, amount);

                OttoActionParams params;
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
            xTaskCreate(ActionTask, "otto_action", 1024 * 3, this, configMAX_PRIORITIES - 1,
                        &action_task_handle_);
        }
    }

    ~OttoController() {
        if (action_task_handle_ != nullptr) {
            vTaskDelete(action_task_handle_);
            action_task_handle_ = nullptr;
        }
        vQueueDelete(action_queue_);
    }
};

}  // namespace iot

DECLARE_THING(OttoController);
