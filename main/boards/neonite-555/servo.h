#pragma once
/**
 * @file servo.h
 * @brief 四足机器狗舵机驱动 + 6拍 Trot 步态
 *
 * 硬件参数（参考 dog_control.cc）：
 *   - 右前腿 FR : GPIO9  / LEDC_CHANNEL_0
 *   - 右后腿 BR : GPIO10 / LEDC_CHANNEL_1
 *   - 左后腿 BL : GPIO21 / LEDC_CHANNEL_2
 *   - 左前腿 FL : GPIO47 / LEDC_CHANNEL_3
 *
 * 软件限位：45° ~ 135°，上电归位 90°
 * 抬腿 lift 固定值：15°（从中位上下偏移，不提供运行时修改接口）
 */

#include "esp_err.h"
#include <stdint.h>
#include <driver/ledc.h>
#include <driver/gpio.h>

/* ================================================================
 * 硬件与限位配置
 * ================================================================ */
#define SERVO_FREQ_HZ        50
#define SERVO_BIT_RES        LEDC_TIMER_12_BIT          /* 12位分辨率 */
#define SERVO_MAX_DUTY       ((1 << 12) - 1)              /* 4095 */

#define SERVO_GPIO_FR        GPIO_NUM_9   /* 右前腿 */
#define SERVO_GPIO_BR        GPIO_NUM_10  /* 右后腿 */
#define SERVO_GPIO_BL        GPIO_NUM_21  /* 左后腿 */
#define SERVO_GPIO_FL        GPIO_NUM_47  /* 左前腿 */

#define SERVO_CH_FR          LEDC_CHANNEL_0
#define SERVO_CH_BR          LEDC_CHANNEL_1
#define SERVO_CH_BL          LEDC_CHANNEL_2
#define SERVO_CH_FL          LEDC_CHANNEL_3

#define SERVO_ANGLE_MIN      45   /* 最小限位（°） */
#define SERVO_ANGLE_MAX      135  /* 最大限位（°） */
#define SERVO_ANGLE_HOME     90   /* 上电归位 */
#define SERVO_LIFT_FIXED     20   /* 抬腿偏移固定值 */

/* ================================================================
 * 步态参数（参考 dog_control.cc）
 * ================================================================ */
#define WALK_STEP_PERIOD_MS   1500  /* 一步周期（ms） */
#define WALK_STEP_SWING       12   /* 步幅（°） */
#define WALK_LIFT_DEGREE      20   /* 抬腿偏移（从中位±lift） */
#define WALK_SMOOTH_STEPS     12   /* 每拍插值细分步数（越大越平滑，建议 8~20） */

/* ================================================================
 * 四腿舵机结构体
 * ================================================================ */
typedef struct {
    ledc_channel_t ch;
    gpio_num_t     gpio;
    uint32_t       angle;      /* 当前角度 */
    uint32_t       lift;       /* 抬腿偏移（从中位的偏移量） */
} servo_leg_t;

/* ================================================================
 * 行走状态
 * ================================================================ */
typedef enum {
    WALK_STATE_IDLE = 0,
    WALK_STATE_WALKING,      /* 前进 */
    WALK_STATE_BACKING,      /* 倒退 */
    WALK_STATE_TURN_LEFT,    /* 左转 */
    WALK_STATE_TURN_RIGHT,   /* 右转 */
} walk_state_t;

/* ================================================================
 * API
 * ================================================================ */

/* 初始化全部四个舵机，LEDC + 归位 90° */
esp_err_t servo_init(void);

/* ---- 单腿角度控制（限幅自动保护） ---- */
void servo_set_angle_fr(uint32_t angle);
void servo_set_angle_br(uint32_t angle);
void servo_set_angle_bl(uint32_t angle);
void servo_set_angle_fl(uint32_t angle);

/* 抬腿偏移已固定为 SERVO_LIFT_FIXED=20°，不再提供运行时修改接口 */

/* 查询 */
uint32_t servo_get_angle_fr(void);
uint32_t servo_get_angle_br(void);
uint32_t servo_get_angle_bl(void);
uint32_t servo_get_angle_fl(void);

/* 全部归位 90°（瞬间，仅供初始化使用） */
void servo_reset_all(void);

/* 全部平滑归位 90°（动作完成后调用，防止电压冲击） */
void servo_reset_all_smooth(void);

/* 全部依次归位 90°（上电/急停后调用，逐条腿降低电源冲击） */
void servo_reset_all_sequential(void);

/* ---- 遥控：启动 / 停止异步行走 ---- */
void servo_walk_start(void);        /* 启动后台持续前进 */
void servo_walk_back_start(void);   /* 启动后台持续倒退 */
void servo_turn_left_start(void);   /* 启动后台持续左转 */
void servo_turn_right_start(void); /* 启动后台持续右转 */
void servo_walk_stop(void);         /* 停止行走并归位 */
void servo_wait_walk_idle(int timeout_ms);  /* 等待行走任务退出（毫秒） */
walk_state_t servo_walk_get_state(void);

/* ---- 紧急停止：立即中断所有运动（行走+摇摆），强制归位 ---- */
void servo_emergency_stop(void);

/* 设置异步动作任务句柄（供紧急停止等待动作任务退出） */
void servo_set_action_task_handle(void* handle);

