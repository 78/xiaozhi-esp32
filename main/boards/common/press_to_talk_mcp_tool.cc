#include "press_to_talk_mcp_tool.h"
#include <esp_log.h>

static const char* TAG = "PressToTalkMcpTool";

PressToTalkMcpTool::PressToTalkMcpTool()
    : press_to_talk_enabled_(false) {
}

void PressToTalkMcpTool::Initialize() {
    // 从设置中读取当前状态
    Settings settings("vendor");
    press_to_talk_enabled_ = settings.GetInt("press_to_talk", 0) != 0;

    // 注册MCP工具
    auto& mcp_server = McpServer::GetInstance();
    mcp_server.AddTool("self.set_press_to_talk",
        "Switch between press to talk mode (长按说话) and click to talk mode (单击说话).\n"
        "The mode can be `press_to_talk` or `click_to_talk`.",
        PropertyList({
            Property("mode", kPropertyTypeString)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            return HandleSetPressToTalk(properties);
        });

    ESP_LOGI(TAG, "PressToTalkMcpTool initialized, current mode: %s", 
        press_to_talk_enabled_ ? "press_to_talk" : "click_to_talk");
}

bool PressToTalkMcpTool::IsPressToTalkEnabled() const {
    return press_to_talk_enabled_;
}

ReturnValue PressToTalkMcpTool::HandleSetPressToTalk(const PropertyList& properties) {
    auto mode = properties["mode"].value<std::string>();
    
    if (mode == "press_to_talk") {
        SetPressToTalkEnabled(true);
        ESP_LOGI(TAG, "Switched to press to talk mode");
        return true;
    } else if (mode == "click_to_talk") {
        SetPressToTalkEnabled(false);
        ESP_LOGI(TAG, "Switched to click to talk mode");
        return true;
    }
    
    throw std::runtime_error("Invalid mode: " + mode);
}

void PressToTalkMcpTool::SetPressToTalkEnabled(bool enabled) {
    press_to_talk_enabled_ = enabled;
    
    Settings settings("vendor", true);
    settings.SetInt("press_to_talk", enabled ? 1 : 0);
    ESP_LOGI(TAG, "Press to talk enabled: %d", enabled);
} 