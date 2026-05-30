#ifndef APP_MCP_TOOL_H_
#define APP_MCP_TOOL_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化并注册Doly机器人的MCP工具
 * 
 * 注册两个核心工具：
 * 1. doly.action.preset - 预定义情景动作
 * 2. doly.action.custom - 自定义动作序列
 */
void app_mcp_tool_register(void);

#ifdef __cplusplus
}
#endif

#endif // APP_MCP_TOOL_H_
