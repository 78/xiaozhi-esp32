#include "timer_manager_simple.h"
#include <esp_log.h>
#include <cstring>
#include <ctime>

const char* SimpleTimerManager::TAG = "SimpleTimerManager";

SimpleTimerManager::SimpleTimerManager() : task_count_(0) {
    // 初始化定时器句柄数组
    for (int i = 0; i < 50; i++) {
        timers_[i] = nullptr;
    }
    ESP_LOGI(TAG, "SimpleTimerManager initialized");
}

SimpleTimerManager::~SimpleTimerManager() {
    // 清理所有定时器
    for (int i = 0; i < task_count_; i++) {
        if (timers_[i] != nullptr) {
            xTimerStop(timers_[i], 0);
            xTimerDelete(timers_[i], 0);
        }
    }
    ESP_LOGI(TAG, "SimpleTimerManager destroyed");
}

std::string SimpleTimerManager::CreateCountdownTimer(const std::string& name, 
                                                   uint32_t duration_ms,
                                                   const std::string& description) {
    if (task_count_ >= 50) {
        ESP_LOGE(TAG, "Maximum task count reached");
        return "";
    }
    
    SimpleTimerTask& task = tasks_[task_count_];
    task.id = GenerateTaskId();
    task.name = name;
    task.type = SimpleTimerType::COUNTDOWN;
    task.status = SimpleTimerStatus::PENDING;
    task.duration_ms = duration_ms;
    task.description = description;
    task.created_time = time(nullptr);
    
    task_count_++;
    
    ESP_LOGI(TAG, "Created countdown timer: %s (ID: %s, Duration: %u ms)", 
             name.c_str(), task.id.c_str(), duration_ms);
    
    return task.id;
}

std::string SimpleTimerManager::CreateDelayedMcpTask(const std::string& name,
                                                   uint32_t delay_ms,
                                                   const std::string& mcp_tool_name,
                                                   const std::string& mcp_tool_args,
                                                   const std::string& description) {
    if (task_count_ >= 50) {
        ESP_LOGE(TAG, "Maximum task count reached");
        return "";
    }
    
    SimpleTimerTask& task = tasks_[task_count_];
    task.id = GenerateTaskId();
    task.name = name;
    task.type = SimpleTimerType::DELAYED_EXEC;
    task.status = SimpleTimerStatus::PENDING;
    task.duration_ms = delay_ms;
    task.mcp_tool_name = mcp_tool_name;
    task.mcp_tool_args = mcp_tool_args;
    task.description = description;
    task.created_time = time(nullptr);
    
    task_count_++;
    
    ESP_LOGI(TAG, "Created delayed MCP task: %s (ID: %s, Delay: %u ms, Tool: %s)", 
             name.c_str(), task.id.c_str(), delay_ms, mcp_tool_name.c_str());
    
    return task.id;
}

bool SimpleTimerManager::StartTask(const std::string& task_id) {
    for (int i = 0; i < task_count_; i++) {
        if (tasks_[i].id == task_id) {
            SimpleTimerTask& task = tasks_[i];
            
            if (task.status != SimpleTimerStatus::PENDING) {
                ESP_LOGW(TAG, "Task %s is not in pending status", task_id.c_str());
                return false;
            }
            
            // 创建FreeRTOS定时器
            TimerHandle_t timer_handle = xTimerCreate(
                task.name.c_str(),
                pdMS_TO_TICKS(task.duration_ms),
                pdFALSE,  // 一次性定时器
                (void*)task_id.c_str(),
                TimerCallback
            );
            
            if (timer_handle == nullptr) {
                ESP_LOGE(TAG, "Failed to create timer for task: %s", task_id.c_str());
                return false;
            }
            
            timers_[i] = timer_handle;
            task.status = SimpleTimerStatus::RUNNING;
            task.start_time = time(nullptr);
            
            if (xTimerStart(timer_handle, 0) != pdPASS) {
                ESP_LOGE(TAG, "Failed to start timer for task: %s", task_id.c_str());
                xTimerDelete(timer_handle, 0);
                timers_[i] = nullptr;
                task.status = SimpleTimerStatus::CANCELLED;
                return false;
            }
            
            ESP_LOGI(TAG, "Started task: %s", task_id.c_str());
            return true;
        }
    }
    
    ESP_LOGE(TAG, "Task not found: %s", task_id.c_str());
    return false;
}

