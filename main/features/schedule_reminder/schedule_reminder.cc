#include "schedule_reminder.h"
#include <esp_log.h>
#include <cJSON.h>
#include "settings.h"
#include <ctime>
#include <algorithm>

#define TAG "ScheduleReminder"

ScheduleReminder& ScheduleReminder::GetInstance() {
    static ScheduleReminder instance;
    return instance;
}

ScheduleReminder::ScheduleReminder() 
    : check_timer_(nullptr), initialized_(false) {
}

ScheduleReminder::~ScheduleReminder() {
    if (check_timer_) {
        esp_timer_stop(check_timer_);
        esp_timer_delete(check_timer_);
        check_timer_ = nullptr;
    }
}

bool ScheduleReminder::Initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "Schedule reminder already initialized");
        return true;
    }
    
    std::lock_guard<std::mutex> lock(schedules_mutex_);
    
    LoadSchedules();
    
    if (!SetupTimer()) {
        ESP_LOGE(TAG, "Failed to setup schedule timer");
        return false;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Schedule reminder initialized successfully");
    return true;
}

void ScheduleReminder::Shutdown() {
    std::lock_guard<std::mutex> lock(schedules_mutex_);
    
    if (check_timer_) {
        esp_timer_stop(check_timer_);
        esp_timer_delete(check_timer_);
        check_timer_ = nullptr;
    }
    
    initialized_ = false;
    ESP_LOGI(TAG, "Schedule reminder shutdown");
}

bool ScheduleReminder::SetupTimer() {
    esp_timer_create_args_t timer_args = {
        .callback = TimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "schedule_check_timer",
        .skip_unhandled_events = true
    };
    
    esp_err_t err = esp_timer_create(&timer_args, &check_timer_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create schedule timer: %s", esp_err_to_name(err));
        return false;
    }
    
    err = esp_timer_start_periodic(check_timer_, CONFIG_SCHEDULE_CHECK_INTERVAL * 1000000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start schedule timer: %s", esp_err_to_name(err));
        esp_timer_delete(check_timer_);
        check_timer_ = nullptr;
        return false;
    }
    
    ESP_LOGI(TAG, "Schedule timer started with interval: %d seconds", CONFIG_SCHEDULE_CHECK_INTERVAL);
    return true;
}

void ScheduleReminder::TimerCallback(void* arg) {
    ScheduleReminder* instance = static_cast<ScheduleReminder*>(arg);
    instance->CheckDueSchedules();
}

void ScheduleReminder::CheckDueSchedules() {
    if (!initialized_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(schedules_mutex_);
    
    time_t now = time(nullptr);
    bool schedules_updated = false;
    
    for (auto& item : schedules_) {
        if (!item.enabled) continue;
        
        if (item.trigger_time <= now) {
            ESP_LOGI(TAG, "Schedule due: %s", item.title.c_str());
            
            // Trigger reminder callback
            if (reminder_callback_) {
                reminder_callback_(item);
            }
            
            // Handle recurring reminders
            if (item.recurring && item.repeat_interval > 0) {
                item.trigger_time += item.repeat_interval;
                schedules_updated = true;
                ESP_LOGI(TAG, "Recurring schedule updated: %s, next trigger: %ld", 
                         item.title.c_str(), item.trigger_time);
            } else {
                item.enabled = false;  // One-time reminder, disable
                schedules_updated = true;
                ESP_LOGI(TAG, "One-time schedule disabled: %s", item.title.c_str());
            }
        }
    }
    
    // Save updated schedules
    if (schedules_updated) {
        SaveSchedules();
    }
}

ScheduleError ScheduleReminder::AddSchedule(const ScheduleItem& item) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Schedule reminder not initialized");
        return ScheduleError::kNotInitialized;
    }
    
    // Validate input
    if (item.id.empty()) {
        ESP_LOGE(TAG, "Cannot add schedule: empty ID");
        return ScheduleError::kInvalidTime;
    }
    
    if (item.trigger_time <= time(nullptr)) {
        ESP_LOGE(TAG, "Cannot add schedule: trigger time must be in the future");
        return ScheduleError::kInvalidTime;
    }
    
    std::lock_guard<std::mutex> lock(schedules_mutex_);
    
    if (schedules_.size() >= CONFIG_MAX_SCHEDULE_ITEMS) {
        ESP_LOGE(TAG, "Cannot add schedule: maximum items reached (%d)", CONFIG_MAX_SCHEDULE_ITEMS);
        return ScheduleError::kMaxItemsReached;
    }
    
    // Check if ID already exists
    for (const auto& existing : schedules_) {
        if (existing.id == item.id) {
            ESP_LOGE(TAG, "Schedule with ID %s already exists", item.id.c_str());
            return ScheduleError::kDuplicateId;
        }
    }
    
    schedules_.push_back(item);
    
    if (!SaveSchedules()) {
        ESP_LOGE(TAG, "Failed to save schedules after adding");
        schedules_.pop_back();  // Rollback
        return ScheduleError::kStorageError;
    }
    
    ESP_LOGI(TAG, "Schedule added: %s (ID: %s)", item.title.c_str(), item.id.c_str());
    return ScheduleError::kSuccess;
}

