#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <ctime>
#include <mutex>
#include <esp_log.h>

// 日程事件结构
struct ScheduleEvent {
    std::string id;           // 唯一标识符
    std::string title;         // 事件标题
    std::string description;   // 事件描述
    std::string category;      // 事件分类（工作、生活、学习等）
    time_t start_time;         // 开始时间
    time_t end_time;           // 结束时间
    bool is_all_day;          // 是否全天事件
    bool is_recurring;         // 是否重复事件
    std::string recurrence;    // 重复规则（daily, weekly, monthly）
    int reminder_minutes;      // 提醒时间（分钟）
    bool is_completed;         // 是否已完成
    time_t created_time;       // 创建时间
    time_t updated_time;       // 更新时间
    
    ScheduleEvent() : start_time(0), end_time(0), is_all_day(false), 
                     is_recurring(false), reminder_minutes(0), 
                     is_completed(false), created_time(0), updated_time(0) {}
};

// 智能分类枚举
enum class EventCategory {
    WORK,       // 工作
    LIFE,       // 生活
    STUDY,      // 学习
    HEALTH,     // 健康
    ENTERTAINMENT, // 娱乐
    TRAVEL,     // 旅行
    FAMILY,     // 家庭
    OTHER       // 其他
};

// 提醒回调函数类型
using ReminderCallback = std::function<void(const ScheduleEvent&)>;

class ScheduleManager {
public:
    static ScheduleManager& GetInstance() {
        static ScheduleManager instance;
        return instance;
    }

    // 事件管理
    std::string CreateEvent(const std::string& title, 
                           const std::string& description,
                           time_t start_time, 
                           time_t end_time,
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
    
    ScheduleEvent* GetEvent(const std::string& event_id);
    
    // 查询功能
    std::vector<ScheduleEvent> GetEventsByDate(time_t date);
    std::vector<ScheduleEvent> GetEventsByCategory(const std::string& category);
    std::vector<ScheduleEvent> GetUpcomingEvents(int days = 7);
    std::vector<ScheduleEvent> GetEventsByKeyword(const std::string& keyword);
    
    // 智能分类
    std::string CategorizeEvent(const std::string& title, const std::string& description);
    
    // 提醒功能
    void SetReminderCallback(ReminderCallback callback);
    void CheckReminders();
    
    // 统计功能
    int GetEventCount();
    int GetEventCountByCategory(const std::string& category);
    std::map<std::string, int> GetCategoryStatistics();
    
    // 数据持久化
    bool SaveToStorage();
    bool LoadFromStorage();
    
    // 导出功能
    std::string ExportToJson();
    bool ImportFromJson(const std::string& json_data);

private:
    ScheduleManager();
    ~ScheduleManager();
    
    std::string GenerateEventId();
    std::string CategoryToString(EventCategory category);
    EventCategory StringToCategory(const std::string& category_str);
    bool IsEventTimeValid(time_t start_time, time_t end_time);
    void UpdateEventTimestamp(ScheduleEvent& event);
    
    std::map<std::string, ScheduleEvent> events_;
    std::mutex events_mutex_;
    ReminderCallback reminder_callback_;
    
    static const char* TAG;
};

#endif // SCHEDULE_MANAGER_H
