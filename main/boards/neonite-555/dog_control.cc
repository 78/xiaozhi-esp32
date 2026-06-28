/*
 * dog_control.cc - 机器狗控制模块
 *
 * 基于 weixintest2 的 servo.c 完美步态实现
 * 移植到小智 NEONITE-555 板子
 *
 * GPIO 映射（与 weixintest2 完全一致）：
 *   - 右前腿 FR : GPIO9  / LEDC_CHANNEL_0
 *   - 右后腿 BR : GPIO10 / LEDC_CHANNEL_1
 *   - 左后腿 BL : GPIO21 / LEDC_CHANNEL_2
 *   - 左前腿 FL : GPIO47 / LEDC_CHANNEL_3
 *
 * 限位保护：45° ~ 135°，由 servo.c 的 leg_set_angle() 自动保护
 */

#include "dog_control.h"
#include "servo.h"       // weixintest2 的舵机驱动
#include "choreo.h"       // 舞蹈编排模块
#include "mcp_server.h"
#include "config.h"
#include "application.h"
#include "board.h"
#include "display.h"
#include "wifi_board.h"
#include <wifi_manager.h>
#include <esp_log.h>
#include <stdlib.h>        // rand, srand
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#define TAG "dog"

// ================================================================
// 舵机初始化（调用 weixintest2 的 servo_init）
// ================================================================
void Dog_ServoInit(void) {
    esp_err_t ret = servo_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "舵机初始化成功");
    } else {
        ESP_LOGE(TAG, "舵机初始化失败: %d", ret);
    }
}

// ================================================================
// 单腿角度控制（映射到 servo.h 的 API）
// ================================================================
void Leg1_SetAngle(int angle) {
    servo_set_angle_fr((uint32_t)angle);
}

void Leg2_SetAngle(int angle) {
    servo_set_angle_br((uint32_t)angle);
}

void Leg3_SetAngle(int angle) {
    servo_set_angle_bl((uint32_t)angle);
}

void Leg4_SetAngle(int angle) {
    servo_set_angle_fl((uint32_t)angle);
}

// ================================================================
// 全部归位 90°
// ================================================================
void Dog_ResetAll(void) {
    servo_reset_all_smooth();
    ESP_LOGI(TAG, "全部平滑归位 90°");
}

// ================================================================
// 参数化步态封装
// ================================================================

// 前进 N 步
void Dog_ForwardSteps(int steps) {
    if (steps < 1) steps = 1;
    if (steps > 10) steps = 10;

    ESP_LOGI(TAG, "开始前进 %d 步", steps);
    for (int i = 0; i < steps; i++) {
        servo_one_step();
    }
    servo_reset_all_smooth();
    ESP_LOGI(TAG, "前进完成，已复位");
}

// 后退 N 步
void Dog_BackwardSteps(int steps) {
    if (steps < 1) steps = 1;
    if (steps > 10) steps = 10;

    ESP_LOGI(TAG, "开始后退 %d 步", steps);
    for (int i = 0; i < steps; i++) {
        servo_one_step_back();
    }
    servo_reset_all_smooth();
    ESP_LOGI(TAG, "后退完成，已复位");
}

// 左转 N 步
void Dog_TurnLeftSteps(int steps) {
    if (steps < 1) steps = 1;
    if (steps > 10) steps = 10;

    ESP_LOGI(TAG, "开始左转 %d 步", steps);
    for (int i = 0; i < steps; i++) {
        servo_one_step_turn_left();
    }
    servo_reset_all_smooth();
    ESP_LOGI(TAG, "左转完成，已复位");
}

// 右转 N 步
void Dog_TurnRightSteps(int steps) {
    if (steps < 1) steps = 1;
    if (steps > 10) steps = 10;

    ESP_LOGI(TAG, "开始右转 %d 步", steps);
    for (int i = 0; i < steps; i++) {
        servo_one_step_turn_right();
    }
    servo_reset_all_smooth();
    ESP_LOGI(TAG, "右转完成，已复位");
}

