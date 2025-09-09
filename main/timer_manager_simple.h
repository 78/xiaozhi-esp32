#ifndef TIMER_MANAGER_SIMPLE_H
#define TIMER_MANAGER_SIMPLE_H

#include <string>
#include <ctime>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

// 简化的定时任务类型
enum class SimpleTimerType {
    COUNTDOWN,      // 倒计时
    DELAYED_EXEC    // 延时执行
};

// 简化的定时任务状态
enum class SimpleTimerStatus {
    PENDING,        // 等待中
    RUNNING,        // 运行中
    COMPLETED,      // 已完成
    CANCELLED       // 已取消
};

// 简化的定时任务结构
struct SimpleTimerTask {
    std::string id;
    std::string name;
    SimpleTimerType type;
    SimpleTimerStatus status;
    uint32_t duration_ms;
    time_t created_time;
    time_t start_time;
    time_t end_time;
    std::string mcp_tool_name;
    std::string mcp_tool_args;
    std::string description;
    
    SimpleTimerTask() : type(SimpleTimerType::COUNTDOWN), status(SimpleTimerStatus::PENDING),
                       duration_ms(0), created_time(0), start_time(0), end_time(0) {}
};

// 简化的定时任务管理器
class SimpleTimerManager {
public:
    static SimpleTimerManager& GetInstance() {
        static SimpleTimerManager instance;
        return instance;
    }

    // 基本功能
    std::string CreateCountdownTimer(const std::string& name, 
                                   uint32_t duration_ms,
                                   const std::string& description = "");
    
    std::string CreateDelayedMcpTask(const std::string& name,
                                   uint32_t delay_ms,
                                   const std::string& mcp_tool_name,
                                   const std::string& mcp_tool_args = "",
                                   const std::string& description = "");
    
    bool StartTask(const std::string& task_id);
    bool StopTask(const std::string& task_id);
    bool DeleteTask(const std::string& task_id);
    
    SimpleTimerTask* GetTask(const std::string& task_id);
    int GetTaskCount();
    std::string ExportToJson();

private:
    SimpleTimerManager();
    ~SimpleTimerManager();
    
    std::string GenerateTaskId();
    static void TimerCallback(TimerHandle_t timer_handle);
    
    SimpleTimerTask tasks_[50];  // 固定大小数组
    TimerHandle_t timers_[50];   // 对应的定时器句柄
    int task_count_;
    
    static const char* TAG;
};

#endif // TIMER_MANAGER_SIMPLE_H
