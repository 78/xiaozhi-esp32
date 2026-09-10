/*
    Electron Bot机器人控制器 - MCP协议版本
*/

#include <cJSON.h>
#include <esp_log.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "application.h"
#include "board.h"
#include "config.h"
#include "mcp_server.h"
#include "movements.h"
#include "sdkconfig.h"
#include "settings.h"
#include <wifi_manager.h>

#define TAG "ElectronBotController"

struct ElectronBotActionParams {
    int action_type;
    int steps;
    int speed;
    int direction;
    int amount;
    char servo_sequence_json[512];
};

class ElectronBotController {
private:
    Otto electron_bot_;
    TaskHandle_t action_task_handle_ = nullptr;
    QueueHandle_t action_queue_;
    bool is_action_in_progress_ = false;

    enum ActionType {
        // 手部动作 1-12
        ACTION_HAND_LEFT_UP = 1,      // 举左手
        ACTION_HAND_RIGHT_UP = 2,     // 举右手
        ACTION_HAND_BOTH_UP = 3,      // 举双手
        ACTION_HAND_LEFT_DOWN = 4,    // 放左手
        ACTION_HAND_RIGHT_DOWN = 5,   // 放右手
        ACTION_HAND_BOTH_DOWN = 6,    // 放双手
        ACTION_HAND_LEFT_WAVE = 7,    // 挥左手
        ACTION_HAND_RIGHT_WAVE = 8,   // 挥右手
        ACTION_HAND_BOTH_WAVE = 9,    // 挥双手
        ACTION_HAND_LEFT_FLAP = 10,   // 拍打左手
        ACTION_HAND_RIGHT_FLAP = 11,  // 拍打右手
        ACTION_HAND_BOTH_FLAP = 12,   // 拍打双手

        // 身体动作 13-14
        ACTION_BODY_TURN_LEFT = 13,    // 左转
        ACTION_BODY_TURN_RIGHT = 14,   // 右转
        ACTION_BODY_TURN_CENTER = 15,  // 回中心

        // 头部动作 16-20
        ACTION_HEAD_UP = 16,          // 抬头
        ACTION_HEAD_DOWN = 17,        // 低头
        ACTION_HEAD_NOD_ONCE = 18,    // 点头一次
        ACTION_HEAD_CENTER = 19,      // 回中心
        ACTION_HEAD_NOD_REPEAT = 20,  // 连续点头

        // 系统动作 21
        ACTION_HOME = 21,  // 复位到初始位置
        ACTION_SERVO_MOVE = 22,
        ACTION_SERVO_SEQUENCE = 23
    };

    int ServoIndexFromName(const std::string& servo_type) {
        if (servo_type == "right_pitch" || servo_type == "rp") {
            return RIGHT_PITCH;
        }
        if (servo_type == "right_roll" || servo_type == "rr") {
            return RIGHT_ROLL;
        }
        if (servo_type == "left_pitch" || servo_type == "lp") {
            return LEFT_PITCH;
        }
        if (servo_type == "left_roll" || servo_type == "lr") {
            return LEFT_ROLL;
        }
        if (servo_type == "body" || servo_type == "b") {
            return BODY;
        }
        if (servo_type == "head" || servo_type == "h") {
            return HEAD;
        }
        return -1;
    }

    static int ClampInt(int value, int min_value, int max_value) {
        return std::max(min_value, std::min(max_value, value));
    }

    static int ServoMinAngle(int servo_index) {
        switch (servo_index) {
            case RIGHT_ROLL:
                return 100;
            case LEFT_ROLL:
                return 0;
            case BODY:
                return 30;
            case HEAD:
                return 75;
            default:
                return 0;
        }
    }

    static int ServoMaxAngle(int servo_index) {
        switch (servo_index) {
            case RIGHT_ROLL:
                return 180;
            case LEFT_ROLL:
                return 80;
            case BODY:
                return 150;
            case HEAD:
                return 105;
            default:
                return 180;
        }
    }

