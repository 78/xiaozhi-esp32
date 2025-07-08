
#include <esp_log.h>
#include "iot/thing.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "audio_led_meter.h"

#define TAG "Ws2812Controller"

uint8_t g_color_r = 0;
uint8_t g_color_g = 255;
uint8_t g_color_b = 0;

namespace iot
{
    enum Ws2812EffectType {
        EFFECT_OFF = 0,
        EFFECT_BREATH = 1,
        EFFECT_VOLUME = 2,
        EFFECT_RAINBOW = 3,
        EFFECT_MARQUEE = 4
    };

    class Ws2812Controller : public Thing
    {
    private:
        led_strip_handle_t led_strip_ = nullptr;
        TaskHandle_t effect_task_handle_ = nullptr;
        volatile Ws2812EffectType effect_type_ = EFFECT_OFF;
        volatile bool running_ = false;

        // 动态颜色
        uint8_t color_r_ = 0;
        uint8_t color_g_ = 255;
        uint8_t color_b_ = 0;

        int breath_delay_ms_ = 40; // 默认40ms，呼吸频率减慢一半
        int brightness_ = 100;     // 亮度百分比，0~100

        // 亮度缩放工具
        uint8_t scale(uint8_t c) const {
            return (uint8_t)((int)c * brightness_ / 100);
        }

