#include "schedule_manager.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cstring>
#include <algorithm>
#include <sstream>

const char* ScheduleManager::TAG = "ScheduleManager";

ScheduleManager::ScheduleManager() {
    ESP_LOGI(TAG, "ScheduleManager initialized");
}

ScheduleManager::~ScheduleManager() {
    ESP_LOGI(TAG, "ScheduleManager destroyed");
}

std::string ScheduleManager::CreateEvent(const std::string& title, 
                                       const std::string& description,
                                       time_t start_time, 
                                       time_t end_time,
                                       const std::string& category,
                                       bool is_all_day,
                                       int reminder_minutes) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    if (title.empty()) {
        ESP_LOGE(TAG, "Event title cannot be empty");
        return "";
    }
    
    if (!IsEventTimeValid(start_time, end_time)) {
        ESP_LOGE(TAG, "Invalid event time");
        return "";
    }
    
    ScheduleEvent event;
    event.id = GenerateEventId();
    event.title = title;
    event.description = description;
    event.start_time = start_time;
    event.end_time = end_time;
    event.is_all_day = is_all_day;
    event.reminder_minutes = reminder_minutes;
    event.created_time = time(nullptr);
    event.updated_time = event.created_time;
    
    // 智能分类
    if (category.empty()) {
        event.category = CategorizeEvent(title, description);
    } else {
        event.category = category;
    }
    
    events_[event.id] = event;
    
    ESP_LOGI(TAG, "Created event: %s (ID: %s)", title.c_str(), event.id.c_str());
    return event.id;
}

bool ScheduleManager::UpdateEvent(const std::string& event_id, 
                                const std::string& title,
                                const std::string& description,
                                time_t start_time,
                                time_t end_time,
                                const std::string& category,
                                bool is_all_day,
                                int reminder_minutes) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    auto it = events_.find(event_id);
    if (it == events_.end()) {
        ESP_LOGE(TAG, "Event not found: %s", event_id.c_str());
        return false;
    }
    
    ScheduleEvent& event = it->second;
    
    if (!title.empty()) {
        event.title = title;
    }
    if (!description.empty()) {
        event.description = description;
    }
    if (start_time > 0) {
        event.start_time = start_time;
    }
    if (end_time > 0) {
        event.end_time = end_time;
    }
    if (!category.empty()) {
        event.category = category;
    }
    if (reminder_minutes >= 0) {
        event.reminder_minutes = reminder_minutes;
    }
    
    event.is_all_day = is_all_day;
    event.updated_time = time(nullptr);
    
    ESP_LOGI(TAG, "Updated event: %s", event_id.c_str());
    return true;
}

bool ScheduleManager::DeleteEvent(const std::string& event_id) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    auto it = events_.find(event_id);
    if (it == events_.end()) {
        ESP_LOGE(TAG, "Event not found: %s", event_id.c_str());
        return false;
    }
    
    events_.erase(it);
    ESP_LOGI(TAG, "Deleted event: %s", event_id.c_str());
    return true;
}

ScheduleEvent* ScheduleManager::GetEvent(const std::string& event_id) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    auto it = events_.find(event_id);
    if (it == events_.end()) {
        return nullptr;
    }
    
    return &it->second;
}

std::vector<ScheduleEvent> ScheduleManager::GetEventsByDate(time_t date) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    std::vector<ScheduleEvent> result;
    
    struct tm date_tm = *localtime(&date);
    
    for (const auto& pair : events_) {
        const ScheduleEvent& event = pair.second;
        struct tm event_tm = *localtime(&event.start_time);
        
        if (event_tm.tm_year == date_tm.tm_year &&
            event_tm.tm_mon == date_tm.tm_mon &&
            event_tm.tm_mday == date_tm.tm_mday) {
            result.push_back(event);
        }
    }
    
    // 按开始时间排序
    std::sort(result.begin(), result.end(), 
              [](const ScheduleEvent& a, const ScheduleEvent& b) {
                  return a.start_time < b.start_time;
              });
    
    return result;
}

std::vector<ScheduleEvent> ScheduleManager::GetEventsByCategory(const std::string& category) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    std::vector<ScheduleEvent> result;
    
    for (const auto& pair : events_) {
        const ScheduleEvent& event = pair.second;
        if (event.category == category) {
            result.push_back(event);
        }
    }
    
    return result;
}

