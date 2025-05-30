#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include "servo_dog_ctrl.h"

#define TAG "Message"

namespace iot {

class DogAction_extra : public Thing {
private:
    bool is_moving_ = false;

    void InitializePlayer()
    {
        ESP_LOGI(TAG, "Dog action initialized");
    }

public:
    DogAction_extra() : Thing("DogAction_extra", "机器人扩展动作控制")
    {
        InitializePlayer();

        // 定义设备的属性
        properties_.AddBooleanProperty("is_moving", "机器人是否正在移动", [this]() -> bool {
            return is_moving_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("retract_legs", "机器人收回腿部", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_RETRACT_LEGS, NULL);
        });

        methods_.AddMethod("stop", "立即停止机器人当前动作", ParameterList(), [this](const ParameterList & parameters) {
            if (is_moving_) {
                is_moving_ = false;
                servo_dog_ctrl_send(DOG_STATE_IDLE, NULL);
            }
        });

        methods_.AddMethod("shake_hand", "机器人做握手动作", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_SHAKE_HAND, NULL);
        });

        methods_.AddMethod("shake_back_legs", "机器人伸懒腰", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_SHAKE_BACK_LEGS, NULL);
        });

        methods_.AddMethod("jump_forward", "机器人向前跳跃", ParameterList(), [this](const ParameterList & parameters) {
            is_moving_ = true;
            servo_dog_ctrl_send(DOG_STATE_JUMP_FORWARD, NULL);
        });
    }
};

} // namespace iot

DECLARE_THING(DogAction_extra);
