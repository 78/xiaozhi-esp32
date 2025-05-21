#ifndef BACKGROUND_TASK_H
#define BACKGROUND_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>
#include <list>
#include <condition_variable>
#include <atomic>

class BackgroundTask {
public:
    BackgroundTask(uint32_t stack_size = 4096 * 2);
    ~BackgroundTask();

    void Schedule(std::function<void()> callback);
    void WaitForCompletion();

private:
    std::mutex mutex_;
    std::list<std::function<void()>> main_tasks_;
    std::condition_variable condition_variable_;
    TaskHandle_t background_task_handle_ = nullptr;
    std::atomic<size_t> active_tasks_{0};

    void BackgroundTaskLoop();
};

#endif
