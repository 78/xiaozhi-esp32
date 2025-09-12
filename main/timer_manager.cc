#include "timer_manager.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cstring>
#include <sstream>

const char* TimerManager::TAG = "TimerManager";

TimerManager::TimerManager() : is_running_(false) {
    ESP_LOGI(TAG, "TimerManager initialized");
}

TimerManager::~TimerManager() {
    StopManager();
    ESP_LOGI(TAG, "TimerManager destroyed");
}

std::string TimerManager::CreateCountdownTimer(const std::string& name, 
                                             uint32_t duration_ms,
                                             const std::string& description) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    TimerTask task;
    task.id = GenerateTaskId();
    task.name = name;
    task.type = TimerType::COUNTDOWN;
    task.status = TimerStatus::PENDING;
    task.duration_ms = duration_ms;
    task.description = description;
    task.created_time = time(nullptr);
    
    tasks_[task.id] = task;
    
    ESP_LOGI(TAG, "Created countdown timer: %s (ID: %s, Duration: %u ms)", 
             name.c_str(), task.id.c_str(), duration_ms);
    
    return task.id;
}

std::string TimerManager::CreateDelayedMcpTask(const std::string& name,
                                             uint32_t delay_ms,
                                             const std::string& mcp_tool_name,
                                             const std::string& mcp_tool_args,
                                             const std::string& description) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    TimerTask task;
    task.id = GenerateTaskId();
    task.name = name;
    task.type = TimerType::DELAYED_EXEC;
    task.status = TimerStatus::PENDING;
    task.duration_ms = delay_ms;
    task.mcp_tool_name = mcp_tool_name;
    task.mcp_tool_args = mcp_tool_args;
    task.description = description;
    task.created_time = time(nullptr);
    
    tasks_[task.id] = task;
    
    ESP_LOGI(TAG, "Created delayed MCP task: %s (ID: %s, Delay: %u ms, Tool: %s)", 
             name.c_str(), task.id.c_str(), delay_ms, mcp_tool_name.c_str());
    
    return task.id;
}

std::string TimerManager::CreatePeriodicTask(const std::string& name,
                                           uint32_t interval_ms,
                                           int repeat_count,
                                           const std::string& mcp_tool_name,
                                           const std::string& mcp_tool_args,
                                           const std::string& description) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    TimerTask task;
    task.id = GenerateTaskId();
    task.name = name;
    task.type = TimerType::PERIODIC;
    task.status = TimerStatus::PENDING;
    task.interval_ms = interval_ms;
    task.repeat_count = repeat_count;
    task.current_repeat = 0;
    task.mcp_tool_name = mcp_tool_name;
    task.mcp_tool_args = mcp_tool_args;
    task.description = description;
    task.created_time = time(nullptr);
    
    tasks_[task.id] = task;
    
    ESP_LOGI(TAG, "Created periodic task: %s (ID: %s, Interval: %u ms, Repeat: %d)", 
             name.c_str(), task.id.c_str(), interval_ms, repeat_count);
    
    return task.id;
}

std::string TimerManager::CreateScheduledTask(const std::string& name,
                                            time_t scheduled_time,
                                            const std::string& mcp_tool_name,
                                            const std::string& mcp_tool_args,
                                            const std::string& description) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    TimerTask task;
    task.id = GenerateTaskId();
    task.name = name;
    task.type = TimerType::SCHEDULED;
    task.status = TimerStatus::PENDING;
    task.scheduled_time = scheduled_time;
    task.mcp_tool_name = mcp_tool_name;
    task.mcp_tool_args = mcp_tool_args;
    task.description = description;
    task.created_time = time(nullptr);
    
    tasks_[task.id] = task;
    
    ESP_LOGI(TAG, "Created scheduled task: %s (ID: %s, Time: %ld)", 
             name.c_str(), task.id.c_str(), scheduled_time);
    
    return task.id;
}

