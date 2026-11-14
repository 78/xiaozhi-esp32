#ifndef TOUCH_MCP_TOOL_H
#define TOUCH_MCP_TOOL_H

#include <esp_lcd_touch.h>

#include "mcp_server.h"

// 可复用的触摸屏状态MCP工具类
//
// 41 块板子初始化了触摸控制器,其中 31 块持有 esp_lcd_touch_handle_t,但没有任何一块把触摸
// 暴露给模型 —— 句柄通常是个局部变量,交给 lvgl_port_add_touch() 之后就丢掉了。
// 想用这个工具的板子把句柄留下来即可,用法和 PressToTalkMcpTool 一样。
class TouchMcpTool {
public:
    explicit TouchMcpTool(esp_lcd_touch_handle_t handle);

    // 注册到MCP服务器。句柄为空时不注册:一块没找到触摸芯片的板子不应该报告一个假的触摸面。
    void Initialize();

private:
    ToolResult HandleGetState();

    esp_lcd_touch_handle_t handle_;
};

#endif  // TOUCH_MCP_TOOL_H
