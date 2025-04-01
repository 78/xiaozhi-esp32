/*
    Otto机器人控制器
*/

#include <esp_log.h>

#include <cstring>

#include "Otto.h"
#include "application.h"
#include "board.h"
#include "config.h"
#include "iot/thing.h"
#include "sdkconfig.h"

#define TAG "otto_controller"

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
    Otto otto;
    TaskHandle_t action_task_handle = nullptr;
    QueueHandle_t action_queue;
    TickType_t last_action_time = 0;
    const TickType_t TASK_TIMEOUT = pdMS_TO_TICKS(30000);  // 30秒没动作就自动停止任务

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
    };

    // 限制数值在指定范围内
    static int limit(int value, int min, int max) {
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

    static void action_task(void* arg) {
        OttoController* controller = static_cast<OttoController*>(arg);
        OttoActionParams params;
        controller->last_action_time = xTaskGetTickCount();
        controller->otto.attachServos();

        while (true) {
            if (xQueueReceive(controller->action_queue, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
                ESP_LOGI(TAG, "执行动作: %d", params.action_type);
                controller->last_action_time = xTaskGetTickCount();

                switch (params.action_type) {
                    case ACTION_WALK:
                        controller->otto.walk(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_TURN:
                        controller->otto.turn(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_JUMP:
                        controller->otto.jump(params.steps, params.speed);
                        break;
                    case ACTION_SWING:
                        controller->otto.swing(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_MOONWALK:
                        controller->otto.moonwalker(params.steps, params.speed, params.amount,
                                                    params.direction);
                        break;
                    case ACTION_BEND:
                        controller->otto.bend(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_SHAKE_LEG:
                        controller->otto.shakeLeg(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_UPDOWN:
                        controller->otto.updown(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_TIPTOE_SWING:
                        controller->otto.tiptoeSwing(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_JITTER:
                        controller->otto.jitter(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_ASCENDING_TURN:
                        controller->otto.ascendingTurn(params.steps, params.speed, params.amount);
                        break;
                    case ACTION_CRUSAITO:
                        controller->otto.crusaito(params.steps, params.speed, params.amount,
                                                  params.direction);
                        break;
                    case ACTION_FLAPPING:
                        controller->otto.flapping(params.steps, params.speed, params.amount,
                                                  params.direction);
                        break;
                }

                controller->otto.home();

            } else if ((xTaskGetTickCount() - controller->last_action_time) >
                       controller->TASK_TIMEOUT) {
                ESP_LOGI(TAG, "动作任务超时，自动停止");
                controller->otto.home();
                controller->action_task_handle = nullptr;
                controller->otto.detachServos();
                vTaskDelete(NULL);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

public:
    OttoController() : Thing("OttoController", "Otto机器人的控制器") {
        otto.init(LeftLeg, RightLeg, LeftFoot, RightFoot);
        otto.home();

        action_queue = xQueueCreate(10, sizeof(OttoActionParams));

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("suspend", "清空动作队列,中断Otto机器人动作", ParameterList(),
                           [this](const ParameterList& parameters) {
                               ESP_LOGI(TAG, "停止Otto机器人动作");
                               if (action_task_handle != nullptr) {
                                   vTaskDelete(action_task_handle);
                                   action_task_handle = nullptr;
                               }
                               xQueueReset(action_queue);
                               otto.home();
                           });

        methods_.AddMethod(
            "AIControl", "AI把机器人待执行动作加入队列,动作需要时间",
            ParameterList(
                {Parameter("action_type",
                           "动作类型: 1=行走(前后), 2=转向（左右）, 3=跳跃, 4=摇摆, 5=太空步, "
                           "6=弯曲, 7=摇腿, "
                           "8=上下运动, 9=脚尖摇摆, 10=抖动, 11=上升转弯, 12=十字步, 13=拍打",
                           kValueTypeNumber, false),
                 Parameter("steps", "步数", kValueTypeNumber, false),
                 Parameter("speed", "速度 (越小越快500-3000)默认1000", kValueTypeNumber, false),
                 Parameter("direction", "方向 (1=左/前, -1=右/后)", kValueTypeNumber, true),
                 Parameter(
                     "amount",
                     "动作幅度(最小10) 每个动作限制不一样:摇摆10-50, 太空步15-40"
                     "上下运动10-60, 脚尖摇摆10-50, 抖动5-25, 上升转弯5-15, 十字步20-50, 拍打10-30",
                     kValueTypeNumber, true)}),
            [this](const ParameterList& parameters) {
                int action_type = parameters["action_type"].number();
                int steps = parameters["steps"].number();
                int speed = parameters["speed"].number();
                int direction = parameters["direction"].number();
                int amount = parameters["amount"].number();

                action_type = limit(action_type, ACTION_WALK, ACTION_FLAPPING);
                steps = limit(steps, 1, 100);
                speed = limit(speed, 500, 3000);
                direction = limit(direction, -1, 1);

                switch (action_type) {
                    case ACTION_SWING:
                        amount = limit(amount, 10, 50);
                        break;
                    case ACTION_MOONWALK:
                        amount = limit(amount, 15, 40);
                        break;
                    case ACTION_UPDOWN:
                        amount = limit(amount, 10, 60);
                        break;
                    case ACTION_TIPTOE_SWING:
                        amount = limit(amount, 10, 50);
                        break;
                    case ACTION_JITTER:
                        amount = limit(amount, 5, 25);
                        break;
                    case ACTION_ASCENDING_TURN:
                        amount = limit(amount, 5, 15);
                        break;
                    case ACTION_CRUSAITO:
                        amount = limit(amount, 20, 50);
                        break;
                    case ACTION_FLAPPING:
                        amount = limit(amount, 10, 30);
                        break;
                    default:
                        amount = limit(amount, 10, 50);
                }

                ESP_LOGI(TAG, "AI控制: 动作类型=%d, 步数=%d, 速度=%d, 方向=%d, 幅度=%d",
                         action_type, steps, speed, direction, amount);

                OttoActionParams params;
                params.action_type = action_type;
                params.steps = steps;
                params.speed = speed;
                params.direction = direction;
                params.amount = amount;

                xQueueSend(action_queue, &params, portMAX_DELAY);

                startActionTaskIfNeeded();
            });
    }

    void startActionTaskIfNeeded() {
        if (action_task_handle == nullptr) {
            xTaskCreate(action_task, "otto_action", 1024 * 3, this, 2, &action_task_handle);
        }
    }

    ~OttoController() {
        if (action_task_handle != nullptr) {
            vTaskDelete(action_task_handle);
        }
        vQueueDelete(action_queue);
    }
};

}  // namespace iot

DECLARE_THING(OttoController);
