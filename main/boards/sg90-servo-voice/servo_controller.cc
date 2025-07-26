#include "servo_controller.h"
#include <esp_log.h>
#include <cmath>

#define TAG "ServoController"

ServoController::ServoController(gpio_num_t servo_pin)
    : servo_pin_(servo_pin)
    , ledc_channel_(LEDC_CHANNEL_0)
    , ledc_timer_(LEDC_TIMER_0)
    , current_angle_(SERVO_DEFAULT_ANGLE)
    , target_angle_(SERVO_DEFAULT_ANGLE)
    , is_moving_(false)
    , is_sweeping_(false)
    , stop_requested_(false)
    , servo_task_handle_(nullptr)
    , command_queue_(nullptr)
    , on_move_complete_callback_(nullptr) {
}

ServoController::~ServoController() {
    Stop();
    if (servo_task_handle_ != nullptr) {
        vTaskDelete(servo_task_handle_);
    }
    if (command_queue_ != nullptr) {
        vQueueDelete(command_queue_);
    }
}

bool ServoController::Initialize() {
    ESP_LOGI(TAG, "初始化SG90舵机控制器，引脚: %d", servo_pin_);
    
    // 配置LEDC定时器 (ESP32-S3最大支持14位分辨率)
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = ledc_timer_,
        .freq_hz = 50, // 50Hz for servo
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC定时器配置失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 配置LEDC通道
    ledc_channel_config_t channel_config = {
        .gpio_num = servo_pin_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = ledc_channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = ledc_timer_,
        .duty = 0,
        .hpoint = 0
    };
    
    ret = ledc_channel_config(&channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC通道配置失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 创建命令队列
    command_queue_ = xQueueCreate(10, sizeof(ServoCommand));
    if (command_queue_ == nullptr) {
        ESP_LOGE(TAG, "创建命令队列失败");
        return false;
    }
    
    // 创建舵机控制任务
    BaseType_t task_ret = xTaskCreate(
        ServoTask,
        "servo_task",
        4096,
        this,
        5,
        &servo_task_handle_
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "创建舵机任务失败");
        return false;
    }
    
    // 设置初始位置
    WriteAngle(current_angle_);
    
    ESP_LOGI(TAG, "SG90舵机控制器初始化成功");
    return true;
}

void ServoController::SetAngle(int angle) {
    if (!IsValidAngle(angle)) {
        ESP_LOGW(TAG, "无效角度: %d，将限制在有效范围内", angle);
        angle = ConstrainAngle(angle);
    }
    
    ServoCommand cmd = {CMD_SET_ANGLE, angle, 0, 0};
    xQueueSend(command_queue_, &cmd, portMAX_DELAY);
}

void ServoController::RotateClockwise(int degrees) {
    if (degrees <= 0) {
        ESP_LOGW(TAG, "旋转角度必须大于0");
        return;
    }
    
    ServoCommand cmd = {CMD_ROTATE_CW, degrees, 0, 0};
    xQueueSend(command_queue_, &cmd, portMAX_DELAY);
}

void ServoController::RotateCounterclockwise(int degrees) {
    if (degrees <= 0) {
        ESP_LOGW(TAG, "旋转角度必须大于0");
        return;
    }
    
    ServoCommand cmd = {CMD_ROTATE_CCW, degrees, 0, 0};
    xQueueSend(command_queue_, &cmd, portMAX_DELAY);
}

void ServoController::SweepBetween(int min_angle, int max_angle, int speed_ms) {
    if (!IsValidAngle(min_angle) || !IsValidAngle(max_angle)) {
        ESP_LOGW(TAG, "扫描角度范围无效: %d - %d", min_angle, max_angle);
        return;
    }
    
    if (min_angle >= max_angle) {
        ESP_LOGW(TAG, "最小角度必须小于最大角度");
        return;
    }
    
    ServoCommand cmd = {CMD_SWEEP, min_angle, max_angle, speed_ms};
    xQueueSend(command_queue_, &cmd, portMAX_DELAY);
}

void ServoController::Stop() {
    stop_requested_ = true;
    ServoCommand cmd = {CMD_STOP, 0, 0, 0};
    xQueueSend(command_queue_, &cmd, 0); // 不等待，立即发送停止命令
}

void ServoController::Reset() {
    ServoCommand cmd = {CMD_RESET, SERVO_DEFAULT_ANGLE, 0, 0};
    xQueueSend(command_queue_, &cmd, portMAX_DELAY);
}

void ServoController::WriteAngle(int angle) {
    angle = ConstrainAngle(angle);
    uint32_t compare_value = AngleToCompare(angle);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ledc_channel_, compare_value);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ledc_channel_);
    current_angle_ = angle;
}

