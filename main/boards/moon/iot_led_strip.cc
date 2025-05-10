/*
 * @Date: 2025-04-08 22:17:39
 * @LastEditors: zhouke
 * @LastEditTime: 2025-04-09 20:46:03
 * @FilePath: \xiaozhi-esp32\main\boards\abrobot-1.28tft-wifi\iot_led_strip.cc
 */
#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>

#define TAG "ColorStrip"
#define MAX_EFFECT_MODE 4
#include "settings.h"
#include "ws2812_task.h"
namespace iot {

// 这里仅定义 Lamp 的属性和方法，不包含具体的实现
class ColorStrip : public Thing {
private:
 
    bool power_ = false;
    uint8_t brightness_ = 100;
    uint8_t effect_mode_ = 0;

 
public:
    ColorStrip() : Thing("ColorStrip", "LED 彩灯, 可以调节亮度和灯效")  {
        // 从系统配置中读取亮度和模式
        Settings settings("led_strip");
        brightness_ = settings.GetInt("brightness", 100);
        effect_mode_ = settings.GetInt("effect_mode", 0);

        ESP_LOGI(TAG, "WS2812亮度: %d", brightness_);
        ESP_LOGI(TAG, "WS2812律动模式: %d", effect_mode_);
        
        // 定义设备的属性
        properties_.AddBooleanProperty("power", "彩灯是否打开", [this]() -> bool {
            return power_;
        });

        properties_.AddBooleanProperty("brightness", "彩灯的亮度", [this]() -> uint8_t {
            return brightness_;
        });

        properties_.AddBooleanProperty("effect_mode", "彩灯的模式", [this]() -> uint8_t {
            return effect_mode_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("TurnOn", "打开彩灯", ParameterList(), [this](const ParameterList& parameters) {
            power_ = true;
             ws2812_turn_on();
        });

        methods_.AddMethod("TurnOff", "关闭彩灯", ParameterList(), [this](const ParameterList& parameters) {
            power_ = false;
             ws2812_turn_off();
        });


        methods_.AddMethod("SetBrightness", "设置彩灯亮度", ParameterList({
            Parameter("brightness", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            brightness_ = static_cast<uint8_t>(parameters["brightness"].number());
            //Settings settings("led_strip");
            //settings.SetInt("brightness", brightness_);
            ws2812_set_brightness(brightness_);
        });

        methods_.AddMethod("ChangeEffectMode", "切换彩灯模式", ParameterList(), [this](const ParameterList& parameters) {
            effect_mode_ = (effect_mode_ + 1) % MAX_EFFECT_MODE;
            //Settings settings("led_strip");
            //settings.SetInt("effect_mode", effect_mode_);
            ws2812_set_wave_mode(effect_mode_);
        });

    }
};

} // namespace iot

DECLARE_THING(ColorStrip);
