#include <cJSON.h>
#include <esp_log.h>

#include <cstring>

#include "application.h"
#include "board.h"
#include "config.h"
#include "mcp_server.h"
#include "sdkconfig.h"
#include "settings.h"
#include "display.h"

#define TAG "MCPController"

class MCPController {
public:
    MCPController() {
        RegisterMcpTools();
        ESP_LOGI(TAG, "注册MCP工具");
    }

	void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();
		ESP_LOGI(TAG, "开始注册MCP工具...");

	mcp_server.AddTool(
        "self.AEC.set_mode", 
        "设置AEC对话打断模式。当用户意图切换对话打断模式时或者用户觉得ai对话容易被打断时或者用户觉得无法实现对话打断时都使用此工具。\n"
        "参数：\n"
        "   `mode`: 对话打断模式，可选值只有`kAecOff`(关闭）和`kAecOnDeviceSide`（开启）\n"
        "返回值：\n"
        "   反馈状态信息，不需要确认，立即播报相关数据\n",
        PropertyList({
            Property("mode", kPropertyTypeString)
        }), 
        [](const PropertyList& properties) -> ReturnValue {
            auto mode = properties["mode"].value<std::string>();
            auto& app = Application::GetInstance();
            vTaskDelay(pdMS_TO_TICKS(2000));
            if (mode == "kAecOff") {
                app.SetAecMode(kAecOff);
                return "{\"success\": true, \"message\": \"AEC对话打断模式已关闭\"}";
            }else {
                auto& board = Board::GetInstance();
                app.SetAecMode(kAecOnDeviceSide);
                
                return "{\"success\": true, \"message\": \"AEC对话打断模式已开启\"}";
            }
        }
    );

    mcp_server.AddTool(
        "self.AEC.get_mode",
        "获取AEC对话打断模式状态。当用户意图获取对话打断模式状态时使用此工具。\n"
        "返回值：\n"
        "   反馈状态信息，不需要确认，立即播报相关数据\n",
        PropertyList(),  
        [](const PropertyList&) -> ReturnValue {
            auto& app = Application::GetInstance();
            const bool is_currently_off = (app.GetAecMode() == kAecOff);
           if (is_currently_off) {
                return "{\"success\": true, \"message\": \"AEC对话打断模式处于关闭状态\"}";
            }else {
                return "{\"success\": true, \"message\": \"AEC对话打断模式处于开启状态\"}";
            }
        }
    );
	
    mcp_server.AddTool(
        "self.res.esp_restart",
        "重启设备。当用户意图重启设备时使用此工具。\n",
        PropertyList(),  
        [](const PropertyList&) -> ReturnValue {
            vTaskDelay(pdMS_TO_TICKS(1000));
            // Reboot the device
            esp_restart();
            return true;
        }
    );

        ESP_LOGI(TAG, "MCP工具注册完成");
    }

};

static MCPController* g_mcp_controller = nullptr;

void InitializeMCPController() {
    if (g_mcp_controller == nullptr) {
        g_mcp_controller = new MCPController();
        ESP_LOGI(TAG, "注册MCP工具");
    }
}