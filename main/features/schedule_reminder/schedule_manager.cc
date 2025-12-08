#include "schedule_manager.h"
#include <esp_log.h>
#include <ctime>

#define TAG "ScheduleManager"

void ScheduleManager::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();
    
    // 添加日程工具
    mcp_server.AddTool("schedule.add",
        "Add a new schedule reminder",
        GetAddScheduleProperties(),
        AddScheduleTool);
    
    // 列出日程工具
    mcp_server.AddTool("schedule.list",
        "List all schedule reminders",
        PropertyList(),
        ListSchedulesTool);
    
    // 删除日程工具
    mcp_server.AddTool("schedule.remove",
        "Remove a schedule reminder",
        GetRemoveScheduleProperties(),
        RemoveScheduleTool);
    
    // 更新日程工具
    mcp_server.AddTool("schedule.update",
        "Update an existing schedule reminder",
        GetUpdateScheduleProperties(),
        UpdateScheduleTool);
    
    ESP_LOGI(TAG, "Schedule MCP tools registered");
}

PropertyList ScheduleManager::GetAddScheduleProperties() {
    PropertyList props;
    props.AddProperty("title", "Schedule title", PropertyType::kString, true);
    props.AddProperty("description", "Schedule description", PropertyType::kString, false);
    props.AddProperty("trigger_time", "Trigger time (Unix timestamp)", PropertyType::kNumber, true);
    props.AddProperty("recurring", "Whether this is a recurring schedule", PropertyType::kBoolean, false);
    props.AddProperty("repeat_interval", "Repeat interval in seconds", PropertyType::kNumber, false);
    return props;
}

PropertyList ScheduleManager::GetRemoveScheduleProperties() {
    PropertyList props;
    props.AddProperty("id", "Schedule ID to remove", PropertyType::kString, true);
    return props;
}

PropertyList ScheduleManager::GetUpdateScheduleProperties() {
    PropertyList props;
    props.AddProperty("id", "Schedule ID to update", PropertyType::kString, true);
    props.AddProperty("title", "New schedule title", PropertyType::kString, false);
    props.AddProperty("description", "New schedule description", PropertyType::kString, false);
    props.AddProperty("trigger_time", "New trigger time (Unix timestamp)", PropertyType::kNumber, false);
    props.AddProperty("enabled", "Whether the schedule is enabled", PropertyType::kBoolean, false);
    props.AddProperty("recurring", "Whether this is a recurring schedule", PropertyType::kBoolean, false);
    props.AddProperty("repeat_interval", "Repeat interval in seconds", PropertyType::kNumber, false);
    return props;
}

bool ScheduleManager::AddScheduleTool(const PropertyList& properties) {
    ScheduleItem item;
    item.id = std::to_string(time(nullptr));  // Use timestamp as ID
    item.title = properties.GetValue<std::string>("title");
    item.description = properties.GetValue<std::string>("description", "");
    item.trigger_time = properties.GetValue<int>("trigger_time");
    item.recurring = properties.GetValue<bool>("recurring", false);
    item.repeat_interval = properties.GetValue<int>("repeat_interval", 0);
    item.created_at = std::to_string(time(nullptr));
    
    ScheduleError result = ScheduleReminder::GetInstance().AddSchedule(item);
    
    switch (result) {
        case ScheduleError::kSuccess:
            ESP_LOGI(TAG, "Schedule added via MCP: %s", item.title.c_str());
            return true;
        case ScheduleError::kMaxItemsReached:
            ESP_LOGE(TAG, "Failed to add schedule via MCP: maximum items reached");
            return false;
        case ScheduleError::kDuplicateId:
            ESP_LOGE(TAG, "Failed to add schedule via MCP: duplicate ID");
            return false;
        case ScheduleError::kInvalidTime:
            ESP_LOGE(TAG, "Failed to add schedule via MCP: invalid trigger time");
            return false;
        case ScheduleError::kStorageError:
            ESP_LOGE(TAG, "Failed to add schedule via MCP: storage error");
            return false;
        case ScheduleError::kNotInitialized:
            ESP_LOGE(TAG, "Failed to add schedule via MCP: not initialized");
            return false;
        default:
            ESP_LOGE(TAG, "Failed to add schedule via MCP: unknown error");
            return false;
    }
}

