#ifndef PRESS_TO_TALK_MCP_TOOL_H
#define PRESS_TO_TALK_MCP_TOOL_H

#include "mcp_server.h"
#include "settings.h"

// 可复用的按键说话模式MCP工具类
class PressToTalkMcpTool {
private:
    bool press_to_talk_enabled_;

public:
    PressToTalkMcpTool();
    
    // 初始化工具，注册到MCP服务器
    void Initialize();
    
    // 获取当前按键说话模式状态
    bool IsPressToTalkEnabled() const;

private:
    // MCP工具的回调函数
    ReturnValue HandleSetPressToTalk(const PropertyList& properties);
    
    // 内部方法：设置press to talk状态并保存到设置
    void SetPressToTalkEnabled(bool enabled);
};

#endif // PRESS_TO_TALK_MCP_TOOL_H 