ScheduleError ScheduleReminder::RemoveSchedule(const std::string& id) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Schedule reminder not initialized");
        return ScheduleError::kNotInitialized;
    }
    
    std::lock_guard<std::mutex> lock(schedules_mutex_);
    
    auto it = std::find_if(schedules_.begin(), schedules_.end(),
        [&id](const ScheduleItem& item) { return item.id == id; });
    
    if (it != schedules_.end()) {
        schedules_.erase(it);
        
        if (!SaveSchedules()) {
            ESP_LOGE(TAG, "Failed to save schedules after removal");
            return ScheduleError::kStorageError;
        }
        
        ESP_LOGI(TAG, "Schedule removed: %s", id.c_str());
        return ScheduleError::kSuccess;
    }
    
    ESP_LOGW(TAG, "Schedule not found for removal: %s", id.c_str());
    return ScheduleError::kNotFound;
}

ScheduleError ScheduleReminder::UpdateSchedule(const std::string& id, const ScheduleItem& new_item) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Schedule reminder not initialized");
        return ScheduleError::kNotInitialized;
    }
    
    std::lock_guard<std::mutex> lock(schedules_mutex_);
    
    for (auto& item : schedules_) {
        if (item.id == id) {
            item = new_item;
            
            if (!SaveSchedules()) {
                ESP_LOGE(TAG, "Failed to save schedules after update");
                return ScheduleError::kStorageError;
            }
            
            ESP_LOGI(TAG, "Schedule updated: %s", id.c_str());
            return ScheduleError::kSuccess;
        }
    }
    
    ESP_LOGW(TAG, "Schedule not found for update: %s", id.c_str());
    return ScheduleError::kNotFound;
}

std::vector<ScheduleItem> ScheduleReminder::GetSchedules() const {
    std::lock_guard<std::mutex> lock(schedules_mutex_);
    return schedules_;
}

