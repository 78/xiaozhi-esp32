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
    int height;
};

class OttoController : public Thing {
private:
    Otto otto;
    TaskHandle_t action_task_handle = nullptr;
    bool stop_requested = false;

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

    static void action_task(void* arg) {
        OttoController* controller = static_cast<OttoController*>(arg);
        OttoActionParams params;

        while (!controller->stop_requested) {
            if (xQueueReceive(controller->action_queue, &params, pdMS_TO_TICKS(100)) == pdTRUE) {
                ESP_LOGI(TAG, "执行动作: %d, 步数: %d, 速度: %d", params.action_type, params.steps,
                         params.speed);

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
                        controller->otto.swing(params.steps, params.speed, params.height);
                        break;
                    case ACTION_MOONWALK:
                        controller->otto.moonwalker(params.steps, params.speed, params.height,
                                                    params.direction);
                        break;
                    case ACTION_BEND:
                        controller->otto.bend(params.steps, params.speed, params.direction);
                        break;
                    case ACTION_SHAKE_LEG:
                        controller->otto.shakeLeg(params.steps, params.speed, params.direction);
                        break;
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        controller->stop_requested = false;

        controller->otto.home();

        controller->action_task_handle = nullptr;
        vTaskDelete(NULL);
    }

    QueueHandle_t action_queue;

public:
    OttoController() : Thing("OttoController", "Otto机器人的控制器") {
        otto.init(LeftLeg, RightLeg, LeftFoot, RightFoot, true);
        otto.jump(1, 1000);

        action_queue = xQueueCreate(5, sizeof(OttoActionParams));

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("Stop", "停止所有动作并回到初始位置", ParameterList(),
                           [this](const ParameterList& parameters) {
                               ESP_LOGI(TAG, "停止Otto机器人动作");

                               if (action_task_handle != nullptr) {
                                   stop_requested = true;

                                   for (int i = 0; i < 10; i++) {
                                       if (action_task_handle == nullptr)
                                           break;
                                       vTaskDelay(pdMS_TO_TICKS(100));
                                   }

                                   if (action_task_handle != nullptr) {
                                       vTaskDelete(action_task_handle);
                                       action_task_handle = nullptr;

                                       stop_requested = false;

                                       xQueueReset(action_queue);

                                       otto.home();
                                   }
                               } else {
                                   otto.home();
                               }
                           });

        methods_.AddMethod(
            "AIControl", "AI控制机器人执行动作",
            ParameterList(
                {Parameter("action_type",
                           "动作类型: 1=行走, 2=转向, 3=跳跃, 4=摇摆, 5=太空步, 6=弯曲, 7=摇腿, "
                           "8=上下运动, 9=脚尖摇摆, 10=抖动, 11=上升转弯, 12=十字步, 13=拍打",
                           kValueTypeNumber, false),
                 Parameter("steps", "步数", kValueTypeNumber, false),
                 Parameter("speed", "速度 (越小越快500-3000)", kValueTypeNumber, false),
                 Parameter("direction", "方向 (通常1=左/前, -1=右/后)", kValueTypeNumber, true),
                 Parameter("height", "高度", kValueTypeNumber, true)}),
            [this](const ParameterList& parameters) {
                int action_type = parameters["action_type"].number();
                int steps = parameters["steps"].number();
                int speed = parameters["speed"].number();
                int direction = parameters["direction"].number();
                int height = parameters["height"].number();

                // 参数验证
                steps = steps <= 0 ? 1 : steps;
                speed = speed < 500 ? 500 : (speed > 3000 ? 3000 : speed);
                direction = (direction != 1 && direction != -1) ? 1 : direction;

                ESP_LOGI(TAG, "AI控制: 动作类型=%d, 步数=%d, 速度=%d, 方向=%d, 高度=%d",
                         action_type, steps, speed, direction, height);

                startActionTaskIfNeeded();

                OttoActionParams params;
                params.action_type = action_type;
                params.steps = steps;
                params.speed = speed;
                params.direction = direction;
                params.height = height;

                xQueueSend(action_queue, &params, portMAX_DELAY);
            });
    }

    void startActionTaskIfNeeded() {
        if (action_task_handle == nullptr) {
            stop_requested = false;
            xTaskCreate(action_task, "otto_action", 4096, this, 5, &action_task_handle);
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
