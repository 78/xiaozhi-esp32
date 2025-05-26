#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include "servo_dog_ctrl.h"

#define TAG "Message"

namespace iot {

class DogAction_basic : public Thing {
private:
    bool is_moving_ = false;

    void InitializePlayer()
    {
        ESP_LOGI(TAG, "Dog action initialized");
    }

public:
    DogAction_basic() : Thing("DogAction_basic", "机器人基础动作控制")
    {
        InitializePlayer();

        // 定义设备的属性
        properties_.AddBooleanProperty("is_moving", "机器人是否正在移动", [this]() -> bool {
            return is_moving_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("forward", "机器人向前移动", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_FORWARD, NULL);
        });

        methods_.AddMethod("backward", "机器人向后移动", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_BACKWARD, NULL);
        });

        methods_.AddMethod("sway_back_forth", "机器人做前后摇摆动作", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_SWAY_BACK_FORTH, NULL);
        });

        methods_.AddMethod("turn_left", "机器人向左转", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_TURN_LEFT, NULL);
        });

        methods_.AddMethod("turn_right", "机器人向右转", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_TURN_RIGHT, NULL);
        });

        methods_.AddMethod("lay_down", "机器人趴下", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_LAY_DOWN, NULL);
        });

        methods_.AddMethod("sway", "机器人做左右摇摆动作", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            dog_action_args_t args = {
                .repeat_count = 4,
            };
            servo_dog_ctrl_send(DOG_STATE_SWAY, &args);
        });
    }
};

} // namespace iot

DECLARE_THING(DogAction_basic);