bool TimerManager::StartTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        ESP_LOGE(TAG, "Task not found: %s", task_id.c_str());
        return false;
    }
    
    TimerTask& task = it->second;
    
    if (task.status != TimerStatus::PENDING) {
        ESP_LOGW(TAG, "Task %s is not in pending status", task_id.c_str());
        return false;
    }
    
    // 创建FreeRTOS定时器
    TimerHandle_t timer_handle = xTimerCreate(
        task.name.c_str(),
        pdMS_TO_TICKS(task.duration_ms),
        (task.type == TimerType::PERIODIC) ? pdTRUE : pdFALSE,
        (void*)task_id.c_str(),
        TimerCallback
    );
    
    if (timer_handle == nullptr) {
        ESP_LOGE(TAG, "Failed to create timer for task: %s", task_id.c_str());
        return false;
    }
    
    timers_[task_id] = timer_handle;
    task.status = TimerStatus::RUNNING;
    task.start_time = time(nullptr);
    
    if (xTimerStart(timer_handle, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer for task: %s", task_id.c_str());
        xTimerDelete(timer_handle, 0);
        timers_.erase(task_id);
        task.status = TimerStatus::FAILED;
        return false;
    }
    
    ESP_LOGI(TAG, "Started task: %s", task_id.c_str());
    return true;
}

bool TimerManager::StopTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        ESP_LOGE(TAG, "Task not found: %s", task_id.c_str());
        return false;
    }
    
    TimerTask& task = it->second;
    
    if (task.status != TimerStatus::RUNNING) {
        ESP_LOGW(TAG, "Task %s is not running", task_id.c_str());
        return false;
    }
    
    auto timer_it = timers_.find(task_id);
    if (timer_it != timers_.end()) {
        xTimerStop(timer_it->second, 0);
        xTimerDelete(timer_it->second, 0);
        timers_.erase(timer_it);
    }
    
    task.status = TimerStatus::CANCELLED;
    task.end_time = time(nullptr);
    
    ESP_LOGI(TAG, "Stopped task: %s", task_id.c_str());
    return true;
}

bool TimerManager::CancelTask(const std::string& task_id) {
    return StopTask(task_id);
}

bool TimerManager::DeleteTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    // 先停止任务
    StopTask(task_id);
    
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        ESP_LOGE(TAG, "Task not found: %s", task_id.c_str());
        return false;
    }
    
    tasks_.erase(it);
    ESP_LOGI(TAG, "Deleted task: %s", task_id.c_str());
    return true;
}

TimerTask* TimerManager::GetTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return nullptr;
    }
    
    return &it->second;
}

std::vector<TimerTask> TimerManager::GetAllTasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    std::vector<TimerTask> result;
    
    for (const auto& pair : tasks_) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<TimerTask> TimerManager::GetTasksByStatus(TimerStatus status) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    std::vector<TimerTask> result;
    
    for (const auto& pair : tasks_) {
        if (pair.second.status == status) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

std::vector<TimerTask> TimerManager::GetRunningTasks() {
    return GetTasksByStatus(TimerStatus::RUNNING);
}

std::vector<TimerTask> TimerManager::GetUpcomingTasks(int minutes) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    std::vector<TimerTask> result;
    
    time_t now = time(nullptr);
    time_t future_time = now + (minutes * 60);
    
    for (const auto& pair : tasks_) {
        const TimerTask& task = pair.second;
        if (task.status == TimerStatus::PENDING && 
            task.scheduled_time >= now && 
            task.scheduled_time <= future_time) {
            result.push_back(task);
        }
    }
    
    return result;
}

int TimerManager::GetTaskCount() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    return tasks_.size();
}

int TimerManager::GetTaskCountByStatus(TimerStatus status) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    int count = 0;
    
    for (const auto& pair : tasks_) {
        if (pair.second.status == status) {
            count++;
        }
    }
    
    return count;
}

int TimerManager::GetTaskCountByType(TimerType type) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    int count = 0;
    
    for (const auto& pair : tasks_) {
        if (pair.second.type == type) {
            count++;
        }
    }
    
    return count;
}

void TimerManager::StartManager() {
    if (is_running_) {
        ESP_LOGW(TAG, "TimerManager is already running");
        return;
    }
    
    is_running_ = true;
    worker_thread_ = std::thread(&TimerManager::TaskWorker, this);
    
    ESP_LOGI(TAG, "TimerManager started");
}

