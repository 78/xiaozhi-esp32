### **`motion.h` 模块开发需求文档 (PRD)**

#### **1. 模块概述**

`motion.h`是原子技能层中负责**所有身体姿态和动态表现**的核心模块。它需要为上层应用提供**两种不同粒度**的控制接口：
1.  **声明式接口 (`motion_perform`):** 用于调用预设的、富有表现力的、复杂的连续动作序列。这是上层（尤其是反应池）**最常用**的接口。
2.  **命令式接口 (`motion_set_angle`):** 用于执行精确的、一次性的静态角度定位。主要供需要精确控制的场景使用（如LLM的特定指令或校准程序）。

所有动作的执行都必须是**异步非阻塞**的，通过一个专门的FreeRTOS后台任务 (`motion_task`) 来实现。

#### **2. 预设动作序列清单 (`motion_id_t`)**

通过对《事件交互设计总表》中所有反应池的`perform_motion`调用进行**全面梳理和归并**，我们提炼出以下**必须实现**的预设动作序列清单。

| 动作ID (motion_id_t 枚举) | 引用场景 | 动作描述 (面向设计师) | 技术实现要点 (面向工程师) |
| :--- | :--- | :--- | :--- |
| **`MOTION_HAPPY_WIGGLE`** | 触摸头部 | 表达“开心”的、小幅度的、快速的左右摇摆。 | 快速、小角度的往复运动，节奏欢快。 |
| **`MOTION_SHAKE_HEAD`** | 触摸头部 | 表达“不同意”或“烦躁”的、清晰的摇头动作。 | 角度较大、速度较快的往复运动，带有明确的停顿。 |
| **`MOTION_DODGE_SUBTLE`** | 触摸头部 | 一个快速、小幅度的躲闪，然后缓慢恢复。 | 快速转到一个小角度，然后用更长的时间平滑地回到中心。 |
| **`MOTION_NUZZLE_FORWARD`** | 按住头部 | 表达“亲昵”的、主动向前“蹭”的动作。 | 缓慢地向一个正角度转动，并保持片刻。 |
| **`MOTION_TENSE_UP`** | 按住头部 / 被拿起 | 表达“紧张”或“害怕”的、身体瞬间绷紧的感觉。 | 舵机扭力瞬间开到最大以产生“僵硬”感，可能伴随微小、高频的抖动。 |
| **`MOTION_DODGE_SLOWLY`** | 按住头部 | 在不情愿的状态下，缓慢地躲开。 | 用很长的时间，平滑地转到一个小角度。 |
| **`MOTION_QUICK_TURN_LEFT/RIGHT`** | 触摸身体 | 对身体侧面触摸的、快速、好奇的转向。 | 快速、精准地转到一个中等角度（如+/-30度）。 |
| **`MOTION_CURIOUS_PEEK_LEFT/RIGHT`**| 触摸身体 | 模拟“探头探脑”的好奇窥探动作。 | 一个包含“转动->停顿->小幅晃动->回正”的复杂序列。 |
| **`MOTION_SLOW_TURN_LEFT/RIGHT`** | 触摸身体 | 慵懒地、慢悠悠地看一眼。 | 用极慢的速度转到一个小角度，然后同样缓慢地回来。 |
| **`MOTION_DODGE_OPPOSITE_LEFT/RIGHT`**| 触摸身体 | 被触摸一侧后，迅速向**反方向**躲开。 | `touch_left`时，快速转到`+`角度；`touch_right`时，快速转到`-`角度。 |
| **`MOTION_BODY_SHIVER`** | 触摸身体 | 表达“被打扰”或“冷”的、快速、小幅的身体抖动。 | 舵机在中心点附近进行非常快速、微小的往复运动。 |
| **`MOTION_EXCITED_JIGGLE`** | 被拿起 | 被拿起时，表达极度兴奋的、原地快速晃动。 | 类似`HAPPY_WIGGLE`，但幅度更大、频率更高。 |
| **`MOTION_RELAX_COMPLETELY`** | 被拥抱 | 被拥抱时，身体完全放松，可能会有轻微的姿态调整。 | 舵机扭力降低，让其处于一个非常松弛的状态。 |
| **`MOTION_TICKLE_TWIST_DANCE`** | 被挠痒痒 | 被挠痒痒时，无法控制的、开心的、大幅度的来回扭动。 | 大角度、快速、无规律的往复运动，模拟“笑到失控”的样子。 |
| **`MOTION_ANNOYED_TWIST_TO_HAPPY`** | 被挠痒痒 | 从烦躁的扭动，逐渐过渡到开心的扭动的复杂序列。 | 动作序列的初始节奏是不耐烦的，后半段逐渐变得欢快。 |
| **`MOTION_STRUGGLE_TWIST`** | 被倒置 | 被倒置时，表达“慌乱挣扎”的、不规则的扭动。 | 大角度、快速、但节奏混乱的往复运动。 |
| **`MOTION_UNWILLING_TURN_BACK`**| (冷却阶段) | 从一个偏移角度，不情愿地、带停顿地恢复到中心。 | 这是一个从任意角度回到0度的序列，但速度曲线是非线性的，中间可以加入小的反向运动。 |
| **`MOTION_RELAX_TO_CENTER`** | (冷却阶段) | 从一个偏移角度，非常放松、平滑地恢复到中心。 | 这是一个从任意角度回到0度的序列，速度非常缓慢、平滑。 |