bool SimpleTimerManager::StopTask(const std::string& task_id) {
    for (int i = 0; i < task_count_; i++) {
        if (tasks_[i].id == task_id) {
            SimpleTimerTask& task = tasks_[i];
            
            if (task.status != SimpleTimerStatus::RUNNING) {
                ESP_LOGW(TAG, "Task %s is not running", task_id.c_str());
                return false;
            }
            
            if (timers_[i] != nullptr) {
                xTimerStop(timers_[i], 0);
                xTimerDelete(timers_[i], 0);
                timers_[i] = nullptr;
            }
            
            task.status = SimpleTimerStatus::CANCELLED;
            task.end_time = time(nullptr);
            
            ESP_LOGI(TAG, "Stopped task: %s", task_id.c_str());
            return true;
        }
    }
    
    ESP_LOGE(TAG, "Task not found: %s", task_id.c_str());
    return false;
}

bool SimpleTimerManager::DeleteTask(const std::string& task_id) {
    // 先停止任务
    StopTask(task_id);
    
    for (int i = 0; i < task_count_; i++) {
        if (tasks_[i].id == task_id) {
            // 移动后面的元素
            for (int j = i; j < task_count_ - 1; j++) {
                tasks_[j] = tasks_[j + 1];
                timers_[j] = timers_[j + 1];
            }
            timers_[task_count_ - 1] = nullptr;
            task_count_--;
            
            ESP_LOGI(TAG, "Deleted task: %s", task_id.c_str());
            return true;
        }
    }
    
    ESP_LOGE(TAG, "Task not found: %s", task_id.c_str());
    return false;
}

SimpleTimerTask* SimpleTimerManager::GetTask(const std::string& task_id) {
    for (int i = 0; i < task_count_; i++) {
        if (tasks_[i].id == task_id) {
            return &tasks_[i];
        }
    }
    return nullptr;
}

int SimpleTimerManager::GetTaskCount() {
    return task_count_;
}

std::string SimpleTimerManager::ExportToJson() {
    std::string json = "{\"tasks\":[";
    
    for (int i = 0; i < task_count_; i++) {
        if (i > 0) json += ",";
        
        const SimpleTimerTask& task = tasks_[i];
        json += "{";
        json += "\"id\":\"" + task.id + "\",";
        json += "\"name\":\"" + task.name + "\",";
        json += "\"description\":\"" + task.description + "\",";
        json += "\"duration_ms\":" + std::to_string(task.duration_ms) + ",";
        json += "\"created_time\":" + std::to_string(task.created_time) + ",";
        json += "\"start_time\":" + std::to_string(task.start_time) + ",";
        json += "\"end_time\":" + std::to_string(task.end_time) + ",";
        json += "\"mcp_tool_name\":\"" + task.mcp_tool_name + "\",";
        json += "\"mcp_tool_args\":\"" + task.mcp_tool_args + "\",";
        
        std::string status_str;
        switch (task.status) {
            case SimpleTimerStatus::PENDING: status_str = "pending"; break;
            case SimpleTimerStatus::RUNNING: status_str = "running"; break;
            case SimpleTimerStatus::COMPLETED: status_str = "completed"; break;
            case SimpleTimerStatus::CANCELLED: status_str = "cancelled"; break;
        }
        json += "\"status\":\"" + status_str + "\",";
        
        std::string type_str;
        switch (task.type) {
            case SimpleTimerType::COUNTDOWN: type_str = "countdown"; break;
            case SimpleTimerType::DELAYED_EXEC: type_str = "delayed_exec"; break;
        }
        json += "\"type\":\"" + type_str + "\"";
        
        json += "}";
    }
    
    json += "]}";
    return json;
}

std::string SimpleTimerManager::GenerateTaskId() {
    static int counter = 0;
    return "task_" + std::to_string(++counter) + "_" + std::to_string(time(nullptr));
}

void SimpleTimerManager::TimerCallback(TimerHandle_t timer_handle) {
    const char* task_id = (const char*)pvTimerGetTimerID(timer_handle);
    
    ESP_LOGI(TAG, "Timer callback triggered for task: %s", task_id);
    
    // 获取任务管理器实例
    SimpleTimerManager& manager = SimpleTimerManager::GetInstance();
    
    // 查找并更新任务状态
    for (int i = 0; i < manager.task_count_; i++) {
        if (manager.tasks_[i].id == task_id) {
            SimpleTimerTask& task = manager.tasks_[i];
            
            if (task.type == SimpleTimerType::COUNTDOWN) {
                // 倒计时完成
                task.status = SimpleTimerStatus::COMPLETED;
                task.end_time = time(nullptr);
                ESP_LOGI(TAG, "Countdown timer %s completed", task_id);
            } else if (task.type == SimpleTimerType::DELAYED_EXEC) {
                // 延时执行MCP工具
                task.status = SimpleTimerStatus::COMPLETED;
                task.end_time = time(nullptr);
                ESP_LOGI(TAG, "Delayed MCP task %s completed", task_id);
                // TODO: 执行MCP工具
            }
            
            // 清理定时器
            if (manager.timers_[i] != nullptr) {
                xTimerDelete(manager.timers_[i], 0);
                manager.timers_[i] = nullptr;
            }
            break;
        }
    }
}