        static void EffectTask(void *arg) {
            Ws2812Controller *self = static_cast<Ws2812Controller *>(arg);
            int dir = 1, brightness = 0;
            static int rainbow_base = 0;
            static int marquee_pos = 0;
            ESP_LOGI(TAG, "WS2812灯效任务开始运行");
            while (self->running_) {
                if (self->effect_type_ == EFFECT_BREATH) {
                    // 呼吸灯
                    for (int i = 0; i < WS2812_LED_NUM_USED; i++) {
                        uint8_t r = self->scale(self->color_r_ * brightness / 80);
                        uint8_t g = self->scale(self->color_g_ * brightness / 80);
                        uint8_t b = self->scale(self->color_b_ * brightness / 80);
                        led_strip_set_pixel(self->led_strip_, i, r, g, b);
                    }
                    for (int i = WS2812_LED_NUM_USED; i < WS2812_LED_NUM; i++) {
                        led_strip_set_pixel(self->led_strip_, i, 0, 0, 0);
                    }
                    led_strip_refresh(self->led_strip_);
                    brightness += dir * 5;
                    if (brightness >= 80) { brightness = 80; dir = -1; }
                    if (brightness <= 0)  { brightness = 0; dir = 1; }
                    vTaskDelay(pdMS_TO_TICKS(self->breath_delay_ms_));
                } else if (self->effect_type_ == EFFECT_RAINBOW) {
                    // 彩虹灯效
                    for (int i = 0; i < WS2812_LED_NUM_USED; i++) {
                        int pos = (rainbow_base + i * 256 / WS2812_LED_NUM_USED) % 256;
                        uint8_t r, g, b;
                        if (pos < 85) {
                            r = pos * 3; g = 255 - pos * 3; b = 0;
                        } else if (pos < 170) {
                            pos -= 85;
                            r = 255 - pos * 3; g = 0; b = pos * 3;
                        } else {
                            pos -= 170;
                            r = 0; g = pos * 3; b = 255 - pos * 3;
                        }
                        led_strip_set_pixel(self->led_strip_, i, self->scale(r), self->scale(g), self->scale(b));
                    }
                    for (int i = WS2812_LED_NUM_USED; i < WS2812_LED_NUM; i++) {
                        led_strip_set_pixel(self->led_strip_, i, 0, 0, 0);
                    }
                    led_strip_refresh(self->led_strip_);
                    rainbow_base = (rainbow_base + 5) % 256;
                    vTaskDelay(pdMS_TO_TICKS(50));
                } else if (self->effect_type_ == EFFECT_MARQUEE) {
                    // 跑马灯
                    for (int i = 0; i < WS2812_LED_NUM_USED; i++) {
                        if (i == marquee_pos)
                            led_strip_set_pixel(self->led_strip_, i, self->scale(self->color_r_), self->scale(self->color_g_), self->scale(self->color_b_));
                        else
                            led_strip_set_pixel(self->led_strip_, i, 0, 0, 0);
                    }
                    for (int i = WS2812_LED_NUM_USED; i < WS2812_LED_NUM; i++) {
                        led_strip_set_pixel(self->led_strip_, i, 0, 0, 0);
                    }
                    led_strip_refresh(self->led_strip_);
                    marquee_pos = (marquee_pos + 1) % WS2812_LED_NUM_USED;
                    vTaskDelay(pdMS_TO_TICKS(80));
                } else {
                    // 关灯
                    for (int i = 0; i < WS2812_LED_NUM; i++) {
                        led_strip_set_pixel(self->led_strip_, i, 0, 0, 0);
                    }
                    led_strip_refresh(self->led_strip_);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            }
            // 退出时关灯
            for (int i = 0; i < WS2812_LED_NUM; i++) {
                led_strip_set_pixel(self->led_strip_, i, 0, 0, 0);
            }
            led_strip_refresh(self->led_strip_);
            self->effect_task_handle_ = nullptr;
            vTaskDelete(NULL);
        }

        void StartEffectTask() {
            if (!running_) {
                running_ = true;
                xTaskCreate(EffectTask, "ws2812_effect", 4096, this, 5, &effect_task_handle_);
            }
        }

        void StopEffectTask() {
            running_ = false;
            effect_type_ = EFFECT_OFF;
            while (effect_task_handle_ != nullptr) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

    public:
        Ws2812Controller() : Thing("Ws2812Controller", "WS2812灯带控制器")
        {
            ESP_LOGI(TAG, "初始化WS2812灯带控制器");
            led_strip_config_t strip_config = {
                .strip_gpio_num = WS2812_GPIO,
                .max_leds = WS2812_LED_NUM,
                .led_model = LED_MODEL_WS2812,
                .flags = {
                    .invert_out = false
                }
            };
            led_strip_rmt_config_t rmt_config = {
                .clk_src = RMT_CLK_SRC_DEFAULT,
                .resolution_hz = 10 * 1000 * 1000,
                .flags = {
                    .with_dma = false
                }
            };
            ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
            led_strip_clear(led_strip_);

            // 传给 audio_led_meter
            audio_led_meter_set_strip(led_strip_);

            ESP_LOGI(TAG, "TEST: WS2812灯带初始化完成");

            // 呼吸灯
            methods_.AddMethod("breath", "呼吸灯效果", ParameterList(), [this](const ParameterList &) {
                audio_led_meter_enable(0);
                ESP_LOGI(TAG, "设置呼吸灯效果");
                StopEffectTask();
                for (int i = 0; i < WS2812_LED_NUM; i++) {
                    led_strip_set_pixel(led_strip_, i, 0, 0, 0);
                }
                led_strip_refresh(led_strip_);
                effect_type_ = EFFECT_BREATH;
                StartEffectTask();
            });

            methods_.AddMethod(
                "set_breath_delay", "设置呼吸灯速度，单位ms，越大越慢",
                ParameterList({Parameter("delay", "延迟ms", kValueTypeNumber, false)}),
                [this](const ParameterList &params)
                {
                    int val = params["delay"].number();
                    if (val < 10)
                        val = 10;
                    if (val > 500)
                        val = 500;
                    breath_delay_ms_ = val;
                    ESP_LOGI(TAG, "设置呼吸灯延迟为%dms", breath_delay_ms_);
                });

            // 设置亮度
            methods_.AddMethod(
                "set_brightness", "设置灯带亮度，0~100",
                ParameterList({Parameter("value", "亮度百分比", kValueTypeNumber, false)}),
                [this](const ParameterList &params) {
                    int val = params["value"].number();
                    if (val < 0) val = 0;
                    if (val > 100) val = 100;
                    brightness_ = val;
                    audio_led_meter_set_brightness(val); // 同步到音量律动
                    ESP_LOGI(TAG, "设置亮度为%d%%", brightness_);
                }
            );

            // 音量律动（假律动，真实律动用 audio_led_meter）
            methods_.AddMethod("volume", "音量律动效果", ParameterList(), [this](const ParameterList &) {
                StopEffectTask();
                ESP_LOGI(TAG, "设置音量律动效果");
                for (int i = 0; i < WS2812_LED_NUM; i++) {
                    led_strip_set_pixel(led_strip_, i, 0, 0, 0);
                }
                led_strip_refresh(led_strip_);
                audio_led_meter_enable(1);
            });
            methods_.AddMethod(
                "random_meter_colors", "随机更换音量律动的灯带配色", ParameterList(), [this](const ParameterList &) {
                    audio_led_meter_init_colors(); // 重新随机一组颜色
                    ESP_LOGI(TAG, "已随机更换音量律动的灯带配色");
                }
            );

            methods_.AddMethod(
                "set_meter_single_color", "设置音量律动为单色",
                ParameterList({Parameter("r", "红", kValueTypeNumber, false),
                               Parameter("g", "绿", kValueTypeNumber, false),
                               Parameter("b", "蓝", kValueTypeNumber, false)}),
                [this](const ParameterList &params)
                {
                    uint8_t r = params["r"].number();
                    uint8_t g = params["g"].number();
                    uint8_t b = params["b"].number();
                    audio_led_meter_set_single_color(r, g, b);
                    ESP_LOGI(TAG, "设置音量律动为单色: %d,%d,%d", r, g, b);
                });

            // 彩虹灯效
            methods_.AddMethod("rainbow", "彩虹灯效", ParameterList(), [this](const ParameterList &) {
                audio_led_meter_enable(0);
                StopEffectTask();
                ESP_LOGI(TAG, "设置彩虹灯效");
                for (int i = 0; i < WS2812_LED_NUM; i++) {
                    led_strip_set_pixel(led_strip_, i, 0, 0, 0);
                }
                led_strip_refresh(led_strip_);
                effect_type_ = EFFECT_RAINBOW;
                StartEffectTask();
            });

            // 跑马灯
            methods_.AddMethod("marquee", "跑马灯", ParameterList(), [this](const ParameterList &) {
                audio_led_meter_enable(0);
                StopEffectTask();
                ESP_LOGI(TAG, "设置跑马灯效果");
                for (int i = 0; i < WS2812_LED_NUM; i++) {
                    led_strip_set_pixel(led_strip_, i, 0, 0, 0);
                }
                led_strip_refresh(led_strip_);
                effect_type_ = EFFECT_MARQUEE;
                StartEffectTask();
            });

            // 设置颜色
            methods_.AddMethod(
                "set_color", "设置颜色，可以根据用户的需求自动生成所需要的RGB分量值",
                ParameterList({
                    Parameter("r", "红", kValueTypeNumber, false),
                    Parameter("g", "绿", kValueTypeNumber, false),
                    Parameter("b", "蓝", kValueTypeNumber, false)
                }),
                [this](const ParameterList &params) {
                    color_r_ = params["r"].number();
                    color_g_ = params["g"].number();
                    color_b_ = params["b"].number();
                    g_color_r = color_r_;
                    g_color_g = color_g_;
                    g_color_b = color_b_;
                }
            );

            // 关灯
            methods_.AddMethod("off", "关闭灯带", ParameterList(), [this](const ParameterList &) {
                audio_led_meter_enable(0);
                effect_type_ = EFFECT_OFF;
                StopEffectTask();
                ESP_LOGI(TAG, "关闭灯带");
                for (int i = 0; i < WS2812_LED_NUM; i++) {
                    led_strip_set_pixel(led_strip_, i, 0, 0, 0);
                }
                led_strip_refresh(led_strip_);
            });

            // 默认关闭音量律动
            audio_led_meter_enable(0);
        }

        ~Ws2812Controller() {
            StopEffectTask();
        }
    };

} // namespace iot

DECLARE_THING(Ws2812Controller);