**总结：**
总共需要实现的预设动作序列为 **18个**。

#### **3. 函数接口定义 (`motion.h`)**

```c
#ifndef SKILLS_MOTION_H_
#define SKILLS_MOTION_H_

#include <stdint.h>

// 所有预设的、声明式的动作ID
typedef enum {
    MOTION_HAPPY_WIGGLE,
    MOTION_SHAKE_HEAD,
    MOTION_DODGE_SUBTLE,
    MOTION_NUZZLE_FORWARD,
    MOTION_TENSE_UP,
    MOTION_DODGE_SLOWLY,
    MOTION_QUICK_TURN_LEFT,
    MOTION_QUICK_TURN_RIGHT,
    MOTION_CURIOUS_PEEK_LEFT,
    MOTION_CURIOUS_PEEK_RIGHT,
    MOTION_SLOW_TURN_LEFT,
    MOTION_SLOW_TURN_RIGHT,
    MOTION_DODGE_OPPOSITE_LEFT,
    MOTION_DODGE_OPPOSITE_RIGHT,
    MOTION_BODY_SHIVER,
    MOTION_EXCITED_JIGGLE,
    MOTION_RELAX_COMPLETELY,
    MOTION_TICKLE_TWIST_DANCE,
    MOTION_ANNOYED_TWIST_TO_HAPPY,
    MOTION_STRUGGLE_TWIST,
    MOTION_UNWILLING_TURN_BACK,
    MOTION_RELAX_TO_CENTER,
    // ...可继续扩展
} motion_id_t;

// 用于命令式控制的速度枚举
typedef enum {
    MOTION_SPEED_SLOW,
    MOTION_SPEED_MEDIUM,
    MOTION_SPEED_FAST,
} motion_speed_t;

/**
 * @brief 初始化身体动作技能模块
 *        内部会创建并启动一个后台FreeRTOS任务来处理所有动作
 */
void motion_init(void);

/**
 * @brief 【声明式接口】执行一个预设的、复杂的动作序列
 *        此函数会立即返回 (非阻塞)。动作将在后台任务中执行。
 *        如果后台任务正在执行上一个动作，此新动作会覆盖它。
 * 
 * @param id 要执行的动作ID
 */
void motion_perform(motion_id_t id);

/**
 * @brief 【命令式接口】控制身体转到一个精确的静态角度
 *        此函数会立即返回 (非阻塞)。舵机将在后台任务中平滑地转动到目标角度。
 * 
 * @param angle 目标角度 (-90.0 to 90.0)
 * @param speed 转动速度
 */
void motion_set_angle(float angle, motion_speed_t speed);

/**
 * @brief 查询身体当前是否正在执行一个动作
 * 
 * @return true 如果后台任务正忙, false 如果已空闲
 */
bool motion_is_busy(void);

/**
 * @brief 立即停止当前所有身体动作，并保持在当前位置
 */
void motion_stop(void);

#endif // SKILLS_MOTION_H_
```
