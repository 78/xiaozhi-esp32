#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

// 定时任务类型
enum class TimerType {
    COUNTDOWN,      // 倒计时
    DELAYED_EXEC,   // 延时执行
    PERIODIC,       // 周期性任务
    SCHEDULED       // 定时执行
};

// 定时任务状态
enum class TimerStatus {
    PENDING,        // 等待中
    RUNNING,        // 运行中
    COMPLETED,      // 已完成
    CANCELLED,      // 已取消
    FAILED          // 失败
};

// MCP工具回调函数类型
using McpToolCallback = std::function<bool(const std::string& tool_name, const std::string& arguments)>;

// 定时任务结构
struct TimerTask {
    std::string id;                 // 唯一标识符
    std::string name;               // 任务名称
    TimerType type;                 // 任务类型
    TimerStatus status;             // 任务状态
    uint32_t duration_ms;           // 持续时间（毫秒）
    time_t scheduled_time;          // 预定执行时间
    time_t created_time;            // 创建时间
    time_t start_time;              // 开始时间
    time_t end_time;                // 结束时间
    
    // MCP工具相关
    std::string mcp_tool_name;      // MCP工具名称
    std::string mcp_tool_args;      // MCP工具参数
    McpToolCallback callback;       // 回调函数
    
    // 周期性任务相关
    uint32_t interval_ms;           // 间隔时间（毫秒）
    int repeat_count;               // 重复次数（-1表示无限）
    int current_repeat;             // 当前重复次数
    
    // 用户数据
    std::string user_data;          // 用户自定义数据
    std::string description;        // 任务描述
    
    TimerTask() : type(TimerType::COUNTDOWN), status(TimerStatus::PENDING),
                 duration_ms(0), scheduled_time(0), created_time(0),
                 start_time(0), end_time(0), interval_ms(0),
                 repeat_count(0), current_repeat(0) {}
};

class TimerManager {
public:
    static TimerManager& GetInstance() {
        static TimerManager instance;
        return instance;
    }

    // 倒计时器功能
    std::string CreateCountdownTimer(const std::string& name, 
                                   uint32_t duration_ms,
                                   const std::string& description = "");
    
    // 延时执行MCP工具
    std::string CreateDelayedMcpTask(const std::string& name,
                                   uint32_t delay_ms,
                                   const std::string& mcp_tool_name,
                                   const std::string& mcp_tool_args = "",
                                   const std::string& description = "");
    
    // 周期性任务
    std::string CreatePeriodicTask(const std::string& name,
                                 uint32_t interval_ms,
                                 int repeat_count = -1,  // -1表示无限重复
                                 const std::string& mcp_tool_name = "",
                                 const std::string& mcp_tool_args = "",
                                 const std::string& description = "");
    
    // 定时执行任务
    std::string CreateScheduledTask(const std::string& name,
                                  time_t scheduled_time,
                                  const std::string& mcp_tool_name,
                                  const std::string& mcp_tool_args = "",
                                  const std::string& description = "");
    
    // 任务管理
    bool StartTask(const std::string& task_id);
    bool StopTask(const std::string& task_id);
    bool CancelTask(const std::string& task_id);
    bool DeleteTask(const std::string& task_id);
    
    // 查询功能
    TimerTask* GetTask(const std::string& task_id);
    std::vector<TimerTask> GetAllTasks();
    std::vector<TimerTask> GetTasksByStatus(TimerStatus status);
    std::vector<TimerTask> GetRunningTasks();
    std::vector<TimerTask> GetUpcomingTasks(int minutes = 60);
    
    // 统计功能
    int GetTaskCount();
    int GetTaskCountByStatus(TimerStatus status);
    int GetTaskCountByType(TimerType type);
    
    // 系统控制
    void StartManager();
    void StopManager();
    bool IsRunning();
    
    // 回调设置
    void SetTaskCompletedCallback(std::function<void(const TimerTask&)> callback);
    void SetTaskFailedCallback(std::function<void(const TimerTask&, const std::string&)> callback);
    
    // 数据持久化
    bool SaveToStorage();
    bool LoadFromStorage();
    
    // 导出功能
    std::string ExportToJson();

private:
    TimerManager();
    ~TimerManager();
    
    // 内部方法
    std::string GenerateTaskId();
    void TaskWorker();
    void ExecuteTask(TimerTask& task);
    bool ExecuteMcpTool(const std::string& tool_name, const std::string& args);
    void UpdateTaskStatus(TimerTask& task, TimerStatus status);
    void NotifyTaskCompleted(const TimerTask& task);
    void NotifyTaskFailed(const TimerTask& task, const std::string& error);
    
    // FreeRTOS定时器回调
    static void TimerCallback(TimerHandle_t timer_handle);
    
    std::map<std::string, TimerTask> tasks_;
    std::map<std::string, TimerHandle_t> timers_;
    std::mutex tasks_mutex_;
    std::atomic<bool> is_running_;
    std::thread worker_thread_;
    
    // 回调函数
    std::function<void(const TimerTask&)> task_completed_callback_;
    std::function<void(const TimerTask&, const std::string&)> task_failed_callback_;
    
    static const char* TAG;
};

#endif // TIMER_MANAGER_H
