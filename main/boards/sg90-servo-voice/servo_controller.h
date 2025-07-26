#ifndef __SERVO_CONTROLLER_H__
#define __SERVO_CONTROLLER_H__

#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <functional>
#include "config.h"

class ServoController {
public:
    ServoController(gpio_num_t servo_pin);
    ~ServoController();

    // 基本控制方法
    bool Initialize();
    void SetAngle(int angle);
    int GetCurrentAngle() const { return current_angle_; }
    
    // 运动控制方法
    void RotateClockwise(int degrees);
    void RotateCounterclockwise(int degrees);
    void SweepBetween(int min_angle, int max_angle, int speed_ms = 1000);
    void Stop();
    void Reset(); // 回到中心位置（90度）
    
    // 状态查询
    bool IsMoving() const { return is_moving_; }
    bool IsSweeping() const { return is_sweeping_; }
    
    // 设置回调函数（当运动完成时调用）
    void SetOnMoveCompleteCallback(std::function<void()> callback) {
        on_move_complete_callback_ = callback;
    }

private:
    // 硬件相关
    gpio_num_t servo_pin_;
    ledc_channel_t ledc_channel_;
    ledc_timer_t ledc_timer_;
    
    // 状态变量
    int current_angle_;
    int target_angle_;
    bool is_moving_;
    bool is_sweeping_;
    bool stop_requested_;
    
    // 任务和队列
    TaskHandle_t servo_task_handle_;
    QueueHandle_t command_queue_;
    
    // 回调函数
    std::function<void()> on_move_complete_callback_;
    
    // 命令类型
    enum CommandType {
        CMD_SET_ANGLE,
        CMD_ROTATE_CW,
        CMD_ROTATE_CCW,
        CMD_SWEEP,
        CMD_STOP,
        CMD_RESET
    };
    
    // 命令结构
    struct ServoCommand {
        CommandType type;
        int param1;  // 角度或度数
        int param2;  // 最大角度（用于扫描）或速度
        int param3;  // 速度参数
    };
    
    // 私有方法
    void WriteAngle(int angle);
    uint32_t AngleToCompare(int angle);
    bool IsValidAngle(int angle) const;
    int ConstrainAngle(int angle) const;
    
    // 任务函数
    static void ServoTask(void* parameter);
    void ProcessCommands();
    void ExecuteSetAngle(int angle);
    void ExecuteRotate(int degrees, bool clockwise);
    void ExecuteSweep(int min_angle, int max_angle, int speed_ms);
    void SmoothMoveTo(int target_angle, int speed_ms = 500);
};

#endif // __SERVO_CONTROLLER_H__
