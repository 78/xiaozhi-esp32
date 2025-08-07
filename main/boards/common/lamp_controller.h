#ifndef __LAMP_CONTROLLER_H__
#define __LAMP_CONTROLLER_H__

#include "mcp_server.h"


class LampController {
private:
    bool power_ = false;
    gpio_num_t gpio_num_;

public:
    LampController(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << gpio_num_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        gpio_set_level(gpio_num_, 0);

        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.lamp.get_state", "Get the power state of the lamp", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return power_ ? "{\"power\": true}" : "{\"power\": false}";
        });

        mcp_server.AddTool("self.lamp.turn_on", "Turn on the lamp", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            power_ = true;
            gpio_set_level(gpio_num_, 1);
            return true;
        });

        mcp_server.AddTool("self.lamp.turn_off", "Turn off the lamp", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            power_ = false;
            gpio_set_level(gpio_num_, 0);
            return true;
        });
    }
};


#endif // __LAMP_CONTROLLER_H__
