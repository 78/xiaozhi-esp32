#include "mcp_tools.h"
#include <esp_log.h>
#include <cJSON.h>

static const char *TAG = "FogSeekMCPTools";

void InitializeLightMCP(
    McpServer &mcp_server,
    GpioLed *cold_light,
    GpioLed *warm_light,
    bool cold_light_state,
    bool warm_light_state)
{
    // 添加获取当前灯状态的工具函数
    mcp_server.AddTool("self.light.get_status",
                       "获取当前灯的状态",
                       PropertyList(),
                       [cold_light_state, warm_light_state](const PropertyList &properties) -> ReturnValue
                       {
                           // 使用字符串拼接方式返回JSON - 项目中最标准的做法
                           std::string status = "{\"cold_light\":" + std::string(cold_light_state ? "true" : "false") +
                                                ",\"warm_light\":" + std::string(warm_light_state ? "true" : "false") + "}";
                           return status;
                       });

    // 添加设置冷暖灯光亮度的工具函数
    mcp_server.AddTool("self.light.set_brightness",
                       "设置冷暖灯光的亮度，冷光和暖光可以独立调节，亮度范围为0-100，关灯为0，开灯默认为30亮度。"
                       "根据用户情绪描述调节冷暖灯光亮度，大模型应该分析用户的话语，理解用户的情绪状态和场景描述，然后根据情绪设置合适的冷暖灯光亮度组合。",
                       PropertyList({Property("cold_brightness", kPropertyTypeInteger, 0, 100),
                                     Property("warm_brightness", kPropertyTypeInteger, 0, 100)}),
                       [cold_light, warm_light, &cold_light_state, &warm_light_state](const PropertyList &properties) -> ReturnValue
                       {
                           // 使用operator[]而不是at()访问属性
                           int cold_brightness = properties["cold_brightness"].value<int>();
                           int warm_brightness = properties["warm_brightness"].value<int>();

                           cold_light->SetBrightness(cold_brightness);
                           warm_light->SetBrightness(warm_brightness);

                           // 只有在亮度大于0时才开启灯光
                           if (cold_brightness > 0)
                           {
                               cold_light->TurnOn();
                           }
                           else
                           {
                               cold_light->TurnOff();
                           }

                           if (warm_brightness > 0)
                           {
                               warm_light->TurnOn();
                           }
                           else
                           {
                               warm_light->TurnOff();
                           }

                           // 更新状态
                           cold_light_state = cold_brightness > 0;
                           warm_light_state = warm_brightness > 0;

                           ESP_LOGI(TAG, "Color temperature set - Cold: %d%%, Warm: %d%%",
                                    cold_brightness, warm_brightness);

                           // 使用字符串拼接方式返回JSON - 项目中最标准的做法
                           std::string result = "{\"success\":true"
                                                ",\"cold_brightness\":" +
                                                std::to_string(cold_brightness) +
                                                ",\"warm_brightness\":" + std::to_string(warm_brightness) + "}";
                           return result;
                       });
}