    static int ClampServoPosition(int servo_index, int position) {
        return ClampInt(position, ServoMinAngle(servo_index), ServoMaxAngle(servo_index));
    }

    static void ClampOscillationRange(int amplitude[], int center_angle[]) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            center_angle[i] = ClampServoPosition(i, center_angle[i]);
            int max_safe_amplitude =
                std::min(center_angle[i] - ServoMinAngle(i), ServoMaxAngle(i) - center_angle[i]);
            amplitude[i] = ClampInt(amplitude[i], 0, std::max(0, max_safe_amplitude));
        }
    }

    void ApplyServoObject(cJSON* servo_object, int values[], int min_value, int max_value,
                          bool use_servo_limits = false) {
        if (!cJSON_IsObject(servo_object)) {
            return;
        }

        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, servo_object) {
            if (!cJSON_IsNumber(item) || item->string == nullptr) {
                continue;
            }

            int servo_index = ServoIndexFromName(item->string);
            if (servo_index >= 0) {
                int value = ClampInt(item->valueint, min_value, max_value);
                values[servo_index] = use_servo_limits ? ClampServoPosition(servo_index, value) : value;
            }
        }
    }

    void ExecuteServoSequence(const char* sequence_json) {
        cJSON* json = cJSON_Parse(sequence_json);
        if (json == nullptr) {
            ESP_LOGE(TAG, "Failed to parse servo sequence JSON");
            return;
        }

        cJSON* actions = cJSON_GetObjectItem(json, "a");
        if (!cJSON_IsArray(actions)) {
            ESP_LOGE(TAG, "Servo sequence requires array field 'a'");
            cJSON_Delete(json);
            return;
        }

        int current_positions[SERVO_COUNT];
        electron_bot_.GetServoPositions(current_positions);

        int action_count = cJSON_GetArraySize(actions);
        for (int i = 0; i < action_count; i++) {
            cJSON* action_item = cJSON_GetArrayItem(actions, i);
            if (!cJSON_IsObject(action_item)) {
                continue;
            }

            cJSON* osc_item = cJSON_GetObjectItem(action_item, "osc");
            if (cJSON_IsObject(osc_item)) {
                int amplitude[SERVO_COUNT] = {};
                int center_angle[SERVO_COUNT];
                double phase_diff[SERVO_COUNT] = {};

                for (int j = 0; j < SERVO_COUNT; j++) {
                    center_angle[j] = current_positions[j];
                }

                ApplyServoObject(cJSON_GetObjectItem(osc_item, "a"), amplitude, 0, 90);
                ApplyServoObject(cJSON_GetObjectItem(osc_item, "o"), center_angle, 0, 180, true);
                ClampOscillationRange(amplitude, center_angle);

                cJSON* phase_item = cJSON_GetObjectItem(osc_item, "ph");
                if (cJSON_IsObject(phase_item)) {
                    cJSON* item = nullptr;
                    cJSON_ArrayForEach(item, phase_item) {
                        if (!cJSON_IsNumber(item) || item->string == nullptr) {
                            continue;
                        }

                        int servo_index = ServoIndexFromName(item->string);
                        if (servo_index >= 0) {
                            phase_diff[servo_index] = item->valuedouble * M_PI / 180.0;
                        }
                    }
                }

                int period = 500;
                cJSON* period_item = cJSON_GetObjectItem(osc_item, "p");
                if (cJSON_IsNumber(period_item)) {
                    period = ClampInt(period_item->valueint, 100, 3000);
                }

                float cycles = 5.0f;
                cJSON* cycles_item = cJSON_GetObjectItem(osc_item, "c");
                if (cJSON_IsNumber(cycles_item)) {
                    cycles =
                        std::max(0.1f, std::min(20.0f, static_cast<float>(cycles_item->valuedouble)));
                }

                electron_bot_.OscillateServos(amplitude, center_angle, period, phase_diff, cycles);
                for (int j = 0; j < SERVO_COUNT; j++) {
                    current_positions[j] = center_angle[j];
                }
            } else {
                int servo_target[SERVO_COUNT];
                for (int j = 0; j < SERVO_COUNT; j++) {
                    servo_target[j] = current_positions[j];
                }

                ApplyServoObject(cJSON_GetObjectItem(action_item, "s"), servo_target, 0, 180, true);

                int speed = 1000;
                cJSON* speed_item = cJSON_GetObjectItem(action_item, "v");
                if (cJSON_IsNumber(speed_item)) {
                    speed = ClampInt(speed_item->valueint, 100, 3000);
                }

                electron_bot_.MoveServos(speed, servo_target);
                for (int j = 0; j < SERVO_COUNT; j++) {
                    current_positions[j] = servo_target[j];
                }
            }

            int delay_after = 0;
            cJSON* delay_item = cJSON_GetObjectItem(action_item, "d");
            if (cJSON_IsNumber(delay_item)) {
                delay_after = std::max(0, delay_item->valueint);
            }
            if (delay_after > 0 && i < action_count - 1) {
                vTaskDelay(pdMS_TO_TICKS(delay_after));
            }
        }

        cJSON* sequence_delay_item = cJSON_GetObjectItem(json, "d");
        if (cJSON_IsNumber(sequence_delay_item)) {
            int sequence_delay = std::max(0, sequence_delay_item->valueint);
            if (sequence_delay > 0 && uxQueueMessagesWaiting(action_queue_) > 0) {
                vTaskDelay(pdMS_TO_TICKS(sequence_delay));
            }
        }

        cJSON_Delete(json);
    }

    static void ActionTask(void* arg) {
        ElectronBotController* controller = static_cast<ElectronBotController*>(arg);
        ElectronBotActionParams params;
        controller->electron_bot_.AttachServos();

        while (true) {
            if (xQueueReceive(controller->action_queue_, &params, pdMS_TO_TICKS(1000)) == pdTRUE) {
                ESP_LOGI(TAG, "执行动作: %d", params.action_type);
                controller->is_action_in_progress_ = true;  // 开始执行动作

                // 执行相应的动作
                if (params.action_type >= ACTION_HAND_LEFT_UP &&
                    params.action_type <= ACTION_HAND_BOTH_FLAP) {
                    // 手部动作
                    controller->electron_bot_.HandAction(params.action_type, params.steps,
                                                         params.amount, params.speed);
                } else if (params.action_type >= ACTION_BODY_TURN_LEFT &&
                           params.action_type <= ACTION_BODY_TURN_CENTER) {
                    // 身体动作
                    int body_direction = params.action_type - ACTION_BODY_TURN_LEFT + 1;
                    controller->electron_bot_.BodyAction(body_direction, params.steps,
                                                         params.amount, params.speed);
                } else if (params.action_type >= ACTION_HEAD_UP &&
                           params.action_type <= ACTION_HEAD_NOD_REPEAT) {
                    // 头部动作
                    int head_action = params.action_type - ACTION_HEAD_UP + 1;
                    controller->electron_bot_.HeadAction(head_action, params.steps, params.amount,
                                                         params.speed);
                } else if (params.action_type == ACTION_HOME) {
                    // 复位动作
                    controller->electron_bot_.Home(true);
                } else if (params.action_type == ACTION_SERVO_MOVE) {
                    int servo_target[SERVO_COUNT];
                    controller->electron_bot_.GetServoPositions(servo_target);
                    if (params.direction >= 0 && params.direction < SERVO_COUNT) {
                        servo_target[params.direction] =
                            ClampServoPosition(params.direction, params.amount);
                        controller->electron_bot_.MoveServos(ClampInt(params.speed, 100, 3000),
                                                             servo_target);
                    }
                } else if (params.action_type == ACTION_SERVO_SEQUENCE) {
                    controller->ExecuteServoSequence(params.servo_sequence_json);
                }
                controller->is_action_in_progress_ = false;  // 动作执行完毕
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    void QueueAction(int action_type, int steps, int speed, int direction, int amount) {
        ESP_LOGI(TAG, "动作控制: 类型=%d, 步数=%d, 速度=%d, 方向=%d, 幅度=%d", action_type, steps,
                 speed, direction, amount);

        ElectronBotActionParams params = {};
        params.action_type = action_type;
        params.steps = steps;
        params.speed = speed;
        params.direction = direction;
        params.amount = amount;
        xQueueSend(action_queue_, &params, portMAX_DELAY);
        StartActionTaskIfNeeded();
    }

    void QueueServoSequence(const char* servo_sequence_json) {
        if (servo_sequence_json == nullptr || strlen(servo_sequence_json) == 0) {
            ESP_LOGE(TAG, "Empty servo sequence");
            return;
        }

        ElectronBotActionParams params = {};
        params.action_type = ACTION_SERVO_SEQUENCE;
        strncpy(params.servo_sequence_json, servo_sequence_json,
                sizeof(params.servo_sequence_json) - 1);
        params.servo_sequence_json[sizeof(params.servo_sequence_json) - 1] = '\0';
        xQueueSend(action_queue_, &params, portMAX_DELAY);
        StartActionTaskIfNeeded();
    }

    void StartActionTaskIfNeeded() {
        if (action_task_handle_ == nullptr) {
            xTaskCreate(ActionTask, "electron_bot_action", 1024 * 4, this, configMAX_PRIORITIES - 1,
                        &action_task_handle_);
        }
    }

    void LoadTrimsFromNVS() {
        Settings settings("electron_trims", false);

        int right_pitch = settings.GetInt("right_pitch", 0);
        int right_roll = settings.GetInt("right_roll", 0);
        int left_pitch = settings.GetInt("left_pitch", 0);
        int left_roll = settings.GetInt("left_roll", 0);
        int body = settings.GetInt("body", 0);
        int head = settings.GetInt("head", 0);
        electron_bot_.SetTrims(right_pitch, right_roll, left_pitch, left_roll, body, head);
    }

public:
    ElectronBotController() {
        electron_bot_.Init(Right_Pitch_Pin, Right_Roll_Pin, Left_Pitch_Pin, Left_Roll_Pin, Body_Pin,
                           Head_Pin);

        LoadTrimsFromNVS();
        action_queue_ = xQueueCreate(10, sizeof(ElectronBotActionParams));

        QueueAction(ACTION_HOME, 1, 1000, 0, 0);

        RegisterMcpTools();
        ESP_LOGI(TAG, "Electron Bot控制器已初始化并注册MCP工具");
    }

    void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();

        ESP_LOGI(TAG, "开始注册Electron Bot MCP工具...");

        // 手部动作统一工具
        mcp_server.AddTool(
            "self.electron.hand_action",
            "手部动作控制。action: 1=举手, 2=放手, 3=挥手, 4=拍打; hand: 1=左手, 2=右手, 3=双手; "
            "steps: 动作重复次数(1-10); speed: 动作速度(500-1500，数值越小越快); amount: "
            "拍打幅度(10-50，固件会按安全范围裁剪)",
            PropertyList({Property("action", kPropertyTypeInteger, 1, 1, 4),
                          Property("hand", kPropertyTypeInteger, 3, 1, 3),
                          Property("steps", kPropertyTypeInteger, 1, 1, 10),
                          Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                          Property("amount", kPropertyTypeInteger, 30, 10, 50)}),
            [this](const PropertyList& properties) -> ReturnValue {
                int action_type = properties["action"].value<int>();
                int hand_type = properties["hand"].value<int>();
                int steps = properties["steps"].value<int>();
                int speed = properties["speed"].value<int>();
                int amount = properties["amount"].value<int>();

                // 根据动作类型和手部类型计算具体动作
                int base_action;
                switch (action_type) {
                    case 1:
                        base_action = ACTION_HAND_LEFT_UP;
                        break;  // 举手
                    case 2:
                        base_action = ACTION_HAND_LEFT_DOWN;
                        amount = 0;
                        break;  // 放手
                    case 3:
                        base_action = ACTION_HAND_LEFT_WAVE;
                        amount = 0;
                        break;  // 挥手
                    case 4:
                        base_action = ACTION_HAND_LEFT_FLAP;
                        break;  // 拍打
                    default:
                        base_action = ACTION_HAND_LEFT_UP;
                }
                int action_id = base_action + (hand_type - 1);

                QueueAction(action_id, steps, speed, 0, amount);
                return true;
            });

        // 身体动作
        mcp_server.AddTool(
            "self.electron.body_turn",
            "身体转向。steps: 转向步数(1-10); speed: 转向速度(500-1500，数值越小越快); direction: "
            "转向方向(1=左转, 2=右转, 3=回中心); angle: 转向角度(0-90度)",
            PropertyList({Property("steps", kPropertyTypeInteger, 1, 1, 10),
                          Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                          Property("direction", kPropertyTypeInteger, 1, 1, 3),
                          Property("angle", kPropertyTypeInteger, 45, 0, 90)}),
            [this](const PropertyList& properties) -> ReturnValue {
                int steps = properties["steps"].value<int>();
                int speed = properties["speed"].value<int>();
                int direction = properties["direction"].value<int>();
                int amount = properties["angle"].value<int>();

                int action;
                switch (direction) {
                    case 1:
                        action = ACTION_BODY_TURN_LEFT;
                        break;
                    case 2:
                        action = ACTION_BODY_TURN_RIGHT;
                        break;
                    case 3:
                        action = ACTION_BODY_TURN_CENTER;
                        break;
                    default:
                        action = ACTION_BODY_TURN_LEFT;
                }

                QueueAction(action, steps, speed, 0, amount);
                return true;
            });

        // 头部动作
        mcp_server.AddTool("self.electron.head_move",
                           "头部运动。action: 1=抬头, 2=低头, 3=点头, 4=回中心, 5=连续点头; steps: "
                           "动作重复次数(1-10); speed: 动作速度(500-1500，数值越小越快); angle: "
                           "头部转动角度(1-15度)",
                           PropertyList({Property("action", kPropertyTypeInteger, 3, 1, 5),
                                         Property("steps", kPropertyTypeInteger, 1, 1, 10),
                                         Property("speed", kPropertyTypeInteger, 1000, 500, 1500),
                                         Property("angle", kPropertyTypeInteger, 5, 1, 15)}),
                           [this](const PropertyList& properties) -> ReturnValue {
                               int action_num = properties["action"].value<int>();
                               int steps = properties["steps"].value<int>();
                               int speed = properties["speed"].value<int>();
                               int amount = properties["angle"].value<int>();
                               int action = ACTION_HEAD_UP + (action_num - 1);
                               QueueAction(action, steps, speed, 0, amount);
                               return true;
                           });

        mcp_server.AddTool(
            "self.electron.servo_move",
            "单独调节 ElectronBot 任意舵机到指定绝对角度。servo_type 支持完整名 "
            "right_pitch/right_roll/left_pitch/left_roll/body/head，也支持短键 rp/rr/lp/lr/b/h；"
            "position 会按安全范围自动裁剪：rp/lp=0-180, rr=100-180, lr=0-80, body=30-150, head=75-105；"
            "speed 为移动时间 100-3000 毫秒。",
            PropertyList({Property("servo_type", kPropertyTypeString, "head"),
                          Property("position", kPropertyTypeInteger, 90, 0, 180),
                          Property("speed", kPropertyTypeInteger, 800, 100, 3000)}),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string servo_type = properties["servo_type"].value<std::string>();
                int servo_index = ServoIndexFromName(servo_type);
                if (servo_index < 0) {
                    return "错误：无效的舵机类型，请使用 right_pitch/right_roll/left_pitch/left_roll/body/head 或 rp/rr/lp/lr/b/h";
                }

                int position = properties["position"].value<int>();
                int speed = properties["speed"].value<int>();
                QueueAction(ACTION_SERVO_MOVE, 1, speed, servo_index, position);
                return true;
            });

        mcp_server.AddTool(
            "self.electron.servo_sequences",
            "AI 自编程动作序列。支持分段多次调用，每次发送一个短 JSON 序列并自动排队执行。"
            "舵机短键：rp=右臂pitch, rr=右臂roll, lp=左臂pitch, lr=左臂roll, b=身体旋转, h=头部。"
            "所有舵机角度都会按安全范围裁剪：rp/lp=0-180, rr=100-180, lr=0-80, b=30-150, h=75-105；"
            "振荡模式会自动限制振幅，保证中心角±振幅不越界。"
            "格式：sequence 是 JSON 字符串，顶层包含 a 动作数组，可选 d 为序列间延迟毫秒。"
            "普通动作：{\"s\":{\"rp\":120,\"h\":100},\"v\":800,\"d\":200}，s 是舵机目标角度 0-180，v 是移动时间 100-3000ms。"
            "振荡动作：{\"osc\":{\"a\":{\"rp\":20},\"o\":{\"rp\":120},\"ph\":{\"lp\":180},\"p\":500,\"c\":4}}，"
            "a 是振幅 0-90，o 是中心角度 0-180，ph 是相位角度，p 是周期，c 是周期数。"
            "示例：{\"a\":[{\"s\":{\"rp\":120,\"lp\":60},\"v\":800},{\"osc\":{\"a\":{\"rr\":25,\"lr\":25},\"o\":{\"rr\":160,\"lr\":20},\"ph\":{\"lr\":180},\"p\":400,\"c\":5}}]}",
            PropertyList({Property("sequence", kPropertyTypeString,
                                   "{\"a\":[{\"s\":{\"h\":100},\"v\":800}]}")}),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string sequence = properties["sequence"].value<std::string>();
                QueueServoSequence(sequence.c_str());
                return true;
            });

        // 系统工具
        mcp_server.AddTool("self.electron.stop", "立即停止", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               if (action_task_handle_ != nullptr) {
                                   vTaskDelete(action_task_handle_);
                                   action_task_handle_ = nullptr;
                               }
                               xQueueReset(action_queue_);
                               is_action_in_progress_ = false;
                               QueueAction(ACTION_HOME, 1, 1000, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.electron.home", "复位到 ElectronBot 初始姿态", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               QueueAction(ACTION_HOME, 1, 1000, 0, 0);
                               return true;
                           });

        mcp_server.AddTool("self.electron.get_status", "获取机器人状态，返回 moving 或 idle",
                           PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                               return is_action_in_progress_ ? "moving" : "idle";
                           });

        // 单个舵机校准工具
        mcp_server.AddTool(
            "self.electron.set_trim",
            "校准单个舵机位置。设置指定舵机的微调参数以调整ElectronBot的初始姿态，设置将永久保存。"
            "servo_type: 舵机类型(right_pitch:右臂旋转, right_roll:右臂推拉, left_pitch:左臂旋转, "
            "left_roll:左臂推拉, body:身体, head:头部); "
            "trim_value: 微调值(-30到30度)",
            PropertyList({Property("servo_type", kPropertyTypeString, "right_pitch"),
                          Property("trim_value", kPropertyTypeInteger, 0, -30, 30)}),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string servo_type = properties["servo_type"].value<std::string>();
                int trim_value = properties["trim_value"].value<int>();

                ESP_LOGI(TAG, "设置舵机微调: %s = %d度", servo_type.c_str(), trim_value);

                // 获取当前所有微调值
                Settings settings("electron_trims", true);
                int right_pitch = settings.GetInt("right_pitch", 0);
                int right_roll = settings.GetInt("right_roll", 0);
                int left_pitch = settings.GetInt("left_pitch", 0);
                int left_roll = settings.GetInt("left_roll", 0);
                int body = settings.GetInt("body", 0);
                int head = settings.GetInt("head", 0);

                // 更新指定舵机的微调值
                if (servo_type == "right_pitch") {
                    right_pitch = trim_value;
                    settings.SetInt("right_pitch", right_pitch);
                } else if (servo_type == "right_roll") {
                    right_roll = trim_value;
                    settings.SetInt("right_roll", right_roll);
                } else if (servo_type == "left_pitch") {
                    left_pitch = trim_value;
                    settings.SetInt("left_pitch", left_pitch);
                } else if (servo_type == "left_roll") {
                    left_roll = trim_value;
                    settings.SetInt("left_roll", left_roll);
                } else if (servo_type == "body") {
                    body = trim_value;
                    settings.SetInt("body", body);
                } else if (servo_type == "head") {
                    head = trim_value;
                    settings.SetInt("head", head);
                } else {
                    return "错误：无效的舵机类型，请使用: right_pitch, right_roll, left_pitch, "
                           "left_roll, body, head";
                }

                electron_bot_.SetTrims(right_pitch, right_roll, left_pitch, left_roll, body, head);

                QueueAction(ACTION_HOME, 1, 500, 0, 0);

                return "舵机 " + servo_type + " 微调设置为 " + std::to_string(trim_value) +
                       " 度，已永久保存";
            });

        mcp_server.AddTool("self.electron.get_trims", "获取当前的舵机微调设置", PropertyList(),
                           [this](const PropertyList& properties) -> ReturnValue {
                               Settings settings("electron_trims", false);

                               int right_pitch = settings.GetInt("right_pitch", 0);
                               int right_roll = settings.GetInt("right_roll", 0);
                               int left_pitch = settings.GetInt("left_pitch", 0);
                               int left_roll = settings.GetInt("left_roll", 0);
                               int body = settings.GetInt("body", 0);
                               int head = settings.GetInt("head", 0);

                               std::string result =
                                   "{\"right_pitch\":" + std::to_string(right_pitch) +
                                   ",\"right_roll\":" + std::to_string(right_roll) +
                                   ",\"left_pitch\":" + std::to_string(left_pitch) +
                                   ",\"left_roll\":" + std::to_string(left_roll) +
                                   ",\"body\":" + std::to_string(body) +
                                   ",\"head\":" + std::to_string(head) + "}";

                               ESP_LOGI(TAG, "获取微调设置: %s", result.c_str());
                               return result;
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

        mcp_server.AddTool("self.electron.get_ip", "获取 ElectronBot WiFi IP 地址", PropertyList(),
                           [](const PropertyList& properties) -> ReturnValue {
                               auto& wifi = WifiManager::GetInstance();
                               std::string ip = wifi.GetIpAddress();
                               if (ip.empty()) {
                                   return "{\"ip\":\"\",\"connected\":false}";
                               }
                               return "{\"ip\":\"" + ip + "\",\"connected\":true}";
                           });

        ESP_LOGI(TAG, "Electron Bot MCP工具注册完成");
    }

    ~ElectronBotController() {
        if (action_task_handle_ != nullptr) {
            vTaskDelete(action_task_handle_);
            action_task_handle_ = nullptr;
        }
        vQueueDelete(action_queue_);
    }
};

static ElectronBotController* g_electron_controller = nullptr;

void InitializeElectronBotController() {
    if (g_electron_controller == nullptr) {
        g_electron_controller = new ElectronBotController();
        ESP_LOGI(TAG, "Electron Bot控制器已初始化并注册MCP工具");
    }
}
