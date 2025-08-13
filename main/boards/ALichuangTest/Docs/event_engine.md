模块概述:
作为交互层的“神经末梢”，event_engine负责将来自L0驱动层的、离散的、原始的传感器数据，实时地处理和融合成L2层可以理解的、有意义的高级事件。它是连接物理世界和虚拟情感的桥梁。
核心需求与逻辑:
后台任务驱动: event_engine必须在一个独立的、周期性运行的FreeRTOS后台任务 (event_task) 中执行，以确保不阻塞其他系统。推荐运行频率为20-50Hz（每20-50ms运行一次）。
短期事件缓冲区: 引擎内部需维护一个固定大小的FIFO（先进先出）队列作为“短期记忆”，用于存储最近1-2秒内发生的所有基础事件（如TOUCH_HEAD_ON, TOUCH_HEAD_OFF, IMU_HIGH_G_IMPACT）。每个基础事件都应带有精确的时间戳。
状态机与规则引擎:
简单事件识别: 通过状态机判断触摸传感器的ON/OFF和持续时间，来生成EVENT_TOUCH_HEAD_SINGLE、EVENT_TOUCH_HEAD_HOLD等简单事件。
复合事件识别: 周期性地扫描事件缓冲区，应用一系列规则来匹配模式，生成复合事件。例如：
规则EVENT_TICKLED: IF 缓冲区在2秒内的TOUCH_ON事件数量 > 4, THEN 生成EVENT_TICKLED。
规则EVENT_INVERTED: IF IMU的重力向量Z轴持续为负超过1秒, THEN 生成EVENT_INVERTED。
事件发布机制:
当一个高级事件被成功识别后，引擎应通过一个全局的回调函数指针或消息队列，将该事件的ID (event_id_t) “发布”出去，供emotion_engine和reaction_engine等上层模块订阅和接收。
事件抑制逻辑: 为避免事件风暴，需要实现抑制逻辑。例如，在生成一个EVENT_TICKLED复合事件后，应在接下来的一小段时间内抑制所有构成它的简单TOUCH事件的生成。
输入:
IMU传感器数据（加速度、角速度）。
三个触摸传感器的实时状态（按下/弹起）。
输出 (发布的事件ID event_id_t):
EVENT_TOUCH_HEAD_SINGLE
EVENT_TOUCH_HEAD_HOLD
EVENT_TOUCH_LEFT_SINGLE
EVENT_TOUCH_RIGHT_SINGLE
EVENT_PICKED_UP (复合)
EVENT_CRADLED (复合)
EVENT_TICKLED (复合)
EVENT_PUT_DOWN (复合)
EVENT_INVERTED (复合)
EVENT_IMU_FREE_FALL
EVENT_IMU_SHAKE_VIOLENT
函数接口定义 (event_engine.h):
code
C
#ifndef INTERACTION_EVENT_ENGINE_H_
#define INTERACTION_EVENT_ENGINE_H_

// 定义所有高级事件的ID
typedef enum { EVENT_NONE, EVENT_TOUCH_HEAD_SINGLE, ... } event_id_t;

// 定义事件回调函数的类型
typedef void (*event_callback_t)(event_id_t event_id);

/**
 * @brief 初始化智能事件引擎
 *        内部会创建并启动一个后台FreeRTOS任务来处理传感器数据
 */
void event_engine_init(void);

/**
 * @brief 注册一个事件回调函数
 *        当引擎识别出新事件时，将通过此回调通知订阅者
 * @param callback 要注册的回调函数指针
 */
void event_engine_register_callback(event_callback_t callback);

#endif // INTERACTION_EVENT_ENGINE_H_