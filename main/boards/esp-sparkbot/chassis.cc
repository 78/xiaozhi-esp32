/*
    ESP-SparkBot 的底座
    https://gitee.com/esp-friends/esp_sparkbot/tree/master/example/tank/c2_tracked_chassis
*/

#include "chassis.h"

#define TAG "Chassis"

namespace iot {

Chassis::Chassis(SimpleComm *comm) : Thing("Chassis", "小机器人的底座：有履带可以移动；可以调整灯光效果"), 
            light_mode_(LIGHT_MODE_ALWAYS_ON), comm_(comm) {

    // 定义设备的属性
    properties_.AddNumberProperty("light_mode", "灯光效果编号", [this]() -> int {
        return (light_mode_ - 2 <= 0) ? 1 : light_mode_ - 2;
    });

    // 定义设备可以被远程执行的指令
    methods_.AddMethod("GoForward", "向前走", ParameterList(), [this](const ParameterList& parameters) {
        SendMessage("x0.0 y1.0");
    });

    methods_.AddMethod("GoBack", "向后退", ParameterList(), [this](const ParameterList& parameters) {
        SendMessage("x0.0 y-1.0");
    });

    methods_.AddMethod("TurnLeft", "向左转", ParameterList(), [this](const ParameterList& parameters) {
        SendMessage("x-1.0 y0.0");
    });

    methods_.AddMethod("TurnRight", "向右转", ParameterList(), [this](const ParameterList& parameters) {
        SendMessage("x1.0 y0.0");
    });

    methods_.AddMethod("Dance", "跳舞", ParameterList(), [this](const ParameterList& parameters) {
        SendMessage("d1");
        light_mode_ = LIGHT_MODE_MAX;
    });

    methods_.AddMethod("SwitchLightMode", "打开灯", ParameterList({
        Parameter("lightmode", "1到6之间的整数", kValueTypeNumber, true)
    }), [this](const ParameterList& parameters) {
        char command_str[5] = {'w', 0, 0};
        char mode = static_cast<char>(parameters["lightmode"].number()) + 2;

        ESP_LOGI(TAG, "Input Light Mode: %c", (mode + '0'));

        if (mode >= 3 && mode <= 8) {
            command_str[1] = mode + '0';
            SendMessage(command_str);
        }
    });

    SendMessage("w2");
}

void Chassis::SendMessage(const char * command_str) {
    if (comm_) {
        comm_->Send(command_str);
        ESP_LOGI(TAG, "Sent command: %s", command_str);
    } else {
        ESP_LOGE(TAG, "communication channel is not exist!");
    }
}


} // namespace iot
