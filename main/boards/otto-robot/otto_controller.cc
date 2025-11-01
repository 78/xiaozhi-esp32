/*
    Otto机器人控制器 - MCP协议版本
*/

#include <cJSON.h>
#include <esp_log.h>

#include <cstdlib> 
#include <cstring>

#include "application.h"
#include "board.h"
#include "config.h"
#include "mcp_server.h"
#include "otto_movements.h"
#include "sdkconfig.h"
#include "settings.h"

#define TAG "OttoController"

class OttoController {
private:
    Otto otto_;
    TaskHandle_t action_task_handle_ = nullptr;
    QueueHandle_t action_queue_;
    bool has_hands_ = false;
    bool is_action_in_progress_ = false;

    struct OttoActionParams {
        int action_type;
        int steps;
        int speed;
        int direction;
        int amount;
        char servo_sequence_json[512];  // 用于存储舵机序列的JSON字符串
    };

    enum ActionType {
        ACTION_WALK = 1,
        ACTION_TURN = 2,
        ACTION_JUMP = 3,
        ACTION_SWING = 4,
        ACTION_MOONWALK = 5,
        ACTION_BEND = 6,
        ACTION_SHAKE_LEG = 7,
        ACTION_SIT = 25,  // 坐下
        ACTION_RADIO_CALISTHENICS = 26,  // 广播体操
        ACTION_MAGIC_CIRCLE = 27,  // 爱的魔力转圈圈
        ACTION_UPDOWN = 8,
        ACTION_TIPTOE_SWING = 9,
        ACTION_JITTER = 10,
        ACTION_ASCENDING_TURN = 11,
        ACTION_CRUSAITO = 12,
        ACTION_FLAPPING = 13,
        ACTION_HANDS_UP = 14,
        ACTION_HANDS_DOWN = 15,
        ACTION_HAND_WAVE = 16,
        ACTION_WINDMILL = 20,  // 大风车
        ACTION_TAKEOFF = 21,   // 起飞
        ACTION_FITNESS = 22,   // 健身
        ACTION_GREETING = 23,  // 打招呼
        ACTION_SHY = 24,        // 害羞
        ACTION_SHOWCASE = 28,   // 展示动作
        ACTION_HOME = 17,
        ACTION_SERVO_SEQUENCE = 18,  // 舵机序列（自编程）
        ACTION_WHIRLWIND_LEG = 19    // 旋风腿
    };