// ================================================================
// 持续行走控制
// ================================================================

void Dog_WalkStart(void) {
    servo_walk_start();
    ESP_LOGI(TAG, "开始持续前进");
}

void Dog_WalkStop(void) {
    servo_walk_stop();
    ESP_LOGI(TAG, "停止行走");
}

void Dog_EmergencyStop(void) {
    /* 先停止舞蹈编排（设 s_choreo_stop_req，等 choreo_task 退出），
       再停止底层伺服运动（设 s_emergency_stop，等 walk_task/action_task 退出）。
       顺序不能反：choreo_stop 会让 dog_action 的 while(choreo_is_playing()) 退出，
       servo_emergency_stop 才能等到 s_action_task_handle 变为 NULL。 */
    choreo_stop();
    servo_emergency_stop();
    ESP_LOGI(TAG, "紧急停止");
}

int Dog_GetWalkState(void) {
    return (int)servo_walk_get_state();
}

// ================================================================
// 摇摆控制封装
// ================================================================

void Dog_SwingFB(int cycles, int half_ms) {
    ESP_LOGI(TAG, "前后摇摆 cycles=%d half_ms=%d", cycles, half_ms);
    servo_swing_fb(15, cycles, half_ms);
    ESP_LOGI(TAG, "前后摇摆完成");
}

void Dog_SwingLR(int dir, int cycles, int half_ms) {
    ESP_LOGI(TAG, "左右摇摆 dir=%d cycles=%d half_ms=%d", dir, cycles, half_ms);
    servo_swing_lr(45, dir, cycles, half_ms);
    ESP_LOGI(TAG, "左右摇摆完成");
}

void Dog_SwingTwist(int cycles, int half_ms) {
    ESP_LOGI(TAG, "旋转摇摆 cycles=%d half_ms=%d", cycles, half_ms);
    servo_swing_twist(25, cycles, half_ms);
    ESP_LOGI(TAG, "旋转摇摆完成");
}

void Dog_SwingUpDown(int cycles, int half_ms) {
    ESP_LOGI(TAG, "上下摇摆 cycles=%d half_ms=%d", cycles, half_ms);
    servo_swing_updown(35, cycles, half_ms);
    ESP_LOGI(TAG, "上下摇摆完成");
}

void Dog_SwingSideLeft(int cycles, int half_ms) {
    ESP_LOGI(TAG, "左侧侧摇 cycles=%d half_ms=%d", cycles, half_ms);
    servo_swing_side_left(20, 20, cycles, half_ms);
    ESP_LOGI(TAG, "左侧侧摇完成");
}

void Dog_SwingSideRight(int cycles, int half_ms) {
    ESP_LOGI(TAG, "右侧侧摇 cycles=%d half_ms=%d", cycles, half_ms);
    servo_swing_side_right(20, 20, cycles, half_ms);
    ESP_LOGI(TAG, "右侧侧摇完成");
}

void Dog_SwingWave(int cycles, int step_ms) {
    ESP_LOGI(TAG, "波浪步 cycles=%d step_ms=%d", cycles, step_ms);
    servo_swing_wave(20, cycles, step_ms);
    ESP_LOGI(TAG, "波浪步完成");
}

void Dog_SwingMarch(int cycles, int half_ms) {
    ESP_LOGI(TAG, "原地踏步 cycles=%d half_ms=%d", cycles, half_ms);
    servo_swing_march(20, cycles, half_ms);
    ESP_LOGI(TAG, "原地踏步完成");
}

void Dog_SwingNod(int cycles, int half_ms) {
    ESP_LOGI(TAG, "侧向点头 cycles=%d half_ms=%d", cycles, half_ms);
    servo_swing_nod(18, cycles, half_ms);
    ESP_LOGI(TAG, "侧向点头完成");
}