std::vector<ScheduleEvent> ScheduleManager::GetUpcomingEvents(int days) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    std::vector<ScheduleEvent> result;
    
    time_t now = time(nullptr);
    time_t future_time = now + (days * 24 * 60 * 60);
    
    for (const auto& pair : events_) {
        const ScheduleEvent& event = pair.second;
        if (event.start_time >= now && event.start_time <= future_time && !event.is_completed) {
            result.push_back(event);
        }
    }
    
    // 按开始时间排序
    std::sort(result.begin(), result.end(), 
              [](const ScheduleEvent& a, const ScheduleEvent& b) {
                  return a.start_time < b.start_time;
              });
    
    return result;
}

std::vector<ScheduleEvent> ScheduleManager::GetEventsByKeyword(const std::string& keyword) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    std::vector<ScheduleEvent> result;
    
    std::string lower_keyword = keyword;
    std::transform(lower_keyword.begin(), lower_keyword.end(), lower_keyword.begin(), ::tolower);
    
    for (const auto& pair : events_) {
        const ScheduleEvent& event = pair.second;
        
        std::string lower_title = event.title;
        std::string lower_description = event.description;
        std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
        std::transform(lower_description.begin(), lower_description.end(), lower_description.begin(), ::tolower);
        
        if (lower_title.find(lower_keyword) != std::string::npos ||
            lower_description.find(lower_keyword) != std::string::npos) {
            result.push_back(event);
        }
    }
    
    return result;
}

std::string ScheduleManager::CategorizeEvent(const std::string& title, const std::string& description) {
    std::string text = title + " " + description;
    std::transform(text.begin(), text.end(), text.begin(), ::tolower);
    
    // 工作相关关键词
    if (text.find("会议") != std::string::npos || text.find("工作") != std::string::npos ||
        text.find("项目") != std::string::npos || text.find("报告") != std::string::npos ||
        text.find("deadline") != std::string::npos || text.find("meeting") != std::string::npos) {
        return CategoryToString(EventCategory::WORK);
    }
    
    // 学习相关关键词
    if (text.find("学习") != std::string::npos || text.find("课程") != std::string::npos ||
        text.find("考试") != std::string::npos || text.find("作业") != std::string::npos ||
        text.find("study") != std::string::npos || text.find("exam") != std::string::npos) {
        return CategoryToString(EventCategory::STUDY);
    }
    
    // 健康相关关键词
    if (text.find("运动") != std::string::npos || text.find("健身") != std::string::npos ||
        text.find("医院") != std::string::npos || text.find("体检") != std::string::npos ||
        text.find("exercise") != std::string::npos || text.find("doctor") != std::string::npos) {
        return CategoryToString(EventCategory::HEALTH);
    }
    
    // 娱乐相关关键词
    if (text.find("电影") != std::string::npos || text.find("游戏") != std::string::npos ||
        text.find("聚会") != std::string::npos || text.find("娱乐") != std::string::npos ||
        text.find("movie") != std::string::npos || text.find("party") != std::string::npos) {
        return CategoryToString(EventCategory::ENTERTAINMENT);
    }
    
    // 旅行相关关键词
    if (text.find("旅行") != std::string::npos || text.find("旅游") != std::string::npos ||
        text.find("出差") != std::string::npos || text.find("travel") != std::string::npos ||
        text.find("trip") != std::string::npos) {
        return CategoryToString(EventCategory::TRAVEL);
    }
    
    // 家庭相关关键词
    if (text.find("家庭") != std::string::npos || text.find("家人") != std::string::npos ||
        text.find("孩子") != std::string::npos || text.find("family") != std::string::npos ||
        text.find("child") != std::string::npos) {
        return CategoryToString(EventCategory::FAMILY);
    }
    
    return CategoryToString(EventCategory::OTHER);
}

void ScheduleManager::SetReminderCallback(ReminderCallback callback) {
    reminder_callback_ = callback;
}

