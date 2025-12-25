#ifndef _FOGSEEK_MCP_TOOLS_H_
#define _FOGSEEK_MCP_TOOLS_H_

#include "mcp_server.h"
#include "led/gpio_led.h"

/**
 * @brief 初始化灯光控制 MCP 工具函数
 * @param mcp_server MCP 服务器实例
 * @param cold_light 冷色灯控制实例
 * @param warm_light 暖色灯控制实例
 * @param cold_light_state 冷色灯状态引用
 * @param warm_light_state 暖色灯状态引用
 */
void InitializeLightMCP(
    McpServer &mcp_server,
    GpioLed *cold_light,
    GpioLed *warm_light,
    bool cold_light_state,
    bool warm_light_state);

#endif // _FOGSEEK_MCP_TOOLS_H_