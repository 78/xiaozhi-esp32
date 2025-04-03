/*
 * @Date: 2025-01-10 20:36:47
 * @LastEditors: zhouke
 * @LastEditTime: 2025-03-11 21:45:31
 * @FilePath: \xiaozhi-esp32\main\iot\things\lamp.cc
 */
#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include "led/single_led.h"
#include "application.h"
#define TAG "ColorLamp"

namespace iot {

// 这里仅定义 ColorLamp 的属性和方法，不包含具体的实现
class ColorLamp : public Thing {
private:
    bool power_ = false;
    int brightness_;
   
    SingleLed *led_;
    std::string color_;

    SingleLed* GetLed()  {
        static SingleLed led(GPIO_NUM_48);
        return &led;
    }

    //初始化灯
    void InitializeGpio() {
        led_ = GetLed();
        brightness_ = 50;
        power_ = false;
        color_ = "白";
    }

    //设置灯的颜色
    void set_led(int brightness, std::string color){
    if(brightness == 0){
        led_->TurnOff();
        return;
    }

    if (color.find("红") != std::string::npos) {
        led_->SetColor(brightness, 0, 0);
    } else if (color.find("橙") != std::string::npos) {
        led_->SetColor(brightness, brightness / 2, 0);
    } else if (color.find("绿") != std::string::npos) {
        led_->SetColor(0, brightness, 0); 
    } else if (color.find("蓝") != std::string::npos) {
        led_->SetColor(0, 0, brightness);
    } else if (color.find("黄") != std::string::npos) {
        led_->SetColor(brightness, brightness, 0);
    } else if (color.find("青") != std::string::npos) {
        led_->SetColor(0, brightness, brightness);
    } else if (color.find("紫") != std::string::npos) {
        led_->SetColor(brightness, 0, brightness);
    } else { // 白色
        led_->SetColor(brightness, brightness, brightness);
    }
    led_->TurnOn();
    }


 
public:
 
    //初始化灯的描述，用一句简单描述让大模型理解这个灯的功能
    ColorLamp() : Thing("ColorLamp", "这是一个可以调节亮度和颜色的智能灯，可以设置红、橙、黄、蓝、绿、青、紫、白几种颜色"), power_(false) {
        InitializeGpio();

        // 定义设备的属性，包括亮灭，颜色
        properties_.AddBooleanProperty("power", "灯是否打开", [this]() -> bool {
            return power_;
        });

        properties_.AddNumberProperty("brightness", "当前亮度值", [this]() -> int {
 
            return brightness_;
        });

        properties_.AddStringProperty("color", "当前颜色", [this]() -> std::string {
 
            return color_;
        });

        // 定义设备可以被远程执行的各个指令
        methods_.AddMethod("TurnOn", "打开灯", ParameterList(), [this](const ParameterList& parameters) {
            power_ = true;
            
            set_led(brightness_, color_);
             ESP_LOGI(TAG, "开灯");
        });

        methods_.AddMethod("TurnOff", "关闭灯", ParameterList(), [this](const ParameterList& parameters) {
            power_ = false;
            ESP_LOGI(TAG, "关灯");
            led_->TurnOff(); 
        });

        methods_.AddMethod("SetBrightness", "设置亮度", ParameterList({
            Parameter("brightness", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            brightness_ = parameters["brightness"].number();
            set_led(brightness_, color_ );
        });

        methods_.AddMethod("SetColor", "设置颜色", ParameterList({
            Parameter("color", "红、橙、黄、蓝、绿、青、紫、白其中一种颜色", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            color_ = parameters["color"].string();
            set_led(brightness_, color_ );
            ESP_LOGI(TAG, "设置颜色为: %s", color_.c_str());
        });
    }
};

} // namespace iot




DECLARE_THING(ColorLamp);