void ScheduleManager::CheckReminders() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    time_t now = time(nullptr);
    
    for (const auto& pair : events_) {
        const ScheduleEvent& event = pair.second;
        
        if (event.is_completed || event.reminder_minutes <= 0) {
            continue;
        }
        
        time_t reminder_time = event.start_time - (event.reminder_minutes * 60);
        
        if (now >= reminder_time && now < event.start_time) {
            ESP_LOGI(TAG, "Reminder triggered for event: %s", event.title.c_str());
            if (reminder_callback_) {
                reminder_callback_(event);
            }
        }
    }
}

int ScheduleManager::GetEventCount() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    return events_.size();
}

int ScheduleManager::GetEventCountByCategory(const std::string& category) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    int count = 0;
    
    for (const auto& pair : events_) {
        if (pair.second.category == category) {
            count++;
        }
    }
    
    return count;
}

std::map<std::string, int> ScheduleManager::GetCategoryStatistics() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    std::map<std::string, int> stats;
    
    for (const auto& pair : events_) {
        stats[pair.second.category]++;
    }
    
    return stats;
}

bool ScheduleManager::SaveToStorage() {
    // TODO: 实现数据持久化到NVS或SPIFFS
    ESP_LOGW(TAG, "SaveToStorage not implemented yet");
    return true;
}

bool ScheduleManager::LoadFromStorage() {
    // TODO: 实现从NVS或SPIFFS加载数据
    ESP_LOGW(TAG, "LoadFromStorage not implemented yet");
    return true;
}

std::string ScheduleManager::ExportToJson() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    std::stringstream json;
    json << "{\"events\":[";
    
    bool first = true;
    for (const auto& pair : events_) {
        if (!first) json << ",";
        first = false;
        
        const ScheduleEvent& event = pair.second;
        json << "{"
             << "\"id\":\"" << event.id << "\","
             << "\"title\":\"" << event.title << "\","
             << "\"description\":\"" << event.description << "\","
             << "\"category\":\"" << event.category << "\","
             << "\"start_time\":" << event.start_time << ","
             << "\"end_time\":" << event.end_time << ","
             << "\"is_all_day\":" << (event.is_all_day ? "true" : "false") << ","
             << "\"reminder_minutes\":" << event.reminder_minutes << ","
             << "\"is_completed\":" << (event.is_completed ? "true" : "false") << ","
             << "\"created_time\":" << event.created_time << ","
             << "\"updated_time\":" << event.updated_time
             << "}";
    }
    
    json << "]}";
    return json.str();
}

bool ScheduleManager::ImportFromJson(const std::string& json_data) {
    // TODO: 实现JSON导入功能
    ESP_LOGW(TAG, "ImportFromJson not implemented yet");
    return false;
}

std::string ScheduleManager::GenerateEventId() {
    static int counter = 0;
    return "event_" + std::to_string(++counter) + "_" + std::to_string(time(nullptr));
}

std::string ScheduleManager::CategoryToString(EventCategory category) {
    switch (category) {
        case EventCategory::WORK: return "工作";
        case EventCategory::LIFE: return "生活";
        case EventCategory::STUDY: return "学习";
        case EventCategory::HEALTH: return "健康";
        case EventCategory::ENTERTAINMENT: return "娱乐";
        case EventCategory::TRAVEL: return "旅行";
        case EventCategory::FAMILY: return "家庭";
        case EventCategory::OTHER: return "其他";
        default: return "其他";
    }
}

EventCategory ScheduleManager::StringToCategory(const std::string& category_str) {
    if (category_str == "工作") return EventCategory::WORK;
    if (category_str == "生活") return EventCategory::LIFE;
    if (category_str == "学习") return EventCategory::STUDY;
    if (category_str == "健康") return EventCategory::HEALTH;
    if (category_str == "娱乐") return EventCategory::ENTERTAINMENT;
    if (category_str == "旅行") return EventCategory::TRAVEL;
    if (category_str == "家庭") return EventCategory::FAMILY;
    return EventCategory::OTHER;
}

bool ScheduleManager::IsEventTimeValid(time_t start_time, time_t end_time) {
    if (start_time <= 0) return false;
    if (end_time > 0 && end_time <= start_time) return false;
    return true;
}

void ScheduleManager::UpdateEventTimestamp(ScheduleEvent& event) {
    event.updated_time = time(nullptr);
}