uint32_t ServoController::AngleToCompare(int angle) {
    // 将角度转换为PWM占空比
    // SG90: 0.5ms-2.5ms 对应 0-180度
    // 50Hz周期 = 20ms
    // 占空比 = (脉宽 / 周期) * 2^14 (ESP32-S3使用14位分辨率)

    float pulse_width_ms = 0.5f + (angle / 180.0f) * 2.0f; // 0.5ms to 2.5ms
    float duty_cycle = pulse_width_ms / 20.0f; // 20ms period
    uint32_t compare_value = (uint32_t)(duty_cycle * 16383); // 14-bit resolution (2^14 - 1)

    return compare_value;
}

bool ServoController::IsValidAngle(int angle) const {
    return angle >= SERVO_MIN_DEGREE && angle <= SERVO_MAX_DEGREE;
}

int ServoController::ConstrainAngle(int angle) const {
    if (angle < SERVO_MIN_DEGREE) return SERVO_MIN_DEGREE;
    if (angle > SERVO_MAX_DEGREE) return SERVO_MAX_DEGREE;
    return angle;
}

void ServoController::ServoTask(void* parameter) {
    ServoController* controller = static_cast<ServoController*>(parameter);
    controller->ProcessCommands();
}

void ServoController::ProcessCommands() {
    ServoCommand cmd;
    
    while (true) {
        if (xQueueReceive(command_queue_, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (stop_requested_ && cmd.type != CMD_STOP) {
                continue; // 忽略非停止命令
            }
            
            switch (cmd.type) {
                case CMD_SET_ANGLE:
                    ExecuteSetAngle(cmd.param1);
                    break;
                    
                case CMD_ROTATE_CW:
                    ExecuteRotate(cmd.param1, true);
                    break;
                    
                case CMD_ROTATE_CCW:
                    ExecuteRotate(cmd.param1, false);
                    break;
                    
                case CMD_SWEEP:
                    ExecuteSweep(cmd.param1, cmd.param2, cmd.param3);
                    break;
                    
                case CMD_STOP:
                    is_moving_ = false;
                    is_sweeping_ = false;
                    stop_requested_ = false;
                    ESP_LOGI(TAG, "舵机停止");
                    break;
                    
                case CMD_RESET:
                    ExecuteSetAngle(cmd.param1);
                    break;
            }
        }
    }
}

void ServoController::ExecuteSetAngle(int angle) {
    ESP_LOGI(TAG, "设置舵机角度: %d度", angle);
    is_moving_ = true;
    SmoothMoveTo(angle, 500);
    is_moving_ = false;

    if (on_move_complete_callback_) {
        on_move_complete_callback_();
    }
}

void ServoController::ExecuteRotate(int degrees, bool clockwise) {
    int target = current_angle_ + (clockwise ? degrees : -degrees);
    target = ConstrainAngle(target);

    ESP_LOGI(TAG, "%s旋转 %d度，从 %d度 到 %d度",
             clockwise ? "顺时针" : "逆时针", degrees, current_angle_, target);

    is_moving_ = true;
    SmoothMoveTo(target, 500);
    is_moving_ = false;

    if (on_move_complete_callback_) {
        on_move_complete_callback_();
    }
}

void ServoController::ExecuteSweep(int min_angle, int max_angle, int speed_ms) {
    ESP_LOGI(TAG, "开始扫描模式: %d度 - %d度，速度: %dms", min_angle, max_angle, speed_ms);

    is_sweeping_ = true;
    is_moving_ = true;

    bool direction = true; // true = 向最大角度，false = 向最小角度

    while (is_sweeping_ && !stop_requested_) {
        int target = direction ? max_angle : min_angle;
        SmoothMoveTo(target, speed_ms);

        if (stop_requested_) break;

        direction = !direction;
        vTaskDelay(pdMS_TO_TICKS(100)); // 短暂停顿
    }

    is_sweeping_ = false;
    is_moving_ = false;

    ESP_LOGI(TAG, "扫描模式结束");

    if (on_move_complete_callback_) {
        on_move_complete_callback_();
    }
}

void ServoController::SmoothMoveTo(int target_angle, int speed_ms) {
    target_angle = ConstrainAngle(target_angle);

    if (target_angle == current_angle_) {
        return; // 已经在目标位置
    }

    int start_angle = current_angle_;
    int angle_diff = target_angle - start_angle;
    int steps = abs(angle_diff);

    if (steps == 0) return;

    int delay_per_step = speed_ms / steps;
    if (delay_per_step < 10) delay_per_step = 10; // 最小延迟

    for (int i = 1; i <= steps && !stop_requested_; i++) {
        int current_step_angle = start_angle + (angle_diff * i) / steps;
        WriteAngle(current_step_angle);
        vTaskDelay(pdMS_TO_TICKS(delay_per_step));
    }

    // 确保到达精确位置
    if (!stop_requested_) {
        WriteAngle(target_angle);
    }
}