void Dog_SwingTremble(int cycles, int half_ms) {
    ESP_LOGI(TAG, "颤抖 cycles=%d half_ms=%d", cycles, half_ms);
    servo_swing_tremble(8, cycles, half_ms);
    ESP_LOGI(TAG, "颤抖完成");
}

void Dog_BodySway(int fb, int lr) {
    servo_body_sway(fb, lr);
}

// ================================================================
// 长跳舞：除侧向点头和颤抖外，8种动作随机顺序各执行一次
// ================================================================
void Dog_DanceLong(void) {
    ESP_LOGI(TAG, "开始长跳舞（8种动作随机顺序）");

    srand(xTaskGetTickCount());

    const char* names[8] = {
        "前后摇摆", "左右摇摆", "旋转摇摆", "上下摇摆",
        "左侧侧摇", "右侧侧摇", "波浪步", "原地踏步"
    };
    int order[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    // Fisher-Yates 洗牌
    for (int i = 7; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    for (int i = 0; i < 8; i++) {
        ESP_LOGI(TAG, "长跳舞 [%d/8]: %s", i + 1, names[order[i]]);
        switch (order[i]) {
            case 0: Dog_SwingFB(2, 500); break;
            case 1: Dog_SwingLR((rand() % 2) ? 1 : -1, 2, 500); break;
            case 2: Dog_SwingTwist(2, 500); break;
            case 3: Dog_SwingUpDown(2, 500); break;
            case 4: Dog_SwingSideLeft(2, 500); break;
            case 5: Dog_SwingSideRight(2, 500); break;
            case 6: Dog_SwingWave(2, 350); break;
            case 7: Dog_SwingMarch(3, 300); break;
        }
    }

    servo_reset_all_smooth();
    ESP_LOGI(TAG, "长跳舞完成，已归位");
}

// ================================================================
// 短跳舞：前四种基础摇摆（前后/左右/旋转/上下）随机取3种
// ================================================================
void Dog_DanceShort(void) {
    ESP_LOGI(TAG, "开始短跳舞（4选3随机）");

    srand(xTaskGetTickCount());

    const char* names[4] = {
        "前后摇摆", "左右摇摆", "旋转摇摆", "上下摇摆"
    };
    int order[4] = {0, 1, 2, 3};

    // Fisher-Yates 洗牌
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "短跳舞 [%d/3]: %s", i + 1, names[order[i]]);
        switch (order[i]) {
            case 0: Dog_SwingFB(2, 500); break;
            case 1: Dog_SwingLR((rand() % 2) ? 1 : -1, 2, 500); break;
            case 2: Dog_SwingTwist(2, 500); break;
            case 3: Dog_SwingUpDown(2, 500); break;
        }
    }

    servo_reset_all_smooth();
    ESP_LOGI(TAG, "短跳舞完成，已归位");
}