/* 单步步态（同步，供 walk_task 内部调用） */
void servo_one_step(void);       /* 前进一步 */
void servo_one_step_back(void);  /* 倒退一步 */
void servo_one_step_turn_left(void);  /* 左转一步（4拍对角交替） */
void servo_one_step_turn_right(void); /* 右转一步（4拍对角交替，左转镜像） */

/* ---- 身体摇摆（同步，自动停止 walk_task，完成后归位）---- */
/*
 * servo_swing_fb: 前后摇摆
 *   amplitude : 摇摆半幅（°），建议 10~15，超出限位自动截断
 *   cycles    : 完整往返周期数（语音控制传 2）
 *   half_ms   : 每个半摆时长(ms)，建议 400~600
 *
 *   原理：4腿同时偏 +amplitude → -amplitude → 归位，每往返算 1 cycle
 *         角度极性：右侧(FR/BR) 增大=向前；左侧(FL/BL) 减小=向前
 *         前后摇摆时 4 腿同向偏移（同正同负），身体重心前后移动
 */
void servo_swing_fb(int amplitude, int cycles, int half_ms);

/*
 * servo_swing_lr: 左右摇摆
 *   amplitude : 摇摆半幅（°），建议 10~45
 *   dir       : 1=先右倾  -1=先左倾
 *   cycles    : 完整往返周期数（语音控制传 2）
 *   half_ms   : 每个半摆时长(ms)
 *
 *   原理：向右倾 → 动侧=左(FL/BL) 前后岔开，静侧=右(FR/BR) 保持 90°
 *         FL = 90 - amplitude  (左前向前)
 *         BL = 90 + amplitude  (左后向后)
 *         → 左侧撑高，机身右倾；向左倾时对称处理
 */
void servo_swing_lr(int amplitude, int dir, int cycles, int half_ms);

/*
 * servo_swing_twist: 旋转摇摆（身体扭转）
 *   前腿同时向前 + 后腿同时向后，再反相，形成 S 型扭转
 *   amplitude : 扭转半幅（°），建议 10~25
 *   cycles    : 完整往返周期数
 *   half_ms   : 每个半摆时长(ms)
 */
void servo_swing_twist(int amplitude, int cycles, int half_ms);

/*
 * servo_swing_updown: 上下摇摆（蹲起）
 *   四腿同时前后岔开下沉，再回归 90° 抬起
 *   amplitude : 岔开半幅（°），建议 15~45
 *   cycles    : 完整蹲起周期数
 *   half_ms   : 每个半摆时长(ms)
 */
void servo_swing_updown(int amplitude, int cycles, int half_ms);

/*
 * servo_swing_side_left: 左侧侧摇
 *   左腿岔开↔反岔开交替（正负值都有），右腿保持岔开向内收固定不动
 *   amplitude : 左腿摇摆幅度（°），建议 15~30
 *   in_deg    : 右腿固定内收角度（°），通常 20
 *   cycles    : 摇摆周期数
 *   half_ms   : 每拍时长(ms)
 */
void servo_swing_side_left(int amplitude, int in_deg, int cycles, int half_ms);

/*
 * servo_swing_side_right: 右侧侧摇（左侧侧摇镜像）
 *   右腿岔开↔反岔开交替（正负值都有），左腿保持岔开向内收固定不动
 */
void servo_swing_side_right(int amplitude, int in_deg, int cycles, int half_ms);

/*
 * servo_swing_wave: 波浪步
 *   逆时针一圈(FL→FR→BR→BL)，然后顺时针一圈(BL→BR→FR→FL)
 *   每圈5段波形，前腿归位时后腿同步出击，形成连续波浪接力
 *   amplitude : 前伸幅度（°），建议 15~25
 *   cycles    : 双向循环次数（每圈含逆时针+顺时针各一圈）
 *   step_ms   : 每步时长(ms)
 */
void servo_swing_wave(int amplitude, int cycles, int step_ms);

/*
 * servo_swing_march: 原地踏步
 *   对角腿交替岔开归位，比行走步态更快更紧凑
 *   amplitude : 岔开幅度（°），建议 15~20
 *   cycles    : 踏步周期数
 *   half_ms   : 每拍时长(ms)
 */
void servo_swing_march(int amplitude, int cycles, int half_ms);

/*
 * servo_swing_nod: 侧向点头
 *   前腿左右交替前伸，后腿保持 90°，模拟机器狗左右点头
 *   amplitude : 前伸幅度（°），建议 15~20
 *   cycles    : 点头周期数
 *   half_ms   : 每拍时长(ms)
 */
void servo_swing_nod(int amplitude, int cycles, int half_ms);

/*
 * servo_swing_tremble: 颤抖
 *   四腿高频微幅对角交替，模拟颤抖/兴奋感
 *   amplitude : 颤抖幅度（°），建议 6~10
 *   cycles    : 颤抖次数（建议 8~12）
 *   half_ms   : 每拍时长（建议 100~150ms）
 */
void servo_swing_tremble(int amplitude, int cycles, int half_ms);

/*
 * servo_body_sway: 按任意前后/左右幅度设置机身姿态（摇杆直接调用，非周期性）
 *   fb  : 前后偏移量（°），正=前倾，负=后倾，0=无前后偏
 *   lr  : 左右偏移量（°），正=右倾，负=左倾，0=无左右偏
 *
 *   说明：直接设置舵机角度，不平滑过渡（由 ESP32 端快速响应摇杆实时指令）
 *         自动停止 walk_task，调用 servo_reset_all() 可恢复
 */
void servo_body_sway(int fb, int lr);
