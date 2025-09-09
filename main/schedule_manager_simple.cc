#include "schedule_manager_simple.h"
#include <esp_log.h>
#include <cstring>
#include <ctime>

const char* SimpleScheduleManager::TAG = "SimpleScheduleManager";

SimpleScheduleManager::SimpleScheduleManager() : event_count_(0) {
    ESP_LOGI(TAG, "SimpleScheduleManager initialized");
}

SimpleScheduleManager::~SimpleScheduleManager() {
    ESP_LOGI(TAG, "SimpleScheduleManager destroyed");
}

std::string SimpleScheduleManager::CreateEvent(const std::string& title, 
                                             const std::string& description,
                                             time_t start_time, 
                                             time_t end_time,
                                             const std::string& category,
                                             bool is_all_day,
                                             int reminder_minutes) {
    if (event_count_ >= 100) {
        ESP_LOGE(TAG, "Maximum event count reached");
        return "";
    }
    
    if (title.empty()) {
        ESP_LOGE(TAG, "Event title cannot be empty");
        return "";
    }
    
    SimpleScheduleEvent& event = events_[event_count_];
    event.id = GenerateEventId();
    event.title = title;
    event.description = description;
    event.start_time = start_time;
    event.end_time = end_time;
    event.category = category.empty() ? "其他" : category;
    event.is_all_day = is_all_day;
    event.reminder_minutes = reminder_minutes;
    event.created_time = time(nullptr);
    
    event_count_++;
    
    ESP_LOGI(TAG, "Created event: %s (ID: %s)", title.c_str(), event.id.c_str());
    return event.id;
}

bool SimpleScheduleManager::UpdateEvent(const std::string& event_id, 
                                       const std::string& title,
                                       const std::string& description,
                                       time_t start_time,
                                       time_t end_time,
                                       const std::string& category,
                                       bool is_all_day,
                                       int reminder_minutes) {
    for (int i = 0; i < event_count_; i++) {
        if (events_[i].id == event_id) {
            SimpleScheduleEvent& event = events_[i];
            
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
            
            ESP_LOGI(TAG, "Updated event: %s", event_id.c_str());
            return true;
        }
    }
    
    ESP_LOGE(TAG, "Event not found: %s", event_id.c_str());
    return false;
}

bool SimpleScheduleManager::DeleteEvent(const std::string& event_id) {
    for (int i = 0; i < event_count_; i++) {
        if (events_[i].id == event_id) {
            // 移动后面的元素
            for (int j = i; j < event_count_ - 1; j++) {
                events_[j] = events_[j + 1];
            }
            event_count_--;
            
            ESP_LOGI(TAG, "Deleted event: %s", event_id.c_str());
            return true;
        }
    }
    
    ESP_LOGE(TAG, "Event not found: %s", event_id.c_str());
    return false;
}

SimpleScheduleEvent* SimpleScheduleManager::GetEvent(const std::string& event_id) {
    for (int i = 0; i < event_count_; i++) {
        if (events_[i].id == event_id) {
            return &events_[i];
        }
    }
    return nullptr;
}

int SimpleScheduleManager::GetEventCount() {
    return event_count_;
}

std::string SimpleScheduleManager::ExportToJson() {
    std::string json = "{\"events\":[";
    
    for (int i = 0; i < event_count_; i++) {
        if (i > 0) json += ",";
        
        const SimpleScheduleEvent& event = events_[i];
        json += "{";
        json += "\"id\":\"" + event.id + "\",";
        json += "\"title\":\"" + event.title + "\",";
        json += "\"description\":\"" + event.description + "\",";
        json += "\"category\":\"" + event.category + "\",";
        json += "\"start_time\":" + std::to_string(event.start_time) + ",";
        json += "\"end_time\":" + std::to_string(event.end_time) + ",";
        json += "\"is_all_day\":";
        json += (event.is_all_day ? "true" : "false");
        json += ",\"reminder_minutes\":";
        json += std::to_string(event.reminder_minutes);
        json += ",\"is_completed\":";
        json += (event.is_completed ? "true" : "false");
        json += ",";
        json += "\"created_time\":" + std::to_string(event.created_time);
        json += "}";
    }
    
    json += "]}";
    return json;
}

std::string SimpleScheduleManager::GenerateEventId() {
    static int counter = 0;
    return "event_" + std::to_string(++counter) + "_" + std::to_string(time(nullptr));
}
