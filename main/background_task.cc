#include "background_task.h"

#include <esp_log.h>
#include <esp_task_wdt.h>

#define TAG "BackgroundTask"  // 日志标签

// 构造函数，初始化后台任务
BackgroundTask::BackgroundTask(uint32_t stack_size) {
    // 创建一个后台任务，任务名为 "background_task"，栈大小为 stack_size，优先级为 2
    xTaskCreate([](void* arg) {
        BackgroundTask* task = (BackgroundTask*)arg;  // 获取当前对象的指针
        task->BackgroundTaskLoop();  // 执行后台任务循环
    }, "background_task", stack_size, this, 2, &background_task_handle_);  // 任务句柄
}

// 析构函数，释放资源
BackgroundTask::~BackgroundTask() {
    if (background_task_handle_ != nullptr) {
        vTaskDelete(background_task_handle_);  // 删除后台任务
    }
}

// 调度任务，将任务加入任务队列
void BackgroundTask::Schedule(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);  // 加锁，确保线程安全
    if (active_tasks_ >= 30) {  // 如果当前活跃任务数超过 30
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);  // 获取当前空闲的 SRAM
        if (free_sram < 10000) {  // 如果空闲 SRAM 小于 10000 字节
            ESP_LOGW(TAG, "active_tasks_ == %u, free_sram == %u", active_tasks_.load(), free_sram);  // 打印警告日志
        }
    }
    active_tasks_++;  // 增加活跃任务计数
    // 将任务加入任务队列，任务执行完成后减少活跃任务计数
    main_tasks_.emplace_back([this, cb = std::move(callback)]() {
        cb();  // 执行回调函数
        {
            std::lock_guard<std::mutex> lock(mutex_);  // 加锁
            active_tasks_--;  // 减少活跃任务计数
            if (main_tasks_.empty() && active_tasks_ == 0) {  // 如果任务队列为空且没有活跃任务
                condition_variable_.notify_all();  // 通知所有等待的线程
            }
        }
    });
    condition_variable_.notify_all();  // 通知后台任务线程有新任务
}

// 等待所有任务完成
void BackgroundTask::WaitForCompletion() {
    std::unique_lock<std::mutex> lock(mutex_);  // 加锁
    // 等待条件变量，直到任务队列为空且没有活跃任务
    condition_variable_.wait(lock, [this]() {
        return main_tasks_.empty() && active_tasks_ == 0;
    });
}

// 后台任务循环，不断从任务队列中取出任务并执行
void BackgroundTask::BackgroundTaskLoop() {
    ESP_LOGI(TAG, "background_task started");  // 打印日志，后台任务启动
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);  // 加锁
        // 等待条件变量，直到任务队列不为空
        condition_variable_.wait(lock, [this]() { return !main_tasks_.empty(); });
        
        std::list<std::function<void()>> tasks = std::move(main_tasks_);  // 取出所有任务
        lock.unlock();  // 解锁

        for (auto& task : tasks) {
            task();  // 执行任务
        }
    }
}