#ifndef __IR_FILTER_CONTROLLER_H__
#define __IR_FILTER_CONTROLLER_H__

#include "mcp_server.h"


class IrFilterController {
private:
    bool enable_ = false;
    gpio_num_t gpio_num_;

public:
    IrFilterController(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
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
        mcp_server.AddTool("self.camera.get_ir_filter_state", "Get the state of the camera's infrared filter", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return enable_ ? "{\"enable\": true}" : "{\"enable\": false}";
        });

        mcp_server.AddTool("self.camera.enable_ir_filter", "Enable the camera's infrared filter", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            enable_ = true;
            gpio_set_level(gpio_num_, 1);
            return true;
        });

        mcp_server.AddTool("self.camera.disable_ir_filter", "Disable the camera's infrared filter", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            enable_ = false;
            gpio_set_level(gpio_num_, 0);
            return true;
        });
    }
};


#endif // __IR_FILTER_CONTROLLER_H__
