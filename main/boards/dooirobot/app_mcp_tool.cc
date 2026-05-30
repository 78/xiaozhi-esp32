#include "app_mcp_tool.h"
#include "mcp_server.h"
#include "app_action_seq.h"
#include "app_ui.h"
#include "app_servo.h"
#include "app_motor.h"
#include <string>
#include <map>
#include <vector>
#include <cJSON.h>
#include <cstring>
#include "application.h"
// ==================== 辅助函数：解析颜色 ====================

static uint32_t parse_color(const char* color_str) {
    if (!color_str) return 0;
    
    if (color_str[0] == '#') {
        return (uint32_t)strtol(color_str + 1, nullptr, 16);
    } else if (color_str[0] == '0' && (color_str[1] == 'x' || color_str[1] == 'X')) {
        return (uint32_t)strtol(color_str, nullptr, 16);
    }
    return (uint32_t)strtol(color_str, nullptr, 16);
}

// ==================== 辅助函数：解析自定义动作序列 ====================

static bool parse_custom_sequence(const cJSON* actions_json, 
                                  std::vector<action_cmd_t>& out_actions) {
    if (!cJSON_IsArray(actions_json)) {
        return false;
    }
    
    int action_count = cJSON_GetArraySize(actions_json);
    out_actions.reserve(action_count);
    
    for (int i = 0; i < action_count; i++) {
        cJSON* action = cJSON_GetArrayItem(actions_json, i);
        if (!action) continue;
        
        cJSON* type_json = cJSON_GetObjectItem(action, "type");
        if (!type_json || !cJSON_IsString(type_json)) continue;
        
        std::string type = type_json->valuestring;
        action_cmd_t cmd = {};
        
        if (type == "expression") {
            cJSON* emotion = cJSON_GetObjectItem(action, "emotion");
            if (emotion && cJSON_IsNumber(emotion)) {
                cmd.type = ACTION_EXPRESSION;
                cmd.params.expression.emotion_id = emotion->valueint;
                out_actions.push_back(cmd);
            }
        }
        else if (type == "eye_move") {
            cJSON* x = cJSON_GetObjectItem(action, "x");
            cJSON* y = cJSON_GetObjectItem(action, "y");
            cJSON* duration = cJSON_GetObjectItem(action, "duration");
            if (x && y && cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
                cmd.type = ACTION_EYE_MOVE;
                cmd.params.eye_move.x = (float)x->valuedouble;
                cmd.params.eye_move.y = (float)y->valuedouble;
                cmd.params.eye_move.duration = duration && cJSON_IsNumber(duration) ? 
                                               duration->valueint : 300;
                out_actions.push_back(cmd);
            }
        }
        else if (type == "led_set") {
            cJSON* left = cJSON_GetObjectItem(action, "left_color");
            cJSON* right = cJSON_GetObjectItem(action, "right_color");
            if (left && right && cJSON_IsString(left) && cJSON_IsString(right)) {
                cmd.type = ACTION_LED_SET;
                cmd.params.led_set.left_color = parse_color(left->valuestring);
                cmd.params.led_set.right_color = parse_color(right->valuestring);
                out_actions.push_back(cmd);
            }
        }
        else if (type == "led_breathe") {
            cJSON* color = cJSON_GetObjectItem(action, "color");
            cJSON* period = cJSON_GetObjectItem(action, "period");
            cJSON* symmetric = cJSON_GetObjectItem(action, "symmetric");
            if (color && cJSON_IsString(color)) {
                cmd.type = ACTION_LED_BREATHE;
                cmd.params.led_breathe.color = parse_color(color->valuestring);
                cmd.params.led_breathe.period = period && cJSON_IsNumber(period) ? 
                                                period->valueint : 1000;
                cmd.params.led_breathe.symmetric = symmetric && cJSON_IsBool(symmetric) ? 
                                                   cJSON_IsTrue(symmetric) : true;
                out_actions.push_back(cmd);
            }
        }
        else if (type == "led_blink") {
            cJSON* color = cJSON_GetObjectItem(action, "color");
            cJSON* period = cJSON_GetObjectItem(action, "period");
            cJSON* symmetric = cJSON_GetObjectItem(action, "symmetric");
            if (color && cJSON_IsString(color)) {
                cmd.type = ACTION_LED_BLINK;
                cmd.params.led_blink.color = parse_color(color->valuestring);
                cmd.params.led_blink.period_ms = period && cJSON_IsNumber(period) ? 
                                                 period->valueint : 500;
                cmd.params.led_blink.symmetric = symmetric && cJSON_IsBool(symmetric) ? 
                                                 cJSON_IsTrue(symmetric) : false;
                out_actions.push_back(cmd);
            }
        }
        else if (type == "led_rainbow") {
            cJSON* period = cJSON_GetObjectItem(action, "period");
            cJSON* symmetric = cJSON_GetObjectItem(action, "symmetric");
            cmd.type = ACTION_LED_RAINBOW;
            cmd.params.led_rainbow.period_ms = period && cJSON_IsNumber(period) ? 
                                               period->valueint : 1000;
            cmd.params.led_rainbow.symmetric = symmetric && cJSON_IsBool(symmetric) ? 
                                               cJSON_IsTrue(symmetric) : true;
            out_actions.push_back(cmd);
        }
        else if (type == "led_off") {
            cmd.type = ACTION_LED_OFF;
            out_actions.push_back(cmd);
        }
        else if (type == "servo") {
            cJSON* servo_id = cJSON_GetObjectItem(action, "servo_id");
            cJSON* angle = cJSON_GetObjectItem(action, "angle");
            if (servo_id && angle && cJSON_IsNumber(servo_id) && cJSON_IsNumber(angle)) {
                cmd.type = ACTION_SERVO_MOVE;
                cmd.params.servo_move.servo_id = servo_id->valueint;
                cmd.params.servo_move.angle = angle->valueint;
                cmd.params.servo_move.duration_ms = 1000;
                out_actions.push_back(cmd);
            }
        }
        else if (type == "motor") {
            cJSON* left_speed = cJSON_GetObjectItem(action, "left_speed");
            cJSON* right_speed = cJSON_GetObjectItem(action, "right_speed");
            cJSON* duration = cJSON_GetObjectItem(action, "duration");
            if (left_speed && right_speed && cJSON_IsNumber(left_speed) && cJSON_IsNumber(right_speed)) {
                cmd.type = ACTION_MOTOR_CONTROL;
                cmd.params.motor_control.left_speed = left_speed->valueint;
                cmd.params.motor_control.right_speed = right_speed->valueint;
                cmd.params.motor_control.duration = duration && cJSON_IsNumber(duration) ? 
                                                    duration->valueint : 0;
                out_actions.push_back(cmd);
            }
        }
        else if (type == "delay") {
            cJSON* duration = cJSON_GetObjectItem(action, "duration");
            if (duration && cJSON_IsNumber(duration)) {
                cmd.type = ACTION_DELAY;
                cmd.params.delay.duration = duration->valueint;
                out_actions.push_back(cmd);
            }
        }
        else if (type == "turn_angle") {
            cJSON* angle = cJSON_GetObjectItem(action, "angle");
            cJSON* speed = cJSON_GetObjectItem(action, "speed");
            if (angle && cJSON_IsNumber(angle)) {
                float target_angle = (float)angle->valuedouble;
                int16_t max_speed = speed && cJSON_IsNumber(speed) ? (int16_t)speed->valueint : 100;
                app_motor_turn_angle(target_angle, max_speed);

                // do nothing 
                cmd.type = ACTION_EYE_MOVE;
                out_actions.push_back(cmd);
            }
        }
    }
    
    return !out_actions.empty();
}

