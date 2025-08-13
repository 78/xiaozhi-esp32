#ifndef SKILLS_VIBRATION_H_
#define SKILLS_VIBRATION_H_

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <string>
#include <functional>
#include "../config.h"

// 所有预设的、声明式的振动模式ID
typedef enum {
    VIBRATION_SHORT_BUZZ,        // 轻抚头部 - 短促清脆的确认反馈
    VIBRATION_PURR_SHORT,        // 轻抚头部 - 短促的咕噜声
    VIBRATION_PURR_PATTERN,      // 按住头部 - 持续的咕噜咕噜声
    VIBRATION_GENTLE_HEARTBEAT,  // 按住头部/被拥抱 - 温暖平稳的心跳
    VIBRATION_STRUGGLE_PATTERN,  // 按住头部/被倒置 - 表达挣扎的不规律振动
    VIBRATION_SHARP_BUZZ,        // 轻触身体 - 表达被打扰的尖锐振动
    VIBRATION_TREMBLE_PATTERN,   // 被拿起(不开心时) - 表达害怕的颤抖
    VIBRATION_GIGGLE_PATTERN,    // 被挠痒痒 - 模拟笑到发抖的欢快振动
    VIBRATION_HEARTBEAT_STRONG,  // (掌心的约定) - 表达力量和信念的强心跳
    VIBRATION_ERRATIC_STRONG,    // 被剧烈晃动 - 表达眩晕的混乱强振动
    VIBRATION_STOP,              // 停止所有振动的特殊命令
    VIBRATION_MAX                // 枚举最大值，用于边界检查（id是否大于MAX）
} vibration_id_t;

// 振动关键帧结构，定义每个振动步骤的强度和持续时间
typedef struct {
    uint8_t strength;     // 振动强度 (0-255)
    uint16_t duration_ms; // 持续时间 (毫秒)
} vibration_keyframe_t;

/**
 * @brief 振动技能管理类
 *        管理所有触觉反馈，支持情绪驱动的振动模式
 */
class Vibration {
private:
    gpio_num_t vibration_pin_;
    QueueHandle_t vibration_queue_;
    TaskHandle_t vibration_task_handle_;
    bool initialized_;
    vibration_id_t current_pattern_;
    std::string current_emotion_;
    bool emotion_based_enabled_;

    // 静态任务函数
    static void VibrationTask(void* parameter);
    static void ButtonTestTask(void* parameter);
    
    // 私有方法
    void SetVibrationStrength(uint8_t strength);
    void PlayVibrationPattern(const vibration_keyframe_t* pattern);
    esp_err_t InitVibrationGpio();
    esp_err_t InitTestButton();
    vibration_id_t GetVibrationForEmotion(const std::string& emotion);
    
    // 测试相关成员变量
    gpio_num_t test_button_pin_;
    TaskHandle_t button_test_task_handle_;
    vibration_id_t current_test_pattern_;
    bool button_test_enabled_;
    bool cycle_test_mode_;  // 是否为循环测试模式

public:
    Vibration();
    ~Vibration();

    /**
     * @brief 初始化振动系统（只初始化GPIO）
     * @return ESP_OK 如果成功，其他错误码如果失败
     */
    esp_err_t Initialize();

    /**
     * @brief 启动振动任务（创建队列和任务）
     * @return ESP_OK 如果成功，其他错误码如果失败
     */
    esp_err_t StartTask();

    /**
     * @brief 播放指定的振动模式
     * @param id 振动模式ID
     */
    void Play(vibration_id_t id);

    /**
     * @brief 停止所有振动
     */
    void Stop();

    /**
     * @brief 根据情绪播放对应的振动
     * @param emotion 情绪字符串
     */
    void PlayForEmotion(const std::string& emotion);

    /**
     * @brief 启用/禁用基于情绪的自动振动
     * @param enabled true为启用，false为禁用
     */
    void SetEmotionBasedEnabled(bool enabled);

    /**
     * @brief 检查是否已初始化
     * @return true如果已初始化
     */
    bool IsInitialized() const { return initialized_; }

    /**
     * @brief 获取当前播放的振动模式
     * @return 当前振动模式ID
     */
    vibration_id_t GetCurrentPattern() const { return current_pattern_; }

    /**
     * @brief 获取当前情绪
     * @return 当前情绪字符串
     */
    const std::string& GetCurrentEmotion() const { return current_emotion_; }

    /**
     * @brief 启用按键测试功能
     * @param pattern_id 按下按键时执行的振动模式ID
     */
    void EnableButtonTest(vibration_id_t pattern_id);

    /**
     * @brief 启用按键循环测试功能（按一次按键循环测试所有振动模式）
     */
    void EnableButtonCycleTest();

    /**
     * @brief 禁用按键测试功能
     */
    void DisableButtonTest();

    /**
     * @brief 设置当前测试的振动模式
     * @param pattern_id 振动模式ID
     */
    void SetTestPattern(vibration_id_t pattern_id);

    /**
     * @brief 获取当前测试的振动模式
     * @return 当前测试的振动模式ID
     */
    vibration_id_t GetTestPattern() const { return current_test_pattern_; }
};

#endif // SKILLS_VIBRATION_H_