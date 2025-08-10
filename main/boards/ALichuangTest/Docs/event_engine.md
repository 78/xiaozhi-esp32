emotion_engine.h: 情感状态机
模块概述:
emotion_engine是玩具“性格”的数学模型。它不直接产生行为，而是作为一个状态管理器，根据外部事件来更新内部的{Happiness, Energy}情感坐标，并为其他模块提供查询当前情感状态的服务。
核心需求与逻辑:
数据维护: 模块内部需维护两个核心浮点数变量：current_happiness和current_energy。
情感影响映射表: 内部需要一个static const的查找表（或switch-case结构），将每个event_id_t映射到一个预设的{ΔH, ΔE}变化值。
状态更新: 当接收到新事件时，根据查找表更新current_happiness和current_energy。
边界与饱和: 更新后的情感值必须被限制在**[-10.0, +10.0]**的范围内。
时间衰减: 模块需要一个低频定时器（例如每10-30秒触发一次）。每次触发时，将当前的情感值向默认的基线（如H=+2.0, E=0.0）缓慢地移动一小步，模拟情绪的自然平复。
输入:
由event_engine发布的event_id_t。
由云端下发的直接情感覆写指令。
输出 (供查询):
当前所属的情感象限 (emotion_quadrant_t枚举)。
当前的精确{H, E}坐标值（可选，用于调试或更精细的逻辑）。
函数接口定义 (emotion_engine.h):

#ifndef INTERACTION_EMOTION_ENGINE_H_
#define INTERACTION_EMOTION_ENGINE_H_

#include "event_engine.h" // 依赖事件定义

// 定义四个情感象限
typedef enum { 
    QUADRANT_HAPPY_EXCITED, 
    QUADRANT_HAPPY_CALM, 
    QUADRANT_UNHAPPY_ENERGETIC, 
    QUADRANT_UNHAPPY_CALM 
} emotion_quadrant_t;

/**
 * @brief 初始化情感状态机
 *        内部会启动一个用于时间衰减的低频定时器
 */
void emotion_engine_init(void);

/**
 * @brief [核心接口] 根据一个事件来更新情感状态
 *        此函数会被事件回调函数所调用
 * @param event_id 发生的新事件ID
 */
void emotion_engine_on_event(event_id_t event_id);

/**
 * @brief [核心接口] 从云端直接设置情感状态
 * @param h 目标Happiness值
 * @param e 目标Energy值
 */
void emotion_engine_set_state(float h, float e);

/**
 * @brief [核心接口] 获取当前所属的情感象限
 * @return emotion_quadrant_t 当前的情感象限
 */
emotion_quadrant_t emotion_engine_get_quadrant(void);

#endif // INTERACTION_EMOTION_ENGINE_H_