    static void ActionTask(void* arg) {
        OttoController* controller = static_cast<OttoController*>(arg);
        OttoActionParams params;
        controller->otto_.AttachServos();

        while (true) {
            if (xQueueReceive(controller->action_queue_, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
                ESP_LOGI(TAG, "执行动作: %d", params.action_type);
                controller->is_action_in_progress_ = true;
                if (params.action_type == ACTION_SERVO_SEQUENCE) {
                    // 执行舵机序列（自编程）- 仅支持短键名格式
                    cJSON* json = cJSON_Parse(params.servo_sequence_json);
                    if (json != nullptr) {
                        ESP_LOGD(TAG, "JSON解析成功，长度=%d", strlen(params.servo_sequence_json));
                        // 使用短键名 "a" 表示动作数组
                        cJSON* actions = cJSON_GetObjectItem(json, "a");
                        if (cJSON_IsArray(actions)) {
                            int array_size = cJSON_GetArraySize(actions);
                            ESP_LOGI(TAG, "执行舵机序列，共%d个动作", array_size);
                            
                            // 获取序列执行完成后的延迟（短键名 "d"，顶层参数）
                            int sequence_delay = 0;
                            cJSON* delay_item = cJSON_GetObjectItem(json, "d");
                            if (cJSON_IsNumber(delay_item)) {
                                sequence_delay = delay_item->valueint;
                                if (sequence_delay < 0) sequence_delay = 0;
                            }
                            
                            // 初始化当前舵机位置（用于保持未指定的舵机位置）
                            int current_positions[SERVO_COUNT];
                            for (int j = 0; j < SERVO_COUNT; j++) {
                                current_positions[j] = 90;  // 默认中间位置
                            }
                            // 手部舵机默认位置
                            current_positions[LEFT_HAND] = 45;
                            current_positions[RIGHT_HAND] = 180 - 45;
                            
                            for (int i = 0; i < array_size; i++) {
                                cJSON* action_item = cJSON_GetArrayItem(actions, i);
                                if (cJSON_IsObject(action_item)) {
                                    // 检查是否为振荡器模式（短键名 "osc"）
                                    cJSON* osc_item = cJSON_GetObjectItem(action_item, "osc");
                                    if (cJSON_IsObject(osc_item)) {
                                        // 振荡器模式 - 使用Execute2，以绝对角度为中心振荡
                                        int amplitude[SERVO_COUNT] = {0};
                                        int center_angle[SERVO_COUNT] = {0};
                                        double phase_diff[SERVO_COUNT] = {0};
                                        int period = 500;  // 默认周期500毫秒
                                        float steps = 5.0;  // 默认步数5.0
                                        
                                        const char* servo_names[] = {"ll", "rl", "lf", "rf", "lh", "rh"};
                                        
                                        // 读取振幅（短键名 "a"），默认20度
                                        for (int j = 0; j < SERVO_COUNT; j++) {
                                            amplitude[j] = 20;  // 默认振幅20度
                                        }
                                        cJSON* amp_item = cJSON_GetObjectItem(osc_item, "a");
                                        if (cJSON_IsObject(amp_item)) {
                                            for (int j = 0; j < SERVO_COUNT; j++) {
                                                cJSON* amp_value = cJSON_GetObjectItem(amp_item, servo_names[j]);
                                                if (cJSON_IsNumber(amp_value)) {
                                                    int amp = amp_value->valueint;
                                                    if (amp >= 10 && amp <= 90) {
                                                        amplitude[j] = amp;
                                                    }
                                                }
                                            }
                                        }
                                        
                                        // 读取中心角度（短键名 "o"），默认90度（绝对角度0-180度）
                                        for (int j = 0; j < SERVO_COUNT; j++) {
                                            center_angle[j] = 90;  // 默认中心角度90度（中间位置）
                                        }
                                        cJSON* center_item = cJSON_GetObjectItem(osc_item, "o");
                                        if (cJSON_IsObject(center_item)) {
                                            for (int j = 0; j < SERVO_COUNT; j++) {
                                                cJSON* center_value = cJSON_GetObjectItem(center_item, servo_names[j]);
                                                if (cJSON_IsNumber(center_value)) {
                                                    int center = center_value->valueint;
                                                    if (center >= 0 && center <= 180) {
                                                        center_angle[j] = center;
                                                    }
                                                }
                                            }
                                        }
                                        
                                        // 安全检查：防止左右腿脚同时做大幅度振荡（振幅检查）
                                        const int LARGE_AMPLITUDE_THRESHOLD = 40;  // 大幅度振幅阈值：40度
                                        bool left_leg_large = amplitude[LEFT_LEG] >= LARGE_AMPLITUDE_THRESHOLD;
                                        bool right_leg_large = amplitude[RIGHT_LEG] >= LARGE_AMPLITUDE_THRESHOLD;
                                        bool left_foot_large = amplitude[LEFT_FOOT] >= LARGE_AMPLITUDE_THRESHOLD;
                                        bool right_foot_large = amplitude[RIGHT_FOOT] >= LARGE_AMPLITUDE_THRESHOLD;
                                        
                                        if (left_leg_large && right_leg_large) {
                                            ESP_LOGW(TAG, "检测到左右腿同时大幅度振荡，限制右腿振幅");
                                            amplitude[RIGHT_LEG] = 0;  // 禁止右腿振荡
                                        }
                                        if (left_foot_large && right_foot_large) {
                                            ESP_LOGW(TAG, "检测到左右脚同时大幅度振荡，限制右脚振幅");
                                            amplitude[RIGHT_FOOT] = 0;  // 禁止右脚振荡
                                        }
                                        
                                        // 读取相位差（短键名 "ph"，单位为度，转换为弧度）
                                        cJSON* phase_item = cJSON_GetObjectItem(osc_item, "ph");
                                        if (cJSON_IsObject(phase_item)) {
                                            for (int j = 0; j < SERVO_COUNT; j++) {
                                                cJSON* phase_value = cJSON_GetObjectItem(phase_item, servo_names[j]);
                                                if (cJSON_IsNumber(phase_value)) {
                                                    // 将度数转换为弧度
                                                    phase_diff[j] = phase_value->valuedouble * 3.141592653589793 / 180.0;
                                                }
                                            }
                                        }
                                        
                                        // 读取周期（短键名 "p"），范围100-3000毫秒
                                        cJSON* period_item = cJSON_GetObjectItem(osc_item, "p");
                                        if (cJSON_IsNumber(period_item)) {
                                            period = period_item->valueint;
                                            if (period < 100) period = 100;
                                            if (period > 3000) period = 3000;  // 与描述一致，限制3000毫秒
                                        }
                                        
                                        // 读取周期数（短键名 "c"），范围0.1-20.0
                                        cJSON* steps_item = cJSON_GetObjectItem(osc_item, "c");
                                        if (cJSON_IsNumber(steps_item)) {
                                            steps = (float)steps_item->valuedouble;
                                            if (steps < 0.1) steps = 0.1;
                                            if (steps > 20.0) steps = 20.0;  // 与描述一致，限制20.0
                                        }
                                        
                                        // 执行振荡 - 使用Execute2，以绝对角度为中心
                                        ESP_LOGI(TAG, "执行振荡动作%d: period=%d, steps=%.1f", i, period, steps);
                                        controller->otto_.Execute2(amplitude, center_angle, period, phase_diff, steps);
                                        
                                        // 振荡后更新位置（使用center_angle作为最终位置）
                                        for (int j = 0; j < SERVO_COUNT; j++) {
                                            current_positions[j] = center_angle[j];
                                        }
                                    } else {
                                        // 普通移动模式
                                        // 从当前位置数组复制，保持未指定的舵机位置
                                        int servo_target[SERVO_COUNT];
                                        for (int j = 0; j < SERVO_COUNT; j++) {
                                            servo_target[j] = current_positions[j];
                                        }
                                        
                                        // 从JSON中读取舵机位置（短键名 "s"）
                                        cJSON* servos_item = cJSON_GetObjectItem(action_item, "s");
                                        if (cJSON_IsObject(servos_item)) {
                                            // 短键名：ll/rl/lf/rf/lh/rh
                                            const char* servo_names[] = {"ll", "rl", "lf", "rf", "lh", "rh"};
                                            
                                            for (int j = 0; j < SERVO_COUNT; j++) {
                                                cJSON* servo_value = cJSON_GetObjectItem(servos_item, servo_names[j]);
                                                if (cJSON_IsNumber(servo_value)) {
                                                    int position = servo_value->valueint;
                                                    // 限制位置范围在0-180度
                                                    if (position >= 0 && position <= 180) {
                                                        servo_target[j] = position;
                                                    }
                                                }
                                            }
                                        }
                                        
                                        // 安全检查：防止左右腿脚同时做大幅度动作
                                        const int LARGE_MOVEMENT_THRESHOLD = 40;  // 大幅度动作阈值：40度
                                        bool left_leg_large = abs(servo_target[LEFT_LEG] - current_positions[LEFT_LEG]) >= LARGE_MOVEMENT_THRESHOLD;
                                        bool right_leg_large = abs(servo_target[RIGHT_LEG] - current_positions[RIGHT_LEG]) >= LARGE_MOVEMENT_THRESHOLD;
                                        bool left_foot_large = abs(servo_target[LEFT_FOOT] - current_positions[LEFT_FOOT]) >= LARGE_MOVEMENT_THRESHOLD;
                                        bool right_foot_large = abs(servo_target[RIGHT_FOOT] - current_positions[RIGHT_FOOT]) >= LARGE_MOVEMENT_THRESHOLD;
                                        
                                        if (left_leg_large && right_leg_large) {
                                            ESP_LOGW(TAG, "检测到左右腿同时大幅度动作，限制右腿动作");
                                            // 保持右腿在原位置
                                            servo_target[RIGHT_LEG] = current_positions[RIGHT_LEG];
                                        }
                                        if (left_foot_large && right_foot_large) {
                                            ESP_LOGW(TAG, "检测到左右脚同时大幅度动作，限制右脚动作");
                                            // 保持右脚在原位置
                                            servo_target[RIGHT_FOOT] = current_positions[RIGHT_FOOT];
                                        }
                                        
                                        // 获取移动速度（短键名 "v"，默认1000毫秒）
                                        int speed = 1000;
                                        cJSON* speed_item = cJSON_GetObjectItem(action_item, "v");
                                        if (cJSON_IsNumber(speed_item)) {
                                            speed = speed_item->valueint;
                                            if (speed < 100) speed = 100;  // 最小100毫秒
                                            if (speed > 3000) speed = 3000;  // 最大3000毫秒
                                        }
                                        
                                        // 执行舵机移动
                                        ESP_LOGI(TAG, "执行动作%d: ll=%d, rl=%d, lf=%d, rf=%d, v=%d",
                                                 i, servo_target[LEFT_LEG], servo_target[RIGHT_LEG],
                                                 servo_target[LEFT_FOOT], servo_target[RIGHT_FOOT], speed);
                                        controller->otto_.MoveServos(speed, servo_target);
                                        
                                        // 更新当前位置数组，用于下一个动作
                                        for (int j = 0; j < SERVO_COUNT; j++) {
                                            current_positions[j] = servo_target[j];
                                        }
                                    }
                                    
                                    // 获取动作后的延迟时间（短键名 "d"）
                                    int delay_after = 0;
                                    cJSON* delay_item = cJSON_GetObjectItem(action_item, "d");
                                    if (cJSON_IsNumber(delay_item)) {
                                        delay_after = delay_item->valueint;
                                        if (delay_after < 0) delay_after = 0;
                                    }
                                    
                                    // 动作后的延迟（最后一个动作后不延迟）
                                    if (delay_after > 0 && i < array_size - 1) {
                                        ESP_LOGI(TAG, "动作%d执行完成，延迟%d毫秒", i, delay_after);
                                        vTaskDelay(pdMS_TO_TICKS(delay_after));
                                    }
                                }
                            }
                            
                            // 序列执行完成后的延迟（用于序列之间的停顿）
                            if (sequence_delay > 0) {
                                // 检查队列中是否还有待执行的序列
                                UBaseType_t queue_count = uxQueueMessagesWaiting(controller->action_queue_);
                                if (queue_count > 0) {
                                    ESP_LOGI(TAG, "序列执行完成，延迟%d毫秒后执行下一个序列（队列中还有%d个序列）", 
                                             sequence_delay, queue_count);
                                    vTaskDelay(pdMS_TO_TICKS(sequence_delay));
                                }
                            }
                            // 释放JSON内存
                            cJSON_Delete(json);
                        } else {
                            ESP_LOGE(TAG, "舵机序列格式错误: 'a'不是数组");
                            cJSON_Delete(json);
                        }
                    } else {
                        // 获取cJSON的错误信息
                        const char* error_ptr = cJSON_GetErrorPtr();
                        int json_len = strlen(params.servo_sequence_json);
                        ESP_LOGE(TAG, "解析舵机序列JSON失败，长度=%d，错误位置: %s", json_len, 
                                 error_ptr ? error_ptr : "未知");
                        ESP_LOGE(TAG, "JSON内容: %s", params.servo_sequence_json);
                    }
                } else {
                    // 执行预定义动作
                    switch (params.action_type) {
                        case ACTION_WALK:
                            controller->otto_.Walk(params.steps, params.speed, params.direction,
                                                   params.amount);
                            break;
                        case ACTION_TURN:
                            controller->otto_.Turn(params.steps, params.speed, params.direction,
                                                   params.amount);
                            break;
                        case ACTION_JUMP:
                            controller->otto_.Jump(params.steps, params.speed);
                            break;
                        case ACTION_SWING:
                            controller->otto_.Swing(params.steps, params.speed, params.amount);
                            break;
                        case ACTION_MOONWALK:
                            controller->otto_.Moonwalker(params.steps, params.speed, params.amount,
                                                         params.direction);
                            break;
                        case ACTION_BEND:
                            controller->otto_.Bend(params.steps, params.speed, params.direction);
                            break;
                        case ACTION_SHAKE_LEG:
                            controller->otto_.ShakeLeg(params.steps, params.speed, params.direction);
                            break;
                        case ACTION_SIT:
                            controller->otto_.Sit();
                            break;
                        case ACTION_RADIO_CALISTHENICS:
                            if (controller->has_hands_) {
                                controller->otto_.RadioCalisthenics();
                            }
                            break;
                        case ACTION_MAGIC_CIRCLE:
                            if (controller->has_hands_) {
                                controller->otto_.MagicCircle();
                            }
                            break;
                        case ACTION_SHOWCASE:
                            controller->otto_.Showcase();
                            break;
                        case ACTION_UPDOWN:
                            controller->otto_.UpDown(params.steps, params.speed, params.amount);
                            break;
                        case ACTION_TIPTOE_SWING:
                            controller->otto_.TiptoeSwing(params.steps, params.speed, params.amount);
                            break;
                        case ACTION_JITTER:
                            controller->otto_.Jitter(params.steps, params.speed, params.amount);
                            break;
                        case ACTION_ASCENDING_TURN:
                            controller->otto_.AscendingTurn(params.steps, params.speed, params.amount);
                            break;
                        case ACTION_CRUSAITO:
                            controller->otto_.Crusaito(params.steps, params.speed, params.amount,
                                                       params.direction);
                            break;
                        case ACTION_FLAPPING:
                            controller->otto_.Flapping(params.steps, params.speed, params.amount,
                                                       params.direction);
                            break;
                        case ACTION_WHIRLWIND_LEG:
                            controller->otto_.WhirlwindLeg(params.steps, params.speed, params.amount);
                            break;
                        case ACTION_HANDS_UP:
                            if (controller->has_hands_) {
                                controller->otto_.HandsUp(params.speed, params.direction);
                            }
                            break;
                        case ACTION_HANDS_DOWN:
                            if (controller->has_hands_) {
                                controller->otto_.HandsDown(params.speed, params.direction);
                            }
                            break;
                        case ACTION_HAND_WAVE:
                            if (controller->has_hands_) {
                                controller->otto_.HandWave( params.direction);
                            }
                            break;
                        case ACTION_WINDMILL:
                            if (controller->has_hands_) {
                                controller->otto_.Windmill(params.steps, params.speed, params.amount);
                            }
                            break;
                        case ACTION_TAKEOFF:
                            if (controller->has_hands_) {
                                controller->otto_.Takeoff(params.steps, params.speed, params.amount);
                            }
                            break;
                        case ACTION_FITNESS:
                            if (controller->has_hands_) {
                                controller->otto_.Fitness(params.steps, params.speed, params.amount);
                            }
                            break;
                        case ACTION_GREETING:
                            if (controller->has_hands_) {
                                controller->otto_.Greeting(params.direction, params.steps);
                            }
                            break;
                        case ACTION_SHY:
                            if (controller->has_hands_) {
                                controller->otto_.Shy(params.direction, params.steps);
                            }
                            break;
                        case ACTION_HOME:
                            controller->otto_.Home(true);
                            break;
                    }
                    if(params.action_type != ACTION_SIT){
                        if (params.action_type != ACTION_HOME && params.action_type != ACTION_SERVO_SEQUENCE) {
                            controller->otto_.Home(params.action_type != ACTION_HANDS_UP);
                        }
                    }
                }
                controller->is_action_in_progress_ = false;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
    }

    void StartActionTaskIfNeeded() {
        if (action_task_handle_ == nullptr) {
            xTaskCreate(ActionTask, "otto_action", 1024 * 3, this, configMAX_PRIORITIES - 1,
                        &action_task_handle_);
        }
    }

    void QueueAction(int action_type, int steps, int speed, int direction, int amount) {
        // 检查手部动作
        if ((action_type >= ACTION_HANDS_UP && action_type <= ACTION_HAND_WAVE) || 
            (action_type == ACTION_WINDMILL) || (action_type == ACTION_TAKEOFF) || 
            (action_type == ACTION_FITNESS) || (action_type == ACTION_GREETING) ||
            (action_type == ACTION_SHY) || (action_type == ACTION_RADIO_CALISTHENICS) ||
            (action_type == ACTION_MAGIC_CIRCLE)) {
            if (!has_hands_) {
                ESP_LOGW(TAG, "尝试执行手部动作，但机器人没有配置手部舵机");
                return;
            }
        }

        ESP_LOGI(TAG, "动作控制: 类型=%d, 步数=%d, 速度=%d, 方向=%d, 幅度=%d", action_type, steps,
                 speed, direction, amount);

        OttoActionParams params = {action_type, steps, speed, direction, amount, ""};
        xQueueSend(action_queue_, &params, portMAX_DELAY);
        StartActionTaskIfNeeded();
    }

    void QueueServoSequence(const char* servo_sequence_json) {
        if (servo_sequence_json == nullptr) {
            ESP_LOGE(TAG, "序列JSON为空");
            return;
        }
        
        int input_len = strlen(servo_sequence_json);
        const int buffer_size = 512;  // servo_sequence_json数组大小
        ESP_LOGI(TAG, "队列舵机序列，输入长度=%d，缓冲区大小=%d", input_len, buffer_size);
        
        if (input_len >= buffer_size) {
            ESP_LOGE(TAG, "JSON字符串太长！输入长度=%d，最大允许=%d", input_len, buffer_size - 1);
            return;
        }
        
        if (input_len == 0) {
            ESP_LOGW(TAG, "序列JSON为空字符串");
            return;
        }
        
        OttoActionParams params = {ACTION_SERVO_SEQUENCE, 0, 0, 0, 0, ""};
        // 复制JSON字符串到结构体中（限制长度）
        strncpy(params.servo_sequence_json, servo_sequence_json, sizeof(params.servo_sequence_json) - 1);
        params.servo_sequence_json[sizeof(params.servo_sequence_json) - 1] = '\0';
        
        ESP_LOGD(TAG, "序列已加入队列: %s", params.servo_sequence_json);
        
        xQueueSend(action_queue_, &params, portMAX_DELAY);
        StartActionTaskIfNeeded();
    }

    void LoadTrimsFromNVS() {
        Settings settings("otto_trims", false);

        int left_leg = settings.GetInt("left_leg", 0);
        int right_leg = settings.GetInt("right_leg", 0);
        int left_foot = settings.GetInt("left_foot", 0);
        int right_foot = settings.GetInt("right_foot", 0);
        int left_hand = settings.GetInt("left_hand", 0);
        int right_hand = settings.GetInt("right_hand", 0);

        ESP_LOGI(TAG, "从NVS加载微调设置: 左腿=%d, 右腿=%d, 左脚=%d, 右脚=%d, 左手=%d, 右手=%d",
                 left_leg, right_leg, left_foot, right_foot, left_hand, right_hand);

        otto_.SetTrims(left_leg, right_leg, left_foot, right_foot, left_hand, right_hand);
    }

public:
    OttoController() {
        otto_.Init(LEFT_LEG_PIN, RIGHT_LEG_PIN, LEFT_FOOT_PIN, RIGHT_FOOT_PIN, LEFT_HAND_PIN,
                   RIGHT_HAND_PIN);

        has_hands_ = (LEFT_HAND_PIN != -1 && RIGHT_HAND_PIN != -1);
        ESP_LOGI(TAG, "Otto机器人初始化%s手部舵机", has_hands_ ? "带" : "不带");

        LoadTrimsFromNVS();

        action_queue_ = xQueueCreate(10, sizeof(OttoActionParams));

        QueueAction(ACTION_HOME, 1, 1000, 1, 0);  // direction=1表示复位手部

        RegisterMcpTools();
    }

    void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        ESP_LOGI(TAG, "开始注册MCP工具...");

        // 基础移动动作
        mcp_server.AddTool("self.otto.walk_forward",
                           "行走。steps: 行走步数(1-100); speed: 行走速度(500-1500，数值越小越快); "
                           "direction: 行走方向(-1=后退, 1=前进); arm_swing: 手臂摆动幅度(0-170度)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 1, 100),
                                         Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                                         Property("arm_swing", kPropertyTypeInteger, 50, 0, 170),
                                         Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               int arm_swing = properties["arm_swing"].value<int>();
                               int direction = properties["direction"].value<int>();
                               QueueAction(ACTION_WALK, steps, speed, direction, arm_swing);
                               return true;
                           });

        mcp_server.AddTool("self.otto.turn_left",
                           "转身。steps: 转身步数(1-100); speed: 转身速度(500-1500，数值越小越快); "
                           "direction: 转身方向(1=左转, -1=右转); arm_swing: 手臂摆动幅度(0-170度)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 1, 100),
                                         Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                                         Property("arm_swing", kPropertyTypeInteger, 50, 0, 170),
                                         Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               int arm_swing = properties["arm_swing"].value<int>();
                               int direction = properties["direction"].value<int>();
                               QueueAction(ACTION_TURN, steps, speed, direction, arm_swing);
                               return true;
                           });

        mcp_server.AddTool("self.otto.jump",
                           "跳跃。steps: 跳跃次数(1-100); speed: 跳跃速度(500-1500，数值越小越快)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 1, 1, 100),
                                         Property("speed", kPropertyTypeInteger, 1000, 500, 1500)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               QueueAction(ACTION_JUMP, steps, speed, 0, 0);
                               return true;
                           });

