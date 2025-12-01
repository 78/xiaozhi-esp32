#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include "mcp_server.h"
#include "schedule_reminder.h"

class ScheduleManager {
public:
    static void RegisterMcpTools();
    
private:
    // MCP 工具实现
    static bool AddScheduleTool(const PropertyList& properties);
    static bool ListSchedulesTool(const PropertyList& properties); 
    static bool RemoveScheduleTool(const PropertyList& properties);
    static bool UpdateScheduleTool(const PropertyList& properties);
    
    // 工具属性定义
    static PropertyList GetAddScheduleProperties();
    static PropertyList GetRemoveScheduleProperties();
    static PropertyList GetUpdateScheduleProperties();
};

#endif // SCHEDULE_MANAGER_H
