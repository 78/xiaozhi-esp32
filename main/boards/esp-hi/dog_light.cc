#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include "driver/rmt_tx.h"
#include "led_strip.h"

#define TAG "Light"

static led_strip_handle_t led_strip;

static const led_strip_config_t bsp_strip_config = {
    .strip_gpio_num = GPIO_NUM_8,
    .max_leds = 4,
    .led_model = LED_MODEL_WS2812,
    .flags = {
        .invert_out = false
    }
};

static const led_strip_rmt_config_t bsp_rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10 * 1000 * 1000,
    .flags = {
        .with_dma = false
    }
};

esp_err_t bsp_led_init()
{
    ESP_LOGI(TAG, "BLINK_GPIO setting %d", bsp_strip_config.strip_gpio_num);

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&bsp_strip_config, &bsp_rmt_config, &led_strip));
    led_strip_set_pixel(led_strip, 0, 0x00, 0x00, 0x00);
    led_strip_set_pixel(led_strip, 1, 0x00, 0x00, 0x00);
    led_strip_set_pixel(led_strip, 2, 0x00, 0x00, 0x00);
    led_strip_set_pixel(led_strip, 3, 0x00, 0x00, 0x00);
    led_strip_refresh(led_strip);

    return ESP_OK;
}

esp_err_t bsp_led_rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    esp_err_t ret = ESP_OK;

    ret |= led_strip_set_pixel(led_strip, 0, r, g, b);
    ret |= led_strip_set_pixel(led_strip, 1, r, g, b);
    ret |= led_strip_set_pixel(led_strip, 2, r, g, b);
    ret |= led_strip_set_pixel(led_strip, 3, r, g, b);
    ret |= led_strip_refresh(led_strip);
    return ret;
}

namespace iot {
class DogLight : public Thing {
private:
    bool power_ = false;

    void InitializeGpio()
    {
        bsp_led_init();
        bsp_led_rgb_set(0x00, 0x00, 0x00);
        ESP_LOGI(TAG, "lamp InitializeGpio");
    }

public:
    DogLight() : Thing("DogLight", "机器人头灯"), power_(false)
    {
        InitializeGpio();

        properties_.AddBooleanProperty("power", "灯是否打开", [this]() -> bool {
            return power_;
        });

        methods_.AddMethod("TurnOn", "打开灯", ParameterList(), [this](const ParameterList & parameters) {
            power_ = true;
            bsp_led_rgb_set(0xFF, 0xFF, 0xFF);
            ESP_LOGI(TAG, "lamp TurnOn");
        });

        methods_.AddMethod("TurnOff", "关闭灯", ParameterList(), [this](const ParameterList & parameters) {
            power_ = false;
            bsp_led_rgb_set(0x00, 0x00, 0x00);
            ESP_LOGI(TAG, "lamp TurnOff");
        });

        methods_.AddMethod("SetRGB", "设置RGB颜色",
        ParameterList({
            Parameter("r", "红色值(0-255)", kValueTypeNumber, true),
            Parameter("g", "绿色值(0-255)", kValueTypeNumber, true),
            Parameter("b", "蓝色值(0-255)", kValueTypeNumber, true)
        }), [this](const ParameterList & parameters) {
            int r = parameters["r"].number();
            int g = parameters["g"].number();
            int b = parameters["b"].number();

            r = std::max(0, std::min(255, r));
            g = std::max(0, std::min(255, g));
            b = std::max(0, std::min(255, b));

            power_ = true;
            bsp_led_rgb_set(r, g, b);
            ESP_LOGI(TAG, "lamp SetRGB: r=%d, g=%d, b=%d", r, g, b);
        });
    }
};

} // namespace iot

DECLARE_THING(DogLight);
