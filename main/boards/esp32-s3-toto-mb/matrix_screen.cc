#include "sdkconfig.h"
#include "iot/thing.h"
#include "board.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <cstring>

#include "config.h"

#define TAG "MatrixScreen"

namespace iot {

class MatrixScreen : public Thing {
private:
    // 校验和计算
    uint8_t calculate_checksum(uint8_t *data, int len) {
        uint8_t sum = 0;
        for (int i = 0; i < len; i++) {
            sum += data[i];
        }
        return sum;
    }

    void SendUartMessage(uint16_t anim_index) {

        gpio_set_direction(UART_LED_TXD, GPIO_MODE_INPUT);

        uint8_t high_byte = (anim_index >> 8) & 0xFF; // 高位字节
        uint8_t low_byte  = anim_index & 0xFF;        // 低位字节
        uint8_t cmd_buf[6];
        cmd_buf[0] = 0x4A;
        cmd_buf[1] = 0x42;
        cmd_buf[2] = 0x01;
        cmd_buf[3] = low_byte;
        cmd_buf[4] = high_byte;
        cmd_buf[5] = calculate_checksum(cmd_buf, 5);
        // 唤醒设备
        gpio_set_level(UART_LED_TXD, 0);
        vTaskDelay(pdMS_TO_TICKS(2));
        vTaskDelay(pdMS_TO_TICKS(100));

        // 发送数据
        uart_write_bytes(LED_UART_PORT_NUM, (const char *)cmd_buf, sizeof(cmd_buf));
    }

    void InitializeLedUart() {
        uart_config_t uart_config = {
            .baud_rate = LED_UART_BAUD_RATE,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        int intr_alloc_flags = 0;

        ESP_ERROR_CHECK(uart_driver_install(LED_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
        ESP_ERROR_CHECK(uart_param_config(LED_UART_PORT_NUM, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(LED_UART_PORT_NUM, UART_LED_TXD, UART_LED_RXD, UART_LED_RTS, UART_LED_CTS));
    }

public:
    MatrixScreen() : Thing("MatrixScreen", "矩阵屏幕，显示表情") {
        InitializeLedUart();

        // 定义设备可以被远程执行的指令
        // methods_.AddMethod("ShowAnim", "显示表情", ParameterList({
        //     Parameter("anim_index", "1到6之间的整数", kValueTypeNumber, true)
        // }), [this](const ParameterList& parameters) {
        methods_.AddMethod("ShowAnim", "显示表情", ParameterList(), [this](const ParameterList& parameters) {
            // uint16_t animIndex = parameters["anim_index"].number();
            std::srand(std::time(0));
            uint16_t animIndex = static_cast<uint16_t>(std::rand() % 5 + 1); // 0 ~ 5
            SendUartMessage(animIndex);
        });
    }
};

} // namespace iot

DECLARE_THING(MatrixScreen);
