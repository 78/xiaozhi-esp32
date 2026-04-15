#include "sdkconfig.h"
#include "iot/thing.h"
#include "board.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <cstring>

#include "boards/lc-s3-wifi-1.54tft/config.h"

#define TAG "Relay"

namespace iot {

class Relay : public Thing {
private:
    bool power_ = false;                 // 灯的开关状态
    void InitializeGpio() {
        gpio_config_t io_conf = {       
            .pin_bit_mask = 1 << RELAY_LED, // 配置 GPIO    
            .mode = GPIO_MODE_OUTPUT, // 设置为输出模式
            .pull_up_en = GPIO_PULLUP_DISABLE, // 禁用上拉电阻
            .pull_down_en = GPIO_PULLDOWN_DISABLE, // 禁用下拉电阻
            .intr_type = GPIO_INTR_DISABLE, // 禁用中断
        };
        gpio_config(&io_conf); // 应用配置
        gpio_set_level(RELAY_LED,0);
    }

public:
    Relay() : Thing("Relay", "照明灯"){
        // 初始化GPIO
        InitializeGpio();

        // 定义属性：power（表示灯的开关状态）
        properties_.AddBooleanProperty("power", "灯是否打开", [this]() -> bool {
            return power_;
        });

        // 定义方法：TurnOn（打开灯）
        methods_.AddMethod("TurnOn", "打开灯", ParameterList(), [this](const ParameterList& parameters) {
            power_ = true;
            gpio_set_level(RELAY_LED, 1);
        });

        // 定义方法：TurnOff（关闭灯）
        methods_.AddMethod("TurnOff", "关闭灯", ParameterList(), [this](const ParameterList& parameters) {
            power_ = false;
            gpio_set_level(RELAY_LED, 0);
        });
    }
};

} // namespace iot

DECLARE_THING(Relay);