bool ScheduleManager::ListSchedulesTool(const PropertyList& properties) {
    auto schedules = ScheduleReminder::GetInstance().GetSchedules();
    
    // 这里可以返回日程列表给 MCP 客户端
    // 实际实现中可能需要格式化输出
    
    ESP_LOGI(TAG, "Listed %d schedules via MCP", schedules.size());
    
    // 简单记录到日志
    for (const auto& schedule : schedules) {
        ESP_LOGI(TAG, "Schedule: %s (ID: %s, Time: %ld)", 
                 schedule.title.c_str(), schedule.id.c_str(), schedule.trigger_time);
    }
    
    return true;
}

bool ScheduleManager::RemoveScheduleTool(const PropertyList& properties) {
    std::string id = properties.GetValue<std::string>("id");
    ScheduleError result = ScheduleReminder::GetInstance().RemoveSchedule(id);
    
    switch (result) {
        case ScheduleError::kSuccess:
            ESP_LOGI(TAG, "Schedule removed via MCP: %s", id.c_str());
            return true;
        case ScheduleError::kNotFound:
            ESP_LOGE(TAG, "Failed to remove schedule via MCP: schedule not found - %s", id.c_str());
            return false;
        case ScheduleError::kStorageError:
            ESP_LOGE(TAG, "Failed to remove schedule via MCP: storage error - %s", id.c_str());
            return false;
        case ScheduleError::kNotInitialized:
            ESP_LOGE(TAG, "Failed to remove schedule via MCP: not initialized - %s", id.c_str());
            return false;
        default:
            ESP_LOGE(TAG, "Failed to remove schedule via MCP: unknown error - %s", id.c_str());
            return false;
    }
}

bool ScheduleManager::UpdateScheduleTool(const PropertyList& properties) {
    std::string id = properties.GetValue<std::string>("id");
    
    // Get existing schedule
    ScheduleItem* existing_item = ScheduleReminder::GetInstance().GetSchedule(id);
    if (!existing_item) {
        ESP_LOGE(TAG, "Schedule not found for update: %s", id.c_str());
        return false;
    }
    
    // Create updated schedule item
    ScheduleItem updated_item = *existing_item;
    
    // Update provided fields
    if (properties.HasValue("title")) {
        updated_item.title = properties.GetValue<std::string>("title");
    }
    if (properties.HasValue("description")) {
        updated_item.description = properties.GetValue<std::string>("description");
    }
    if (properties.HasValue("trigger_time")) {
        updated_item.trigger_time = properties.GetValue<int>("trigger_time");
    }
    if (properties.HasValue("enabled")) {
        updated_item.enabled = properties.GetValue<bool>("enabled");
    }
    if (properties.HasValue("recurring")) {
        updated_item.recurring = properties.GetValue<bool>("recurring");
    }
    if (properties.HasValue("repeat_interval")) {
        updated_item.repeat_interval = properties.GetValue<int>("repeat_interval");
    }
    
    ScheduleError result = ScheduleReminder::GetInstance().UpdateSchedule(id, updated_item);
    
    switch (result) {
        case ScheduleError::kSuccess:
            ESP_LOGI(TAG, "Schedule updated via MCP: %s", id.c_str());
            return true;
        case ScheduleError::kNotFound:
            ESP_LOGE(TAG, "Failed to update schedule via MCP: schedule not found - %s", id.c_str());
            return false;
        case ScheduleError::kStorageError:
            ESP_LOGE(TAG, "Failed to update schedule via MCP: storage error - %s", id.c_str());
            return false;
        case ScheduleError::kNotInitialized:
            ESP_LOGE(TAG, "Failed to update schedule via MCP: not initialized - %s", id.c_str());
            return false;
        default:
            ESP_LOGE(TAG, "Failed to update schedule via MCP: unknown error - %s", id.c_str());
            return false;
    }
}
