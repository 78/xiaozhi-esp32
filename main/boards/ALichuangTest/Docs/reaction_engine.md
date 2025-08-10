reaction_engine.h: 反应决策器
模块概述:
reaction_engine是本地反射的“大脑皮层”。它的唯一职责是：当一个事件发生时，它会立刻“咨询”emotion_engine当前的心情，然后决策出应该执行哪一个具体的“动作套件”，并将这个决策结果发送给action_queue去执行。
核心需求与逻辑:
反应池数据结构: 模块内部需要一个核心的、多维的反应池查找表。这个数据结构必须能根据事件ID和情感象限ID，快速定位到一个包含一个或多个“动作套件”的列表。
动作套件定义: 一个“动作套件” (action_kit_t) 是一个由多个原子技能ID（如MOTION_SHAKE_HEAD, ANIMATION_ANNOYED_GLARE）组成的结构体，代表一组需要并行执行的动作。
决策逻辑:
当接收到新事件时，它会立即调用emotion_engine_get_quadrant()获取当前情感象限。
使用event_id和quadrant_id作为双重索引，在反应池查找表中找到对应的动作套件列表。
如果列表包含多个套件，就随机选择一个。
如果事件是物理反射类的，则忽略情感象限，直接查找该事件对应的唯一套件。
输出到动作队列: 将最终决策出的action_kit_t发送给action_queue模块去执行。
输入:
由event_engine发布的event_id_t。
由emotion_engine提供的emotion_quadrant_t。
输出:
一个action_kit_t结构体，发送给action_queue。
函数接口定义 (reaction_engine.h):
code
C
#ifndef INTERACTION_REACTION_ENGINE_H_
#define INTERACTION_REACTION_ENGINE_H_

#include "event_engine.h"

/**
 * @brief 初始化反应决策器
 */
void reaction_engine_init(void);

/**
 * @brief [核心接口] 根据一个事件进行决策并触发反应
 *        此函数会被事件回调函数所调用。它内部会去查询情感状态，
 *        并最终将决策出的动作套件发送给action_queue。
 * @param event_id 发生的新事件ID
 */
void reaction_engine_on_event(event_id_t event_id);

#endif // INTERACTION_REACTION_ENGINE_H_