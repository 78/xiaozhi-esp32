#include <esp_log.h>
#include "iot/thing.h"
#include "driver/gpio.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define TAG "SwitchController"

// 可根据需要修改为 WS2812 的 GPIO
#ifndef SWITCH_GPIO
#define SWITCH_GPIO WS2812_GPIO
#endif

namespace iot
{
    class SwitchController : public Thing
    {
    private:
        bool state_ = false;

        void SetGpio(bool on) {
            gpio_set_level(SWITCH_GPIO, on ? 1 : 0);
        }

    public:
        SwitchController() : Thing("SwitchController", "通用开关控制器")
        {
            ESP_LOGI(TAG, "初始化开关控制器，GPIO=%d", SWITCH_GPIO);

            // 初始化 GPIO
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL << SWITCH_GPIO);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf);

            SetGpio(false);

            // 开
            methods_.AddMethod("on", "打开开关（高电平）", ParameterList(), [this](const ParameterList &) {
                state_ = true;
                SetGpio(true);
                ESP_LOGI(TAG, "开关已打开");
            });

            // 关
            methods_.AddMethod("off", "关闭开关（低电平）", ParameterList(), [this](const ParameterList &) {
                state_ = false;
                SetGpio(false);
                ESP_LOGI(TAG, "开关已关闭");
            });

            // 查询状态
            methods_.AddMethod("get_state", "获取当前开关状态", ParameterList(), [this](const ParameterList &) {
                return state_ ? "on" : "off";
            });

            methods_.AddMethod("trigger", "模拟按键触发（高电平100ms后恢复低电平）", ParameterList(), [this](const ParameterList &)
                               {
                SetGpio(true);
                vTaskDelay(pdMS_TO_TICKS(100));
                SetGpio(false);
                ESP_LOGI(TAG, "已模拟按键触发"); });
        }

        ~SwitchController() {
            SetGpio(false);
        }
    };

} // namespace iot

DECLARE_THING(SwitchController);