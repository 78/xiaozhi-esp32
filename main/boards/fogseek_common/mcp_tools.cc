#include "mcp_tools.h"
#include <esp_log.h>

static const char *TAG = "FogSeekMCPTools";

void InitializeLightMCP(
    McpServer &mcp_server,
    GpioLed *cold_light,
    GpioLed *warm_light,
    bool &cold_light_state,
    bool &warm_light_state)
{
    // 添加获取当前灯状态的工具函数
    mcp_server.AddTool("self.light.get_status", 
                       "获取当前灯的状态", 
                       PropertyList(),
                       [&cold_light_state, &warm_light_state](const PropertyList &properties) -> ReturnValue
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
                           if (cold_brightness > 0) {
                               cold_light->TurnOn();
                           } else {
                               cold_light->TurnOff();
                           }
                           
                           if (warm_brightness > 0) {
                               warm_light->TurnOn();
                           } else {
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