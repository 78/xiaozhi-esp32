/*
    ESP-SparkBot 的底座
    https://gitee.com/esp-friends/esp_sparkbot/tree/master/example/tank/c2_tracked_chassis
*/

#include "sdkconfig.h"
#include "iot/thing.h"
#include "board.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <cstring>

#include "boards/esp-sparkbot/config.h"

#define TAG "Chassis"

namespace iot {

class Chassis : public Thing {
private:
    light_mode_t light_mode_ = LIGHT_MODE_ALWAYS_ON;

    void SendUartMessage(const char * command_str) {
        uint8_t len = strlen(command_str);
        uart_write_bytes(ECHO_UART_PORT_NUM, command_str, len);
        ESP_LOGI(TAG, "Sent command: %s", command_str);
    }

    void InitializeEchoUart() {
        uart_config_t uart_config = {
            .baud_rate = ECHO_UART_BAUD_RATE,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        int intr_alloc_flags = 0;

        ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
        ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, UART_ECHO_TXD, UART_ECHO_RXD, UART_ECHO_RTS, UART_ECHO_CTS));

        SendUartMessage("w2");
    }

public:
    Chassis() : Thing("Chassis", "小机器人的底座：有履带可以移动；可以调整灯光效果"), light_mode_(LIGHT_MODE_ALWAYS_ON) {
        InitializeEchoUart();

        // 定义设备的属性
        properties_.AddNumberProperty("light_mode", "灯光效果编号", [this]() -> int {
            return (light_mode_ - 2 <= 0) ? 1 : light_mode_ - 2;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("GoForward", "向前走", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("x0.0 y1.0");
        });

        methods_.AddMethod("GoBack", "向后退", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("x0.0 y-1.0");
        });

        methods_.AddMethod("TurnLeft", "向左转", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("x-1.0 y0.0");
        });

        methods_.AddMethod("TurnRight", "向右转", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("x1.0 y0.0");
        });

        methods_.AddMethod("Dance", "跳舞", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("d1");
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
                SendUartMessage(command_str);
            }
        });
    }
};

} // namespace iot

DECLARE_THING(Chassis);