// ================================================================
// 关机：拉低 IO3 连续两次，使 IP5306 断电
// ================================================================
static void ip5306_pull_low(void) {
    gpio_set_level(IP5306_SHUTDOWN_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(IP5306_SHUTDOWN_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
}

void Dog_Shutdown(void) {
    // 先停止所有行走和摇摆，并等待任务实际退出
    Dog_WalkStop();
    servo_wait_walk_idle(3000);

    // 归位后再拉 IO
    servo_reset_all();

    // 配置 IO3 为推挽输出，初始高电平
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << IP5306_SHUTDOWN_GPIO);
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(IP5306_SHUTDOWN_GPIO, 1);

    ESP_LOGI("DogShutdown", "正在关机，IO3 连续拉低两次...");

    // 第一次拉低
    ip5306_pull_low();
    // 第二次拉低
    ip5306_pull_low();

    ESP_LOGI("DogShutdown", "关机信号已发出，设备即将断电");
}

// ================================================================
// MCP 工具注册（InitMachineDog）
// ================================================================
void InitMachineDog(void) {
    auto& mcp = McpServer::GetInstance();

    // ========== 全体复位 ==========
    mcp.AddTool("腿部复位",
        "将所有腿部平滑归位到 90 度中立位置",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_ResetAll();
            return "已归位";
        });

    // ========== 自定义腿部角度控制（单工具，替代 12 个固定角度工具）==========
    mcp.AddTool("设置腿部角度",
        "设置指定腿部到指定角度。腿部编号：1=右前腿 2=右后腿 3=左后腿 4=左前腿；角度范围 45-135 度",
        PropertyList({
            Property("腿部编号", kPropertyTypeInteger, 1, 1, 4),
            Property("角度", kPropertyTypeInteger, 90, 45, 135)
        }),
        [](const PropertyList& props) -> ReturnValue {
            int leg   = props["腿部编号"].value<int>();
            int angle = props["角度"].value<int>();
            switch (leg) {
                case 1: Leg1_SetAngle(angle); break;
                case 2: Leg2_SetAngle(angle); break;
                case 3: Leg3_SetAngle(angle); break;
                case 4: Leg4_SetAngle(angle); break;
                default: return "腿部编号无效，请使用 1-4";
            }
            return "已设置完成";
        });

    // ========== 参数化步态工具（支持任意步数）==========
    // 前进 N 步
    mcp.AddTool("向前走",
        "向前走指定的步数，步数范围 1-10",
        PropertyList({
            Property("步数", kPropertyTypeInteger, 3, 1, 10)  // 默认3步，范围1-10
        }),
        [](const PropertyList& props) -> ReturnValue {
            int steps = props["步数"].value<int>();
            Dog_ForwardSteps(steps);
            return "前进完成";
        });

    // 后退 N 步
    mcp.AddTool("向后退",
        "向后退指定的步数，步数范围 1-10",
        PropertyList({
            Property("步数", kPropertyTypeInteger, 3, 1, 10)
        }),
        [](const PropertyList& props) -> ReturnValue {
            int steps = props["步数"].value<int>();
            Dog_BackwardSteps(steps);
            return "后退完成";
        });

    // 左转 N 步
    mcp.AddTool("左转",
        "向左转指定的步数，步数范围 1-10",
        PropertyList({
            Property("步数", kPropertyTypeInteger, 3, 1, 10)
        }),
        [](const PropertyList& props) -> ReturnValue {
            int steps = props["步数"].value<int>();
            Dog_TurnLeftSteps(steps);
            return "左转完成";
        });

    // 右转 N 步
    mcp.AddTool("右转",
        "向右转指定的步数，步数范围 1-10",
        PropertyList({
            Property("步数", kPropertyTypeInteger, 3, 1, 10)
        }),
        [](const PropertyList& props) -> ReturnValue {
            int steps = props["步数"].value<int>();
            Dog_TurnRightSteps(steps);
            return "右转完成";
        });

    // ========== 持续行走控制 ==========
    mcp.AddTool("开始行走",
        "持续向前行走，直到收到停止命令",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_WalkStart();
            return "开始持续前进";
        });

    mcp.AddTool("停止行走",
        "停止行走并归位",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_WalkStop();
            return "已停止，归位";
        });

    mcp.AddTool("紧急停止",
        "立即中断所有运动（行走、摇摆、跳舞等），强制归位。用于紧急情况下的急停",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_EmergencyStop();
            return "紧急停止完成";
        });

    // ========== 摇摆控制 ==========

    // 前后摇摆（语音指令：摇摆到最大幅度，2 个完整周期后归位）
    mcp.AddTool("前后摇摆",
        "机器狗前后摇摆身体，摆动到 15°，摇摆两次后归位",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingFB(2, 500);
            Dog_ResetAll();
            return "前后摇摆完成";
        });

    // 左右摇摆（语音指令：摇摆到最大幅度，2 个完整周期后归位）
    mcp.AddTool("左右摇摆",
        "机器狗左右摇摆身体，摆动到最大幅度，摇摆两次后归位",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingLR(1, 2, 500);
            Dog_ResetAll();
            return "左右摇摆完成";
        });

    // 旋转摇摆（身体扭转）
    mcp.AddTool("旋转摇摆",
        "机器狗身体旋转扭摆（最大 25°），前腿向前后腿向后交替扭转，重复两次后归位",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingTwist(2, 500);
            Dog_ResetAll();
            return "旋转摇摆完成";
        });

    // 上下摇摆（蹲起）
    mcp.AddTool("上下摇摆",
        "机器狗做蹲起动作，四腿同时前后岔开下沉再归位，重复两次",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingUpDown(2, 500);
            Dog_ResetAll();
            return "上下摇摆完成";
        });

    // 左侧侧摇
    mcp.AddTool("左侧侧摇",
        "左腿前后岔开±20°，右腿向内收20°，机身重心左移",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingSideLeft(2, 500);
            Dog_ResetAll();
            return "左侧侧摇完成";
        });

    // 右侧侧摇
    mcp.AddTool("右侧侧摇",
        "右腿前后岔开±20°，左腿向内收20°，机身重心右移",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingSideRight(2, 500);
            Dog_ResetAll();
            return "右侧侧摇完成";
        });

    // 波浪步
    mcp.AddTool("波浪步",
        "圈1 FL→BL→BR→FR，圈2 BR→BL→FL，四腿接力波浪",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingWave(2, 350);
            Dog_ResetAll();
            return "波浪步完成";
        });

    // 原地踏步
    mcp.AddTool("原地踏步",
        "对角腿交替岔开归位，模拟快节奏原地踏步动作",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingMarch(3, 300);
            Dog_ResetAll();
            return "原地踏步完成";
        });

    // 侧向点头
    mcp.AddTool("侧向点头",
        "前腿左右交替前伸，后腿保持不动，模拟机器狗左右点头",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingNod(3, 400);
            Dog_ResetAll();
            return "侧向点头完成";
        });

    // 颤抖
    mcp.AddTool("颤抖",
        "四腿高频微幅对角交替，模拟机器狗兴奋颤抖状态",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingTremble(12, 150);
            Dog_ResetAll();
            return "颤抖完成";
        });

    // 振荡（超长颤抖，约10秒）
    mcp.AddTool("振荡",
        "超长颤抖，连续高频微幅对角交替约10秒，模拟机器狗剧烈振荡",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_SwingTremble(33, 150);
            Dog_ResetAll();
            return "振荡完成";
        });

    // 摇杆实时姿态控制（小程序摇杆专用）
    mcp.AddTool("身体姿态控制",
        "直接设置机器狗身体姿态。fb=前后倾斜量（正=前倾，负=后倾），lr=左右倾斜量（正=右倾，负=左倾），范围 -45 到 45",
        PropertyList({
            Property("fb", kPropertyTypeInteger, 0, -45, 45),
            Property("lr", kPropertyTypeInteger, 0, -45, 45),
        }),
        [](const PropertyList& props) -> ReturnValue {
            int fb = props["fb"].value<int>();
            int lr = props["lr"].value<int>();
            Dog_BodySway(fb, lr);
            return "姿态已设置";
        });
    
    // ========== 长跳舞 ==========
    mcp.AddTool("长跳舞",
        "8种动作随机顺序各执行一次（前后/左右/旋转/上下/左倾/右倾/波浪/踏步），完成后归位",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_DanceLong();
            return "长跳舞完成";
        });
    
    // ========== 短跳舞 ==========
    mcp.AddTool("短跳舞",
        "4种基础摇摆随机取3种（前后/左右/旋转/上下），完成后归位",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_DanceShort();
            return "短跳舞完成";
        });
    
    // ========== 显示网络信息 ==========
    mcp.AddTool("显示网络信息",
        "在屏幕上显示当前WiFi连接的IP地址，并语音播报",
        PropertyList(),
        [](auto&) -> ReturnValue {
            auto& wifi = WifiManager::GetInstance();
            std::string ip = wifi.GetIpAddress();
            if (ip.empty()) {
                return "未连接到WiFi网络，请检查网络配置";
            }
            auto& board = Board::GetInstance();
            board.GetDisplay()->ShowNotification(("IP: " + ip).c_str(), 8000);
            std::string result = "当前网络IP地址: " + ip;
            return result;
        });

    // ========== 进入配网模式 ==========
    mcp.AddTool("进入配网模式",
        "重置WiFi并进入热点配网模式，OLED屏幕会显示配网指引",
        PropertyList(),
        [](auto&) -> ReturnValue {
            auto* wifi_board = static_cast<WifiBoard*>(&Board::GetInstance());
            wifi_board->EnterWifiConfigMode();
            return "已进入配网模式，请连接Xiaozhi开头的热点，浏览器打开192.168.4.1配置WiFi";
        });

    // ========== 系统关机 ==========
    mcp.AddTool("系统关机",
        "关闭机器狗电源。连续两次拉低 IO3 使 IP5306 断电。请确认后再执行。",
        PropertyList(),
        [](auto&) -> ReturnValue {
            Dog_Shutdown();
            return "关机信号已发送";
        });

    // ========== 舞蹈编排 ==========
    mcp.AddTool("舞蹈编排",
        "按 /assets/<名称>.json 编排执行舞蹈并播放配乐。"
        "名称不需要后缀，例如 'dance_01' 对应 /assets/dance_01.json。"
        "可通过 JSON 文件自定义舞步顺序、速度倍率、音乐偏移。",
        PropertyList({
            Property("名称", kPropertyTypeString, /*default*/"dance_01"),
        }),
        [](const PropertyList& props) -> ReturnValue {
            std::string name = props["名称"].value<std::string>();
            if (name.empty()) {
                return "请提供舞蹈名称（对应 /assets/<名称>.json）";
            }
            ESP_LOGI("ChoreoMCP", "收到舞蹈编排请求: %s", name.c_str());
            choreo_routine_t* routine = choreo_load(name.c_str());
            if (!routine) {
                char err[128];
                snprintf(err, sizeof(err), "无法加载舞蹈编排: %s", name.c_str());
                return err;
            }
            esp_err_t ret = choreo_play_async(routine, "choreo_task");
            if (ret != ESP_OK) {
                choreo_free(routine);
                return "舞蹈任务启动失败";
            }
            return "舞蹈编排已开始执行";
        });

    // ========== 自定义舞步：扫描 /assets/*.json，文件名即触发词 ==========
    // 用户只需放入 disco.json + disco.wav，语音说"跳 disco"即可触发
    choreo_name_list_t clist = choreo_list_names();
    for (int i = 0; i < clist.count; i++) {
        std::string choreo_name = clist.names[i];
        // 跳过与内置舞蹈编排同名的文件（避免冲突）
        if (choreo_name == "舞蹈编排") continue;

        char desc[160];
        snprintf(desc, sizeof(desc),
            "执行自定义舞蹈编排 \"%s\"（/assets/%s.json），步态与音乐同步",
            choreo_name.c_str(), choreo_name.c_str());

        mcp.AddTool(choreo_name, desc, PropertyList(),
            [choreo_name](auto&) -> ReturnValue {
                ESP_LOGI("ChoreoMCP", "自定义舞蹈编排: %s", choreo_name.c_str());
                choreo_routine_t* routine = choreo_load(choreo_name.c_str());
                if (!routine) {
                    return "无法加载舞蹈编排: " + choreo_name;
                }
                esp_err_t ret = choreo_play_async(routine, "choreo_task");
                if (ret != ESP_OK) {
                    choreo_free(routine);
                    return "舞蹈编排启动失败";
                }
                return choreo_name + " 舞蹈已开始";
            });
    }
    choreo_name_list_free(&clist);

    ESP_LOGI(TAG, "机器狗 MCP 工具注册完成");
}
