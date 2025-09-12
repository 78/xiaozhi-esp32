#ifndef TOOLS_MANAGER_H
#define TOOLS_MANAGER_H

#include <string>
#include "mcp_server.h"

class ToolsManager {
public:
    static ToolsManager& GetInstance();
    
    // 初始化工具系统
    bool Initialize();
    
    // 工具注册方法
    void RegisterMcpTools();
    void RegisterSystemTools();
    void RegisterAudioTools();
    void RegisterSensorTools();
    
    // 检查是否已初始化
    bool IsInitialized() const { return initialized_; }

private:
    ToolsManager() = default;
    ~ToolsManager() = default;
    ToolsManager(const ToolsManager&) = delete;
    ToolsManager& operator=(const ToolsManager&) = delete;
    
    bool initialized_ = false;
};

#endif // TOOLS_MANAGER_H
