#ifndef SCHEDULE_MANAGER_SIMPLE_H
#define SCHEDULE_MANAGER_SIMPLE_H

#include <string>
#include <ctime>
#include <esp_log.h>

// 简化的日程事件结构
struct SimpleScheduleEvent {
    std::string id;
    std::string title;
    std::string description;
    std::string category;
    time_t start_time;
    time_t end_time;
    bool is_all_day;
    int reminder_minutes;
    bool is_completed;
    time_t created_time;
    
    SimpleScheduleEvent() : start_time(0), end_time(0), is_all_day(false), 
                           reminder_minutes(15), is_completed(false), created_time(0) {}
};

// 简化的日程管理器
class SimpleScheduleManager {
public:
    static SimpleScheduleManager& GetInstance() {
        static SimpleScheduleManager instance;
        return instance;
    }

    // 基本功能
    std::string CreateEvent(const std::string& title, 
                           const std::string& description,
                           time_t start_time, 
                           time_t end_time = 0,
                           const std::string& category = "",
                           bool is_all_day = false,
                           int reminder_minutes = 15);
    
    bool UpdateEvent(const std::string& event_id, 
                    const std::string& title = "",
                    const std::string& description = "",
                    time_t start_time = 0,
                    time_t end_time = 0,
                    const std::string& category = "",
                    bool is_all_day = false,
                    int reminder_minutes = -1);
    
    bool DeleteEvent(const std::string& event_id);
    
    SimpleScheduleEvent* GetEvent(const std::string& event_id);
    
    // 查询功能
    int GetEventCount();
    std::string ExportToJson();

private:
    SimpleScheduleManager();
    ~SimpleScheduleManager();
    
    std::string GenerateEventId();
    
    SimpleScheduleEvent events_[100];  // 固定大小数组
    int event_count_;
    
    static const char* TAG;
};

#endif // SCHEDULE_MANAGER_SIMPLE_H