ScheduleItem* ScheduleReminder::GetSchedule(const std::string& id) {
    std::lock_guard<std::mutex> lock(schedules_mutex_);
    
    for (auto& item : schedules_) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

void ScheduleReminder::LoadSchedules() {
    Settings settings("schedule", true);
    std::string schedules_json = settings.GetString("schedules");
    
    if (schedules_json.empty()) {
        ESP_LOGI(TAG, "No saved schedules found");
        return;
    }
    
    cJSON* root = cJSON_Parse(schedules_json.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse schedules JSON, clearing corrupted data");
        settings.EraseKey("schedules");
        return;
    }
    
    // Data version check
    cJSON* version = cJSON_GetObjectItem(root, "version");
    if (!version || version->valueint != 1) {
        ESP_LOGW(TAG, "Unsupported schedule data version, skipping load");
        cJSON_Delete(root);
        return;
    }
    
    cJSON* schedules_array = cJSON_GetObjectItem(root, "schedules");
    if (cJSON_IsArray(schedules_array)) {
        cJSON* schedule_item;
        cJSON_ArrayForEach(schedule_item, schedules_array) {
            ScheduleItem item;
            
            cJSON* id = cJSON_GetObjectItem(schedule_item, "id");
            cJSON* title = cJSON_GetObjectItem(schedule_item, "title");
            cJSON* description = cJSON_GetObjectItem(schedule_item, "description");
            cJSON* trigger_time = cJSON_GetObjectItem(schedule_item, "trigger_time");
            cJSON* enabled = cJSON_GetObjectItem(schedule_item, "enabled");
            cJSON* recurring = cJSON_GetObjectItem(schedule_item, "recurring");
            cJSON* repeat_interval = cJSON_GetObjectItem(schedule_item, "repeat_interval");
            cJSON* created_at = cJSON_GetObjectItem(schedule_item, "created_at");
            
            if (id && cJSON_IsString(id)) item.id = id->valuestring;
            if (title && cJSON_IsString(title)) item.title = title->valuestring;
            if (description && cJSON_IsString(description)) item.description = description->valuestring;
            if (trigger_time && cJSON_IsNumber(trigger_time)) item.trigger_time = trigger_time->valueint;
            if (enabled && cJSON_IsBool(enabled)) item.enabled = cJSON_IsTrue(enabled);
            if (recurring && cJSON_IsBool(recurring)) item.recurring = cJSON_IsTrue(recurring);
            if (repeat_interval && cJSON_IsNumber(repeat_interval)) item.repeat_interval = repeat_interval->valueint;
            if (created_at && cJSON_IsString(created_at)) item.created_at = created_at->valuestring;
            
            schedules_.push_back(item);
        }
    }
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d schedules", schedules_.size());
}

bool ScheduleReminder::SaveSchedules() {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON root object");
        return false;
    }
    
    cJSON* schedules_array = cJSON_CreateArray();
    if (!schedules_array) {
        ESP_LOGE(TAG, "Failed to create schedules array");
        cJSON_Delete(root);
        return false;
    }
    
    for (const auto& item : schedules_) {
        cJSON* schedule_obj = cJSON_CreateObject();
        if (!schedule_obj) {
            ESP_LOGE(TAG, "Failed to create schedule object");
            cJSON_Delete(root);
            return false;
        }
        
        cJSON_AddStringToObject(schedule_obj, "id", item.id.c_str());
        cJSON_AddStringToObject(schedule_obj, "title", item.title.c_str());
        cJSON_AddStringToObject(schedule_obj, "description", item.description.c_str());
        cJSON_AddNumberToObject(schedule_obj, "trigger_time", item.trigger_time);
        cJSON_AddBoolToObject(schedule_obj, "enabled", item.enabled);
        cJSON_AddBoolToObject(schedule_obj, "recurring", item.recurring);
        cJSON_AddNumberToObject(schedule_obj, "repeat_interval", item.repeat_interval);
        cJSON_AddStringToObject(schedule_obj, "created_at", item.created_at.c_str());
        
        cJSON_AddItemToArray(schedules_array, schedule_obj);
    }
    
    cJSON_AddItemToObject(root, "schedules", schedules_array);
    cJSON_AddNumberToObject(root, "version", 1);  // Data version control
    
    char* json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to serialize JSON");
        cJSON_Delete(root);
        return false;
    }
    
    Settings settings("schedule", true);
    bool success = settings.SetString("schedules", json_str);
    
    free(json_str);
    cJSON_Delete(root);
    
    if (!success) {
        ESP_LOGE(TAG, "Failed to save schedules to settings");
        return false;
    }
    
    ESP_LOGI(TAG, "Schedules saved successfully (%d items)", schedules_.size());
    return true;
}

void ScheduleReminder::SetReminderCallback(std::function<void(const ScheduleItem&)> callback) {
    reminder_callback_ = callback;
}