void InitializeRgbLedMCP(
    McpServer &mcp_server,
    CircularStrip *rgb_strip)
{
    // 添加设置RGB LED灯带颜色的工具函数
    mcp_server.AddTool("self.light.set_rgb_color",
                       "设置RGB LED灯带的颜色，根据用户情绪描述调节灯光颜色亮度，大模型应该分析用户的话语，理解用户的情绪状态和场景描述，然后根据情绪设置合适的灯光颜色亮度组合。",
                       PropertyList({Property("red", kPropertyTypeInteger, 0, 255),
                                     Property("green", kPropertyTypeInteger, 0, 255),
                                     Property("blue", kPropertyTypeInteger, 0, 255),
                                     Property("led_index", kPropertyTypeInteger, -1, 8)}), // -1表示设置所有LED
                       [rgb_strip](const PropertyList &properties) -> ReturnValue
                       {
                           int red = properties["red"].value<int>();
                           int green = properties["green"].value<int>();
                           int blue = properties["blue"].value<int>();
                           int led_index = properties["led_index"].value<int>();

                           StripColor color{static_cast<uint8_t>(red),
                                            static_cast<uint8_t>(green),
                                            static_cast<uint8_t>(blue)};

                           if (led_index == -1)
                           {
                               // 设置所有LED的颜色
                               rgb_strip->SetAllColor(color);
                           }
                           else if (led_index >= 0 && led_index < 8)
                           {
                               // 设置指定索引的LED颜色
                               rgb_strip->SetSingleColor(static_cast<uint8_t>(led_index), color);
                           }
                           else
                           {
                               ESP_LOGE(TAG, "Invalid LED index: %d", led_index);
                               std::string result = "{\"success\":false,\"error\":\"Invalid LED index\"}";
                               return result;
                           }

                           ESP_LOGI(TAG, "RGB LED set - R: %d, G: %d, B: %d, Index: %d",
                                    red, green, blue, led_index);

                           std::string result = "{\"success\":true"
                                                ",\"red\":" +
                                                std::to_string(red) +
                                                ",\"green\":" + std::to_string(green) +
                                                ",\"blue\":" + std::to_string(blue) +
                                                ",\"led_index\":" + std::to_string(led_index) + "}";
                           return result;
                       });

    // 添加RGB LED呼吸效果的工具函数
    mcp_server.AddTool("self.light.set_breathe_effect",
                       "设置RGB LED灯带的呼吸效果，可以根据用户情绪设置不同的颜色组合和效果，大模型应该分析用户的情绪状态和场景描述，然后根据情绪设置合适的呼吸效果。",
                       PropertyList({Property("start_red", kPropertyTypeInteger, 0, 255),
                                     Property("start_green", kPropertyTypeInteger, 0, 255),
                                     Property("start_blue", kPropertyTypeInteger, 0, 255),
                                     Property("end_red", kPropertyTypeInteger, 0, 255),
                                     Property("end_green", kPropertyTypeInteger, 0, 255),
                                     Property("end_blue", kPropertyTypeInteger, 0, 255),
                                     Property("interval_ms", kPropertyTypeInteger, 50, 2000)}),
                       [rgb_strip](const PropertyList &properties) -> ReturnValue
                       {
                           int start_red = properties["start_red"].value<int>();
                           int start_green = properties["start_green"].value<int>();
                           int start_blue = properties["start_blue"].value<int>();
                           int end_red = properties["end_red"].value<int>();
                           int end_green = properties["end_green"].value<int>();
                           int end_blue = properties["end_blue"].value<int>();
                           int interval_ms = properties["interval_ms"].value<int>();

                           StripColor low{static_cast<uint8_t>(start_red),
                                          static_cast<uint8_t>(start_green),
                                          static_cast<uint8_t>(start_blue)};
                           StripColor high{static_cast<uint8_t>(end_red),
                                           static_cast<uint8_t>(end_green),
                                           static_cast<uint8_t>(end_blue)};

                           rgb_strip->Breathe(low, high, interval_ms);

                           ESP_LOGI(TAG, "RGB LED breathe effect set - Start(R:%d,G:%d,B:%d), End(R:%d,G:%d,B:%d), Interval:%dms",
                                    start_red, start_green, start_blue, end_red, end_green, end_blue, interval_ms);

                           std::string result = "{\"success\":true"
                                                ",\"start_red\":" +
                                                std::to_string(start_red) +
                                                ",\"start_green\":" + std::to_string(start_green) +
                                                ",\"start_blue\":" + std::to_string(start_blue) +
                                                ",\"end_red\":" + std::to_string(end_red) +
                                                ",\"end_green\":" + std::to_string(end_green) +
                                                ",\"end_blue\":" + std::to_string(end_blue) +
                                                ",\"interval_ms\":" + std::to_string(interval_ms) + "}";
                           return result;
                       });

    // 添加RGB LED闪烁效果的工具函数
    mcp_server.AddTool("self.light.set_blink_effect",
                       "设置RGB LED灯带的闪烁效果，可以根据用户情绪设置不同的颜色组合和闪烁频率，大模型应该分析用户的情绪状态和场景描述，然后根据情绪设置合适的闪烁效果。",
                       PropertyList({Property("red", kPropertyTypeInteger, 0, 255),
                                     Property("green", kPropertyTypeInteger, 0, 255),
                                     Property("blue", kPropertyTypeInteger, 0, 255),
                                     Property("interval_ms", kPropertyTypeInteger, 50, 2000)}),
                       [rgb_strip](const PropertyList &properties) -> ReturnValue
                       {
                           int red = properties["red"].value<int>();
                           int green = properties["green"].value<int>();
                           int blue = properties["blue"].value<int>();
                           int interval_ms = properties["interval_ms"].value<int>();

                           StripColor color{static_cast<uint8_t>(red),
                                            static_cast<uint8_t>(green),
                                            static_cast<uint8_t>(blue)};

                           rgb_strip->Blink(color, interval_ms);

                           ESP_LOGI(TAG, "RGB LED blink effect set - R: %d, G: %d, B: %d, Interval: %dms",
                                    red, green, blue, interval_ms);

                           std::string result = "{\"success\":true"
                                                ",\"red\":" +
                                                std::to_string(red) +
                                                ",\"green\":" + std::to_string(green) +
                                                ",\"blue\":" + std::to_string(blue) +
                                                ",\"interval_ms\":" + std::to_string(interval_ms) + "}";
                           return result;
                       });

    // 添加设置RGB LED亮度的工具函数
    mcp_server.AddTool("self.light.set_brightness",
                       "设置RGB LED灯带的亮度，亮度范围为0-255，大模型应该分析用户的情绪状态和场景描述，然后根据情绪设置合适的灯光亮度。",
                       PropertyList({Property("brightness", kPropertyTypeInteger, 0, 255)}),
                       [rgb_strip](const PropertyList &properties) -> ReturnValue
                       {
                           int brightness = properties["brightness"].value<int>();
                           int low_brightness = brightness / 8; // 低亮度是默认亮度的1/8

                           rgb_strip->SetBrightness(static_cast<uint8_t>(brightness),
                                                    static_cast<uint8_t>(low_brightness));

                           ESP_LOGI(TAG, "RGB LED brightness set - Brightness: %d", brightness);

                           std::string result = "{\"success\":true"
                                                ",\"brightness\":" +
                                                std::to_string(brightness) + "}";
                           return result;
                       });
}