void TimerManager::StopManager() {
    if (!is_running_) {
        return;
    }
    
    is_running_ = false;
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    // 停止所有定时器
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    for (auto& pair : timers_) {
        xTimerStop(pair.second, 0);
        xTimerDelete(pair.second, 0);
    }
    timers_.clear();
    
    ESP_LOGI(TAG, "TimerManager stopped");
}

bool TimerManager::IsRunning() {
    return is_running_;
}

void TimerManager::SetTaskCompletedCallback(std::function<void(const TimerTask&)> callback) {
    task_completed_callback_ = callback;
}

void TimerManager::SetTaskFailedCallback(std::function<void(const TimerTask&, const std::string&)> callback) {
    task_failed_callback_ = callback;
}

bool TimerManager::SaveToStorage() {
    // TODO: 实现数据持久化到NVS或SPIFFS
    ESP_LOGW(TAG, "SaveToStorage not implemented yet");
    return true;
}

bool TimerManager::LoadFromStorage() {
    // TODO: 实现从NVS或SPIFFS加载数据
    ESP_LOGW(TAG, "LoadFromStorage not implemented yet");
    return true;
}

std::string TimerManager::ExportToJson() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    std::stringstream json;
    json << "{\"tasks\":[";
    
    bool first = true;
    for (const auto& pair : tasks_) {
        if (!first) json << ",";
        first = false;
        
        const TimerTask& task = pair.second;
        json << "{"
             << "\"id\":\"" << task.id << "\","
             << "\"name\":\"" << task.name << "\","
             << "\"description\":\"" << task.description << "\","
             << "\"duration_ms\":" << task.duration_ms << ","
             << "\"interval_ms\":" << task.interval_ms << ","
             << "\"repeat_count\":" << task.repeat_count << ","
             << "\"current_repeat\":" << task.current_repeat << ","
             << "\"created_time\":" << task.created_time << ","
             << "\"start_time\":" << task.start_time << ","
             << "\"end_time\":" << task.end_time << ","
             << "\"scheduled_time\":" << task.scheduled_time << ","
             << "\"mcp_tool_name\":\"" << task.mcp_tool_name << "\","
             << "\"mcp_tool_args\":\"" << task.mcp_tool_args << "\","
             << "\"user_data\":\"" << task.user_data << "\",";
        
        std::string status_str;
        switch (task.status) {
            case TimerStatus::PENDING: status_str = "pending"; break;
            case TimerStatus::RUNNING: status_str = "running"; break;
            case TimerStatus::COMPLETED: status_str = "completed"; break;
            case TimerStatus::CANCELLED: status_str = "cancelled"; break;
            case TimerStatus::FAILED: status_str = "failed"; break;
        }
        json << "\"status\":\"" << status_str << "\",";
        
        std::string type_str;
        switch (task.type) {
            case TimerType::COUNTDOWN: type_str = "countdown"; break;
            case TimerType::DELAYED_EXEC: type_str = "delayed_exec"; break;
            case TimerType::PERIODIC: type_str = "periodic"; break;
            case TimerType::SCHEDULED: type_str = "scheduled"; break;
        }
        json << "\"type\":\"" << type_str << "\"";
        
        json << "}";
    }
    
    json << "]}";
    return json.str();
}

std::string TimerManager::GenerateTaskId() {
    static int counter = 0;
    return "task_" + std::to_string(++counter) + "_" + std::to_string(time(nullptr));
}

void TimerManager::TaskWorker() {
    ESP_LOGI(TAG, "Task worker thread started");
    
    while (is_running_) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // 检查定时任务
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        time_t now = time(nullptr);
        
        for (auto& pair : tasks_) {
            TimerTask& task = pair.second;
            
            if (task.status == TimerStatus::PENDING && 
                task.type == TimerType::SCHEDULED &&
                now >= task.scheduled_time) {
                
                ESP_LOGI(TAG, "Executing scheduled task: %s", task.id.c_str());
                ExecuteTask(task);
            }
        }
    }
    
    ESP_LOGI(TAG, "Task worker thread stopped");
}