        // 特殊动作
        mcp_server.AddTool("self.otto.swing",
                           "左右摇摆。steps: 摇摆次数(1-100); speed: "
                           "摇摆速度(500-1500，数值越小越快); amount: 摇摆幅度(0-170度)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 1, 100),
                                         Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                                         Property("amount", kPropertyTypeInteger, 30, 0, 170)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               int amount = properties["amount"].value<int>();
                               QueueAction(ACTION_SWING, steps, speed, 0, amount);
                               return true;
                           });

        mcp_server.AddTool("self.otto.moonwalk",
                           "太空步。steps: 太空步步数(1-100); speed: 速度(500-1500，数值越小越快); "
                           "direction: 方向(1=左, -1=右); amount: 幅度(0-170度)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 1, 100),
                                         Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                                         Property("direction", kPropertyTypeInteger, 1, -1, 1),
                                         Property("amount", kPropertyTypeInteger, 25, 0, 170)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               int direction = properties["direction"].value<int>();
                               int amount = properties["amount"].value<int>();
                               QueueAction(ACTION_MOONWALK, steps, speed, direction, amount);
                               return true;
                           });

        mcp_server.AddTool("self.otto.bend",
                           "弯曲身体。steps: 弯曲次数(1-100); speed: "
                           "弯曲速度(500-1500，数值越小越快); direction: 弯曲方向(1=左, -1=右)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 1, 1, 100),
                                         Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                                         Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               int direction = properties["direction"].value<int>();
                               QueueAction(ACTION_BEND, steps, speed, direction, 0);
                               return true;
                           });

        mcp_server.AddTool("self.otto.shake_leg",
                           "摇腿。steps: 摇腿次数(1-100); speed: 摇腿速度(500-1500，数值越小越快); "
                           "direction: 腿部选择(1=左腿, -1=右腿)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 1, 1, 100),
                                         Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                                         Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               int direction = properties["direction"].value<int>();
                               QueueAction(ACTION_SHAKE_LEG, steps, speed, direction, 0);
                               return true;
                           });

        mcp_server.AddTool("self.otto.sit",
                           "坐下。不需要参数",
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               QueueAction(ACTION_SIT, 1, 0, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.otto.showcase",
                           "展示动作。串联执行多个动作：往前走3步、挥挥手、跳舞（广播体操）、太空步、摇摆、起飞、健身、往后走3步。不需要参数",
                           PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               QueueAction(ACTION_SHOWCASE, 1, 0, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.otto.updown",
                           "上下运动。steps: 上下运动次数(1-100); speed: "
                           "运动速度(500-1500，数值越小越快); amount: 运动幅度(0-170度)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 1, 100),
                                         Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                                         Property("amount", kPropertyTypeInteger, 20, 0, 170)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               int amount = properties["amount"].value<int>();
                               QueueAction(ACTION_UPDOWN, steps, speed, 0, amount);
                               return true;
                           });

        mcp_server.AddTool("self.otto.whirlwind_leg",
                           "旋风腿。"
                           "steps: 动作次数(3-100); speed: 动作速度(100-1000，数值越小越快，建议300); "
                           "amplitude: 踢腿幅度(20-40度)",
                           PropertyList({Property("steps", kPropertyTypeInteger, 3, 3, 100),
                                         Property("speed", kPropertyTypeInteger, 300, 100, 1000),
                                         Property("amplitude", kPropertyTypeInteger, 30, 20, 40)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               int amplitude = properties["amplitude"].value<int>();
                               QueueAction(ACTION_WHIRLWIND_LEG, steps, speed, 0, amplitude);
                               return true;
                           });

        // 手部动作（仅在有手部舵机时可用）
        if (has_hands_) {
            mcp_server.AddTool(
                "self.otto.hands_up",
                "举手。speed: 举手速度(500-1500，数值越小越快); direction: 手部选择(1=左手, "
                "-1=右手, 0=双手)",
                PropertyList({Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                              Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int speed = properties["speed"].value<int>();
                    int direction = properties["direction"].value<int>();
                    QueueAction(ACTION_HANDS_UP, 1, speed, direction, 0);
                    return true;
                });

            mcp_server.AddTool(
                "self.otto.hands_down",
                "放手。speed: 放手速度(500-1500，数值越小越快); direction: 手部选择(1=左手, "
                "-1=右手, 0=双手)",
                PropertyList({Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                              Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int speed = properties["speed"].value<int>();
                    int direction = properties["direction"].value<int>();
                    QueueAction(ACTION_HANDS_DOWN, 1, speed, direction, 0);
                    return true;
                });

            mcp_server.AddTool(
                "self.otto.hand_wave",
                "挥手。direction: 手部选择(1=左手,-1=右手,0=双手)",
                PropertyList({Property("direction", kPropertyTypeInteger, 1, -1, 1)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int direction = properties["direction"].value<int>();   
                    QueueAction(ACTION_HAND_WAVE, 1, 0, 0, direction);
                    return true;
                });   

            mcp_server.AddTool(
                "self.otto.windmill",
                "大风车。steps: 动作次数(3-100); "
                "speed: 动作周期(300-2000毫秒，数值越小越快); amplitude: 振荡幅度(50-90度)",
                PropertyList({Property("steps", kPropertyTypeInteger, 6, 3, 100),
                              Property("speed", kPropertyTypeInteger, 500, 300, 2000),
                              Property("amplitude", kPropertyTypeInteger, 70, 50, 90)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int steps = properties["steps"].value<int>();
                    int speed = properties["speed"].value<int>();
                    int amplitude = properties["amplitude"].value<int>();
                    QueueAction(ACTION_WINDMILL, steps, speed, 0, amplitude);
                    return true;
                });

            mcp_server.AddTool(
                "self.otto.takeoff",
                "起飞。双手在90度位置同相快速振荡，模拟起飞动作。steps: 动作次数(5-100); "
                "speed: 动作周期(200-600毫秒，数值越小越快，建议300); amplitude: 振荡幅度(20-60度)",
                PropertyList({Property("steps", kPropertyTypeInteger, 5, 5, 100),
                              Property("speed", kPropertyTypeInteger, 300, 200, 600),
                              Property("amplitude", kPropertyTypeInteger, 40, 20, 60)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int steps = properties["steps"].value<int>();
                    int speed = properties["speed"].value<int>();
                    int amplitude = properties["amplitude"].value<int>();
                    QueueAction(ACTION_TAKEOFF, steps, speed, 0, amplitude);
                    return true;
                });

            mcp_server.AddTool(
                "self.otto.fitness",
                "健身。steps: 动作次数(3-100); speed: 动作速度(500-2000毫秒，数值越小越快); amplitude: 振荡幅度(10-50度)",
                PropertyList({Property("steps", kPropertyTypeInteger, 5, 3, 100),
                              Property("speed", kPropertyTypeInteger, 1000, 500, 2000),
                              Property("amplitude", kPropertyTypeInteger, 25, 10, 50)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int steps = properties["steps"].value<int>();
                    int speed = properties["speed"].value<int>();
                    int amplitude = properties["amplitude"].value<int>();
                    QueueAction(ACTION_FITNESS, steps, speed, 0, amplitude);
                    return true;
                });

            mcp_server.AddTool(
                "self.otto.greeting",
                "打招呼。direction: 手部选择(1=左手, -1=右手); steps: 动作次数(3-100)",
                PropertyList({Property("direction", kPropertyTypeInteger, 1, -1, 1),
                              Property("steps", kPropertyTypeInteger, 5, 3, 100)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int direction = properties["direction"].value<int>();
                    int steps = properties["steps"].value<int>();
                    QueueAction(ACTION_GREETING, steps, 0, direction, 0);
                    return true;
                });

            mcp_server.AddTool(
                "self.otto.shy",
                "害羞。direction: 方向(1=左, -1=右); steps: 动作次数(3-100)",
                PropertyList({Property("direction", kPropertyTypeInteger, 1, -1, 1),
                              Property("steps", kPropertyTypeInteger, 5, 3, 100)}),
                [this](const PropertyList& properties) -> ReturnValue {
                    int direction = properties["direction"].value<int>();
                    int steps = properties["steps"].value<int>();
                    QueueAction(ACTION_SHY, steps, 0, direction, 0);
                    return true;
                });

            mcp_server.AddTool("self.otto.radio_calisthenics",
                               "广播体操。不需要参数",
                               PropertyList(),
                               [this](const PropertyList& properties) -> ReturnValue {
                                   QueueAction(ACTION_RADIO_CALISTHENICS, 1, 0, 0, 0);
                                   return true;
                               });

            mcp_server.AddTool("self.otto.magic_circle",
                               "爱的魔力转圈圈。不需要参数",
                               PropertyList(),
                               [this](const PropertyList& properties) -> ReturnValue {
                                   QueueAction(ACTION_MAGIC_CIRCLE, 1, 0, 0, 0);
                                   return true;
                               });
        }

        // 舵机序列工具（支持分段发送，每次发送一个序列，自动排队执行）
        mcp_server.AddTool(
            "self.otto.servo_sequences",
            "控制每个舵机实现自主动作编程。支持分段发送序列：AI可以连续多次调用此工具，每次发送一个短序列，系统会自动排队按顺序执行。支持普通移动和振荡器两种模式。"
            "机器人结构：双手可上下摆动，双腿可内收外展，双脚可上下翻转。"
            "舵机说明："
            "ll(左腿)：内收外展，0度=完全外展，90度=中立，180度=完全内收；"
            "rl(右腿)：内收外展，0度=完全内收，90度=中立，180度=完全外展；"
            "lf(左脚)：上下翻转，0度=完全向上，90度=水平，180度=完全向下；"
            "rf(右脚)：上下翻转，0度=完全向下，90度=水平，180度=完全向上；"
            "lh(左手)：上下摆动，0度=完全向下，90度=水平，180度=完全向上；"
            "rh(右手)：上下摆动，0度=完全向上，90度=水平，180度=完全向下；"
            "sequence: 单个序列对象，包含'a'动作数组，顶层可选参数："
            "'d'(序列执行完成后延迟毫秒数，用于序列之间的停顿)。"
            "每个动作对象包含："
            "普通模式：'s'舵机位置对象(键名：ll/rl/lf/rf/lh/rh，值：0-180度)，'v'移动速度100-3000毫秒(默认1000)，'d'动作后延迟毫秒数(默认0)；"
            "振荡模式：'osc'振荡器对象，包含'a'振幅对象(各舵机振幅10-90度，默认20度)，'o'中心角度对象(各舵机振荡中心绝对角度0-180度，默认90度)，'ph'相位差对象(各舵机相位差，度，0-360度，默认0度)，'p'周期100-3000毫秒(默认500)，'c'周期数0.1-20.0(默认5.0)；"
            "使用方式：AI可以连续多次调用此工具，每次发送一个序列，系统会自动排队按顺序执行。"
            "重要说明：左右腿脚震荡的时候，有一只脚必须在90度，否则会损坏机器人，如果发送多个序列（序列数>1），完成所有序列后需要复位时，AI应该最后单独调用self.otto.home工具进行复位，不要在序列中设置复位参数。"
            "示例：发送3个序列，最后调用复位："
            "第1次调用{\"sequence\":\"{\\\"a\\\":[{\\\"s\\\":{\\\"ll\\\":100},\\\"v\\\":1000}],\\\"d\\\":500}\"}，"
            "第2次调用{\"sequence\":\"{\\\"a\\\":[{\\\"s\\\":{\\\"ll\\\":90},\\\"v\\\":800}],\\\"d\\\":500}\"}，"
            "第3次调用{\"sequence\":\"{\\\"a\\\":[{\\\"s\\\":{\\\"ll\\\":80},\\\"v\\\":800}]}\"}，"
            "最后调用self.otto.home工具进行复位。",
            PropertyList({Property("sequence", kPropertyTypeString,
                                   "{\"a\":[{\"s\":{\"ll\":90,\"rl\":90},\"v\":1000}]}")}),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string sequence = properties["sequence"].value<std::string>();
                // 检查是否是JSON对象（可能是字符串格式或已解析的对象）
                // 如果sequence是JSON字符串，直接使用；如果是对象字符串，也需要使用
                QueueServoSequence(sequence.c_str());
                return true;
            });

        // 系统工具
        mcp_server.AddTool("self.otto.home", "复位机器人到初始位置", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               QueueAction(ACTION_HOME, 1, 1000, 1, 0);
                               return true;
                           });

        mcp_server.AddTool("self.otto.stop", "立即停止所有动作并复位", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               if (action_task_handle_ != nullptr) {
                                   vTaskDelete(action_task_handle_);
                                   action_task_handle_ = nullptr;
                               }
                               is_action_in_progress_ = false;
                               xQueueReset(action_queue_);

                               QueueAction(ACTION_HOME, 1, 1000, 1, 0);
                               return true;
                           });

        mcp_server.AddTool(
            "self.otto.set_trim",
            "校准单个舵机位置。设置指定舵机的微调参数以调整机器人的初始站立姿态，设置将永久保存。"
            "servo_type: 舵机类型(left_leg/right_leg/left_foot/right_foot/left_hand/right_hand); "
            "trim_value: 微调值(-50到50度)",
            PropertyList({Property("servo_type", kPropertyTypeString, "left_leg"),
                          Property("trim_value", kPropertyTypeInteger, 0, -50, 50)}),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string servo_type = properties["servo_type"].value<std::string>();
                int trim_value = properties["trim_value"].value<int>();

                ESP_LOGI(TAG, "设置舵机微调: %s = %d度", servo_type.c_str(), trim_value);

                // 获取当前所有微调值
                Settings settings("otto_trims", true);
                int left_leg = settings.GetInt("left_leg", 0);
                int right_leg = settings.GetInt("right_leg", 0);
                int left_foot = settings.GetInt("left_foot", 0);
                int right_foot = settings.GetInt("right_foot", 0);
                int left_hand = settings.GetInt("left_hand", 0);
                int right_hand = settings.GetInt("right_hand", 0);

                // 更新指定舵机的微调值
                if (servo_type == "left_leg") {
                    left_leg = trim_value;
                    settings.SetInt("left_leg", left_leg);
                } else if (servo_type == "right_leg") {
                    right_leg = trim_value;
                    settings.SetInt("right_leg", right_leg);
                } else if (servo_type == "left_foot") {
                    left_foot = trim_value;
                    settings.SetInt("left_foot", left_foot);
                } else if (servo_type == "right_foot") {
                    right_foot = trim_value;
                    settings.SetInt("right_foot", right_foot);
                } else if (servo_type == "left_hand") {
                    if (!has_hands_) {
                        return "错误：机器人没有配置手部舵机";
                    }
                    left_hand = trim_value;
                    settings.SetInt("left_hand", left_hand);
                } else if (servo_type == "right_hand") {
                    if (!has_hands_) {
                        return "错误：机器人没有配置手部舵机";
                    }
                    right_hand = trim_value;
                    settings.SetInt("right_hand", right_hand);
                } else {
                    return "错误：无效的舵机类型，请使用: left_leg, right_leg, left_foot, "
                           "right_foot, left_hand, right_hand";
                }

                otto_.SetTrims(left_leg, right_leg, left_foot, right_foot, left_hand, right_hand);

                QueueAction(ACTION_JUMP, 1, 500, 0, 0);

                return "舵机 " + servo_type + " 微调设置为 " + std::to_string(trim_value) +
                       " 度，已永久保存";
            });

        mcp_server.AddTool("self.otto.get_trims", "获取当前的舵机微调设置", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               Settings settings("otto_trims", false);

                               int left_leg = settings.GetInt("left_leg", 0);
                               int right_leg = settings.GetInt("right_leg", 0);
                               int left_foot = settings.GetInt("left_foot", 0);
                               int right_foot = settings.GetInt("right_foot", 0);
                               int left_hand = settings.GetInt("left_hand", 0);
                               int right_hand = settings.GetInt("right_hand", 0);

                               std::string result =
                                   "{\"left_leg\":" + std::to_string(left_leg) +
                                   ",\"right_leg\":" + std::to_string(right_leg) +
                                   ",\"left_foot\":" + std::to_string(left_foot) +
                                   ",\"right_foot\":" + std::to_string(right_foot) +
                                   ",\"left_hand\":" + std::to_string(left_hand) +
                                   ",\"right_hand\":" + std::to_string(right_hand) + "}";

                               ESP_LOGI(TAG, "获取微调设置: %s", result.c_str());
                               return result;
                           });

        mcp_server.AddTool("self.otto.get_status", "获取机器人状态，返回 moving 或 idle",
                           PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                               return is_action_in_progress_ ? "moving" : "idle";
                           });

        mcp_server.AddTool("self.battery.get_level", "获取机器人电池电量和充电状态", PropertyList(),
                           [](const PropertyList& properties) -> ReturnValue {
                               auto& board = Board::GetInstance();
                               int level = 0;
                               bool charging = false;
                               bool discharging = false;
                               board.GetBatteryLevel(level, charging, discharging);

                               std::string status =
                                   "{\"level\":" + std::to_string(level) +
                                   ",\"charging\":" + (charging ? "true" : "false") + "}";
                               return status;
                           });

        ESP_LOGI(TAG, "MCP工具注册完成");
    }

    ~OttoController() {
        if (action_task_handle_ != nullptr) {
            vTaskDelete(action_task_handle_);
            action_task_handle_ = nullptr;
        }
        vQueueDelete(action_queue_);
    }
};

static OttoController* g_otto_controller = nullptr;

void InitializeOttoController() {
    if (g_otto_controller == nullptr) {
        g_otto_controller = new OttoController();
        ESP_LOGI(TAG, "Otto控制器已初始化并注册MCP工具");
    }
}
