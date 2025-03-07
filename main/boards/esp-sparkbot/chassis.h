/*
    ESP-SparkBot 的底座
    https://gitee.com/esp-friends/esp_sparkbot/tree/master/example/tank/c2_tracked_chassis
*/

#ifndef __CHASSIS_H__
#define __CHASSIS_H__

#include "sdkconfig.h"
#include "iot/thing.h"
#include "communication/simple_comm.h"
#include "board.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <cstring>

#include "boards/esp-sparkbot/config.h"

namespace iot {

class Chassis : public Thing {
private:
    light_mode_t light_mode_;
    SimpleComm* comm_;

    void SendMessage(const char * command_str);

public:
    Chassis(SimpleComm *comm);
};

} // namespace iot

#endif // CHASSIS_H