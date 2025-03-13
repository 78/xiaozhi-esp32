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

#define TAG "Chassis"  // 定义日志标签

namespace iot {

// Chassis类，继承自Thing，表示机器人的底盘
class Chassis : public Thing {
private:
    light_mode_t light_mode_ = LIGHT_MODE_ALWAYS_ON;  // 灯光模式，默认为常亮

    // 通过UART发送消息
    void SendUartMessage(const char * command_str) {
        uint8_t len = strlen(command_str);  // 获取命令字符串长度
        uart_write_bytes(ECHO_UART_PORT_NUM, command_str, len);  // 通过UART发送命令
        ESP_LOGI(TAG, "Sent command: %s", command_str);  // 记录日志
    }

    // 初始化UART通信
    void InitializeEchoUart() {
        uart_config_t uart_config = {
            .baud_rate = ECHO_UART_BAUD_RATE,  // 波特率
            .data_bits = UART_DATA_8_BITS,  // 数据位
            .parity    = UART_PARITY_DISABLE,  // 无校验位
            .stop_bits = UART_STOP_BITS_1,  // 停止位
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  // 无流控制
            .source_clk = UART_SCLK_DEFAULT,  // 时钟源
        };
        int intr_alloc_flags = 0;  // 中断分配标志

        // 安装UART驱动程序
        ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
        // 配置UART参数
        ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
        // 设置UART引脚
        ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, UART_ECHO_TXD, UART_ECHO_RXD, UART_ECHO_RTS, UART_ECHO_CTS));

        // 发送初始化命令
        SendUartMessage("w2");
    }

public:
    // 构造函数
    Chassis() : Thing("Chassis", "小机器人的底座：有履带可以移动；可以调整灯光效果"), light_mode_(LIGHT_MODE_ALWAYS_ON) {
        InitializeEchoUart();  // 初始化UART通信

        // 定义设备的属性
        properties_.AddNumberProperty("light_mode", "灯光效果编号", [this]() -> int {
            return (light_mode_ - 2 <= 0) ? 1 : light_mode_ - 2;  // 返回灯光模式编号
        });

        // 定义设备可以被远程执行的指令

        // 向前走
        methods_.AddMethod("GoForward", "向前走", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("x0.0 y1.0");  // 发送向前走的命令
        });

        // 向后退
        methods_.AddMethod("GoBack", "向后退", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("x0.0 y-1.0");  // 发送向后退的命令
        });

        // 向左转
        methods_.AddMethod("TurnLeft", "向左转", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("x-1.0 y0.0");  // 发送向左转的命令
        });

        // 向右转
        methods_.AddMethod("TurnRight", "向右转", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("x1.0 y0.0");  // 发送向右转的命令
        });

        // 跳舞
        methods_.AddMethod("Dance", "跳舞", ParameterList(), [this](const ParameterList& parameters) {
            SendUartMessage("d1");  // 发送跳舞的命令
            light_mode_ = LIGHT_MODE_MAX;  // 设置灯光模式为最大值
        });

        // 切换灯光模式
        methods_.AddMethod("SwitchLightMode", "打开灯", ParameterList({
            Parameter("lightmode", "1到6之间的整数", kValueTypeNumber, true)  // 参数：灯光模式编号
        }), [this](const ParameterList& parameters) {
            char command_str[5] = {'w', 0, 0};  // 构造命令字符串
            char mode = static_cast<char>(parameters["lightmode"].number()) + 2;  // 获取灯光模式编号

            ESP_LOGI(TAG, "Input Light Mode: %c", (mode + '0'));  // 记录日志

            if (mode >= 3 && mode <= 8) {  // 检查模式编号是否有效
                command_str[1] = mode + '0';  // 设置命令字符串
                SendUartMessage(command_str);  // 发送切换灯光模式的命令
            }
        });
    }
};

} // namespace iot

DECLARE_THING(Chassis);  // 声明Chassis设备