// ==================== MCP工具初始化 ====================

void app_mcp_tool_register(void) {
    McpServer& mcp = McpServer::GetInstance();

    // ==================== 自定义动作序列 ====================
    mcp.AddTool(
        "action.custom",
        "可以根据情境自由组合各种动作元素，调用身体的硬件，如前进、开灯等。"
        "\n\n【重要】actions参数必须是JSON字符串格式，不是直接的数组对象！"
        "\n\n【机器人结构说明】"
        "\n- 双臂：每个手臂可上下挥动，0度=前方水平，90度=垂直向上，180度=向后收回"
        "\n- 双手RGB灯：左右手各有一个可编程RGB灯珠"
        "\n- 履带：双电机独立控制，可前进、后退、原地旋转"
        "\n- 眼睛：可显示多种表情，眼球可移动"
        "\n\n【动作类型详解】"
        "\n1. expression - 表情切换"
        "\n   参数：emotion (整数，表情ID)"
        "\n\n2. eye_move - 眼球移动"
        "\n   参数：x (-1.0到1.0，左右)，y (-1.0到1.0，上下)，duration (毫秒，默认1000)"
        "\n\n3. led_set - LED直接设色"
        "\n   参数：left_color (左手颜色，如\"#FF0000\")，right_color (右手颜色)"
        "\n\n4. led_breathe - LED呼吸效果"
        "\n   参数：color (颜色)，period (周期毫秒，默认5000)，symmetric (是否对称，默认true)"
        "\n\n5. led_blink - LED闪烁效果"
        "\n   参数：color (颜色)，period (周期毫秒，默认1000)，symmetric (是否对称，默认true)"
        "\n\n6. led_rainbow - LED彩虹效果"
        "\n   参数：period (周期毫秒，默认10000)，symmetric (是否对称，默认true)"
        "\n\n7. led_off - 关闭LED"
        "\n   无参数"
        "\n\n8. servo - 舵机控制"
        "\n   参数：servo_id (舵机ID：0-2,0左边，1右边，2一起)，angle (角度0-180)"
        "\n\n9. motor - 电机控制"
        "\n   参数：left_speed (左电机速度，默认：100)，right_speed (右电机速度，默认：100)，duration (持续时间毫秒)"
        "\n\n10. delay - 延时等待"
        "\n    参数：duration (毫秒)"
        "\n\n【使用示例 - 注意actions是字符串！】"
        "\n\n11. turn_angle - 电机旋转指定角度"
        "\n    参数：angle (旋转角度，正=右转，负=左转，范围-180~180)，speed (速度1~100，默认100)"
        "\n示例1 - 害羞表情："
        "\n{\"actions\":\"[{\\\"type\\\":\\\"expression\\\",\\\"emotion\\\":5},{\\\"type\\\":\\\"led_set\\\",\\\"left_color\\\":\\\"#FFC0CB\\\",\\\"right_color\\\":\\\"#FFC0CB\\\"},{\\\"type\\\":\\\"servo\\\",\\\"servo_id\\\":0,\\\"angle\\\":45},{\\\"type\\\":\\\"servo\\\",\\\"servo_id\\\":1,\\\"angle\\\":45},{\\\"type\\\":\\\"eye_move\\\",\\\"x\\\":0.5,\\\"y\\\":-0.3,\\\"duration\\\":400}]\"}"
        "\n\n示例2 - 兴奋跳跃："
        "\n{\"actions\":\"[{\\\"type\\\":\\\"led_rainbow\\\",\\\"period\\\":10000},{\\\"type\\\":\\\"servo\\\",\\\"servo_id\\\":2,\\\"angle\\\":120},{\\\"type\\\":\\\"servo\\\",\\\"servo_id\\\":2,\\\"angle\\\":120},{\\\"type\\\":\\\"delay\\\",\\\"duration\\\":200},{\\\"type\\\":\\\"servo\\\",\\\"servo_id\\\":2,\\\"angle\\\":60},{\\\"type\\\":\\\"servo\\\",\\\"servo_id\\\":2,\\\"angle\\\":60}]\"}"
        "\n\n示例3 - 转身打招呼："
        "\n{\"actions\":\"[{\\\"type\\\":\\\"turn_angle\\\",\\\"angle\\\":90,\\\"speed\\\":50},{\\\"type\\\":\\\"delay\\\",\\\"duration\\\":500},{\\\"type\\\":\\\"expression\\\",\\\"emotion\\\":5}]\"}"
        "\n\n【创作建议】"
        "\n- 动作要流畅：使用适当的delay让动作有节奏感"
        "\n- 灯光配合：用颜色强化情感表达（红=愤怒，蓝=悲伤，绿=平静等）"
        "\n- 眼神交流：眼球移动让机器人更有灵性"
        "\n- 肢体语言：手臂角度传达不同态度（上举=兴奋，下垂=沮丧）"
        "\n- 运动配合：适当的前进后退让动作更生动",
        PropertyList({
            Property("actions", kPropertyTypeString, "JSON字符串格式的动作数组"),
            Property("priority", kPropertyTypeInteger, PRIO_NORMAL, PRIO_IDLE, PRIO_CRITICAL)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            std::string actions_str = properties["actions"].value<std::string>();
            int priority = properties["priority"].value<int>();

            printf("actions_str: %s\n", actions_str.c_str());
            
            cJSON* root = cJSON_Parse(actions_str.c_str());
            if (!root) {
                const char* error_ptr = cJSON_GetErrorPtr();
                std::string error_msg = "错误：无效的JSON格式";
                if (error_ptr) {
                    error_msg += "，错误位置: " + std::string(error_ptr);
                }
                printf("error_msg: %s\n", error_msg.c_str());
                return error_msg;
            }
            
            std::vector<action_cmd_t> actions;
            bool parse_ok = parse_custom_sequence(root, actions);
            cJSON_Delete(root);
            
            if (!parse_ok || actions.empty()) {
                printf("error_msg: %s\n", "错误：无效的动作数组");
                return "fail";
            }

            uint16_t action_count = actions.size();
            const action_cmd_t* prepared = app_action_engine_prepare_custom(
                actions.data(), 
                actions.size()
            );
            
            bool success = app_action_engine_play_custom(
                prepared,
                action_count,
                static_cast<action_priority_t>(priority),
                nullptr
            );
            
            if (success) {
                return "调用成功";
            } else {
                printf("error_msg: %s\n", "错误：动作执行失败");
                return "调用失败";
            }
        }
    );

}