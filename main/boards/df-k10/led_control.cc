#include "led_control.h"
#include "settings.h"
#include "mcp_server.h"
#include <esp_log.h>

#define TAG "LedStripControl"


int LedStripControl::LevelToBrightness(int level) const {
    if (level < 0) level = 0;
    if (level > 8) level = 8;
    return (1 << level) - 1;  // 2^n - 1
}

StripColor LedStripControl::RGBToColor(int red, int green, int blue) {
    return {static_cast<uint8_t>(red), static_cast<uint8_t>(green), static_cast<uint8_t>(blue)};
}

LedStripControl::LedStripControl(CircularStrip* led_strip) 
    : led_strip_(led_strip) {
    // 从设置中读取亮度等级
    Settings settings("led_strip");
    brightness_level_ = settings.GetInt("brightness", 4);  // 默认等级4
    led_strip_->SetBrightness(LevelToBrightness(brightness_level_), 4);

    auto& mcp_server = McpServer::GetInstance();
    mcp_server.AddTool("self.led_strip.get_brightness",
        "Get the brightness of the led strip (0-8)",
        PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return brightness_level_;
        });

    mcp_server.AddTool("self.led_strip.set_brightness",
        "Set the brightness of the led strip (0-8)",
        PropertyList({
            Property("level", kPropertyTypeInteger, 0, 8)
        }), [this](const PropertyList& properties) -> ReturnValue {
            int level = properties["level"].value<int>();
            ESP_LOGI(TAG, "Set LedStrip brightness level to %d", level);
            brightness_level_ = level;
            led_strip_->SetBrightness(LevelToBrightness(brightness_level_), 4);

            // 保存设置
            Settings settings("led_strip", true);
            settings.SetInt("brightness", brightness_level_);

            return true;
        });

    mcp_server.AddTool("self.led_strip.set_single_color", 
        "Set the color of a single led.", 
        PropertyList({
            Property("index", kPropertyTypeInteger, 0, 2),
            Property("red", kPropertyTypeInteger, 0, 255),
            Property("green", kPropertyTypeInteger, 0, 255),
            Property("blue", kPropertyTypeInteger, 0, 255)
        }), [this](const PropertyList& properties) -> ReturnValue {
            int index = properties["index"].value<int>();
            int red = properties["red"].value<int>();
            int green = properties["green"].value<int>();
            int blue = properties["blue"].value<int>();
            ESP_LOGI(TAG, "Set led strip single color %d to %d, %d, %d",
                index, red, green, blue);
            led_strip_->SetSingleColor(index, RGBToColor(red, green, blue));
            return true;
        });

    mcp_server.AddTool("self.led_strip.set_all_color", 
        "Set the color of all leds.", 
        PropertyList({
            Property("red", kPropertyTypeInteger, 0, 255),
            Property("green", kPropertyTypeInteger, 0, 255),
            Property("blue", kPropertyTypeInteger, 0, 255)
        }), [this](const PropertyList& properties) -> ReturnValue {
            int red = properties["red"].value<int>();
            int green = properties["green"].value<int>();
            int blue = properties["blue"].value<int>();
            ESP_LOGI(TAG, "Set led strip all color to %d, %d, %d",
                red, green, blue);
            led_strip_->SetAllColor(RGBToColor(red, green, blue));
            return true;
        });

    mcp_server.AddTool("self.led_strip.blink", 
        "Blink the led strip. (闪烁)", 
        PropertyList({
            Property("red", kPropertyTypeInteger, 0, 255),
            Property("green", kPropertyTypeInteger, 0, 255),
            Property("blue", kPropertyTypeInteger, 0, 255),
            Property("interval", kPropertyTypeInteger, 0, 1000)
        }), [this](const PropertyList& properties) -> ReturnValue {
            int red = properties["red"].value<int>();
            int green = properties["green"].value<int>();
            int blue = properties["blue"].value<int>();
            int interval = properties["interval"].value<int>();
            ESP_LOGI(TAG, "Blink led strip with color %d, %d, %d, interval %dms",
                red, green, blue, interval);
            led_strip_->Blink(RGBToColor(red, green, blue), interval);
            return true;
        });

    mcp_server.AddTool("self.led_strip.scroll", 
        "Scroll the led strip. (跑马灯)", 
        PropertyList({
            Property("red", kPropertyTypeInteger, 0, 255),
            Property("green", kPropertyTypeInteger, 0, 255),
            Property("blue", kPropertyTypeInteger, 0, 255),
            Property("length", kPropertyTypeInteger, 1, 7),
            Property("interval", kPropertyTypeInteger, 0, 1000)
        }), [this](const PropertyList& properties) -> ReturnValue {
            int red = properties["red"].value<int>();
            int green = properties["green"].value<int>();
            int blue = properties["blue"].value<int>();
            int interval = properties["interval"].value<int>();
            int length = properties["length"].value<int>();
            ESP_LOGI(TAG, "Scroll led strip with color %d, %d, %d, length %d, interval %dms",
                red, green, blue, length, interval);
            StripColor low = RGBToColor(4, 4, 4);
            StripColor high = RGBToColor(red, green, blue);
            led_strip_->Scroll(low, high, length, interval);
            return true;
        });

}