void TimerManager::ExecuteTask(TimerTask& task) {
    task.status = TimerStatus::RUNNING;
    task.start_time = time(nullptr);
    
    bool success = true;
    std::string error_msg;
    
    try {
        if (!task.mcp_tool_name.empty()) {
            success = ExecuteMcpTool(task.mcp_tool_name, task.mcp_tool_args);
            if (!success) {
                error_msg = "MCP tool execution failed";
            }
        }
    } catch (const std::exception& e) {
        success = false;
        error_msg = e.what();
    } catch (...) {
        success = false;
        error_msg = "Unknown error occurred";
    }
    
    task.end_time = time(nullptr);
    
    if (success) {
        if (task.type == TimerType::PERIODIC) {
            task.current_repeat++;
            if (task.repeat_count == -1 || task.current_repeat < task.repeat_count) {
                // 继续下一次重复
                task.status = TimerStatus::PENDING;
                ESP_LOGI(TAG, "Periodic task %s completed repeat %d/%d", 
                         task.id.c_str(), task.current_repeat, task.repeat_count);
            } else {
                // 所有重复完成
                task.status = TimerStatus::COMPLETED;
                ESP_LOGI(TAG, "Periodic task %s completed all repeats", task.id.c_str());
                NotifyTaskCompleted(task);
            }
        } else {
            task.status = TimerStatus::COMPLETED;
            ESP_LOGI(TAG, "Task %s completed successfully", task.id.c_str());
            NotifyTaskCompleted(task);
        }
    } else {
        task.status = TimerStatus::FAILED;
        ESP_LOGE(TAG, "Task %s failed: %s", task.id.c_str(), error_msg.c_str());
        NotifyTaskFailed(task, error_msg);
    }
}

bool TimerManager::ExecuteMcpTool(const std::string& tool_name, const std::string& args) {
    // TODO: 实现MCP工具执行
    ESP_LOGW(TAG, "ExecuteMcpTool not implemented yet: %s with args: %s", 
             tool_name.c_str(), args.c_str());
    return true;
}

void TimerManager::UpdateTaskStatus(TimerTask& task, TimerStatus status) {
    task.status = status;
    if (status == TimerStatus::RUNNING) {
        task.start_time = time(nullptr);
    } else if (status == TimerStatus::COMPLETED || status == TimerStatus::FAILED) {
        task.end_time = time(nullptr);
    }
}

void TimerManager::NotifyTaskCompleted(const TimerTask& task) {
    if (task_completed_callback_) {
        task_completed_callback_(task);
    }
}

void TimerManager::NotifyTaskFailed(const TimerTask& task, const std::string& error) {
    if (task_failed_callback_) {
        task_failed_callback_(task, error);
    }
}

void TimerManager::TimerCallback(TimerHandle_t timer_handle) {
    const char* task_id = (const char*)pvTimerGetTimerID(timer_handle);
    
    ESP_LOGI(TAG, "Timer callback triggered for task: %s", task_id);
    
    // 获取任务管理器实例
    TimerManager& manager = TimerManager::GetInstance();
    
    std::lock_guard<std::mutex> lock(manager.tasks_mutex_);
    
    auto it = manager.tasks_.find(task_id);
    if (it != manager.tasks_.end()) {
        TimerTask& task = it->second;
        
        if (task.type == TimerType::COUNTDOWN) {
            // 倒计时完成
            task.status = TimerStatus::COMPLETED;
            task.end_time = time(nullptr);
            ESP_LOGI(TAG, "Countdown timer %s completed", task_id);
            manager.NotifyTaskCompleted(task);
        } else if (task.type == TimerType::DELAYED_EXEC) {
            // 延时执行MCP工具
            manager.ExecuteTask(task);
        }
    }
    
    // 删除一次性定时器
    if (xTimerIsTimerActive(timer_handle) == pdFALSE) {
        xTimerDelete(timer_handle, 0);
        manager.timers_.erase(task_id);
    }
}
