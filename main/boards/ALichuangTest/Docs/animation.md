

### **`animation.h` 模块开发需求文档 (PRD)**

#### **1. 模块概述**

`animation.h`是原子技能层中负责**所有视觉情感表达**的核心模块。它通过在屏幕上播放预设的动画序列，赋予玩具丰富的面部表情和状态显示。

该模块将基于**LVGL图形库**实现，采用**分层、组合**的动画设计理念（即眼睛、嘴巴等元素作为独立图层），并通过一个**自定义的、轻量级的动画描述格式**来驱动。所有动画的播放都必须是**异步非阻塞**的，通过一个专门的后台任务 (`animation_task`或集成在LVGL的主任务`lv_timer_handler`中)来更新，以确保动画的流畅性不影响系统其他部分的响应。

#### **2. 预设动画清单 (`animation_id_t`)**

通过对《事件交互设计总表》和《动画系统总需求》进行全面梳理，我们提炼出以下**必须实现**的预设动画清单，并按类别进行划分。

##### **A. 待机/空闲动画 (Idle Animations)**

| 动画ID (animation_id_t 枚举) | 引用场景 | 动画描述 (面向设计师) |
| :--- | :--- | :--- |
| **`ANIMATION_IDLE_BREATHING_BLINK`** | 默认待机 | 一个非常微妙的、模拟呼吸的、带有缓慢不对称眨眼的基础动画。 |
| **`ANIMATION_IDLE_LOOK_AROUND`** | 待机随机 | 眼睛好奇地、缓慢地向左看看，停留，再向右看看，然后恢复。 |
| **`ANIMATION_IDLE_DAYDREAM_LOOK_UP`**| 待机随机 (稀有) | 眼睛缓慢地向上看，仿佛在凝望天花板发呆。 |

##### **B. 社交/情感反应动画 (Reaction Animations)**

| 动画ID | 引用场景 | 动画描述 |
| :--- | :--- | :--- |
| **`ANIMATION_QUICK_BLINK`** | 轻抚头部 | 一次非常快速、表示“收到”的眨眼。 |
| **`ANIMATION_HAPPY_WIGGLE`** | 开心反应 | 眼睛弯成`^^`，并伴有轻微的上下晃动，表达开心。 |
| **`ANIMATION_BLINKING_HAPPILY`**| 开心反应 | 持续的、愉快的、比平时更频繁的眨眼。 |
| **`ANIMATION_CONTENT_EYES_CLOSED_SLOWLY`**| 开心平静 | 眼睛非常缓慢、满足地闭上。 |
| **`ANIMATION_MELTING_HAPPY`** | 被持续抚摸 | 眼睛完全眯成一条线，脸上泛起红晕，表达“幸福到融化”。 |
| **`ANIMATION_DEEP_SLEEP_EYES`** | 开心平静 | 眼睛闭上，并出现`Zzz`的睡眠符号，表达极度放松和安心。 |
| **`ANIMATION_ANNOYED_GLARE`** | 烦躁反应 | 眉毛皱起，眼神变得犀利，瞪着某个方向。 |
| **`ANIMATION_SQUINT_EYES`** | 烦躁反应 | 眼睛眯起，表达“不适”或“不耐烦”。 |
| **`ANIMATION_PLAYFUL_WINK_LEFT/RIGHT`**| 触摸身体 | 快速地、俏皮地眨一下左眼或右眼。 |
| **`ANIMATION_CURIOUS_GLANCE_LEFT/RIGHT`**| 触摸身体 | 带有好奇神情的、看向左侧或右侧的眼神。 |
| **`ANIMATION_SLOW_GLANCE_LEFT/RIGHT`** | 触摸身体 | 慵懒地、慢悠悠地看一眼左侧或右侧。 |
| **`ANIMATION_GLARE_LEFT/RIGHT`** | 触摸身体 | 愤怒地瞪向左侧或右侧。 |
| **`ANIMATION_SURPRISED_WIDE_EYES`**| 被拿起 | 眼睛瞬间睁到最大，瞳孔放大，表达惊讶。 |
| **`ANIMATION_EXCITED_STAR_EYES`**| 被拿起 | 眼睛里闪烁着星星，表达极度兴奋和期待。 |
| **`ANIMATION_SCARED_EYES`** | 被拿起 (不开心时) | 瞳孔缩小，眼睛睁大，眼角下垂，表达害怕。 |
| **`ANIMATION_BLISSFUL_SMILE`** | 被拥抱 | 眼睛弯成温柔的月牙状，脸上泛起幸福的红晕。 |
| **`ANIMATION_EYES_CLOSED_PEACEFUL`**| 被拥抱 | 眼睛安详地闭上，嘴角带着一丝微笑，表达宁静。 |
| **`ANIMATION_GIGGLE_JIGGLE`** | 被挠痒痒 | 眼睛弯成`^^`，整个面部都在开心地抖动。 |
| **`ANIMATION_CANT_STOP_LAUGHING`**| 被挠痒痒 | 眼睛笑到眯成一条缝，眼角甚至挤出“泪花”（闪光特效）。 |
| **`ANIMATION_ANNOYED_TO_LAUGHING`**| 被挠痒痒 (不开心时) | 表情从“皱眉烦躁”逐渐“破防”，最终变成忍不住的大笑。 |

##### **C. 物理/本能反应动画 (Physical Reflex Animations)**

| 动画ID | 引用场景 | 动画描述 |
| :--- | :--- | :--- |
| **`ANIMATION_SCREAM_EYES`** | 自由落体 | 眼睛变成极度惊恐的豆豆眼或螺旋眼，瞳孔剧烈收缩。 |
| **`ANIMATION_DIZZY_SPIN_EYES`** | 被剧烈晃动 | 经典的蚊香眼/旋转眼，表达天旋地转的眩晕感。 |
| **`ANIMATION_SHAKE_DIZZY_SHORT`** | 被放下 | 落地后，眼睛晃几下，眨眨眼，恢复正常，表达“回过神来”。 |
| **`ANIMATION_PANIC_FLUSTERED`**| 被倒置 | 眼睛睁大，眼神慌乱地四处乱瞟，头上冒汗（水滴特效）。 |

##### **D. 过渡/冷却动画 (Cooldown Animations)**

| 动画ID | 触发逻辑 | 动画描述 |
| :--- | :--- | :--- |
| **`ANIMATION_SMILE_FADE_OUT`** | 开心动作结束后 | 脸上的笑容或兴奋表情在1-2秒内缓慢、自然地淡出。 |
| **`ANIMATION_CALM_DOWN_SHIVER_FACE`**| 惊恐动作结束后 | 惊恐的表情消失，伴随轻微的“战栗后平复”的细节。 |
| **`ANIMATION_HUFF_FACE`** | 烦躁动作结束后 | 做出一个轻哼一声的口型，然后表情恢复正常。 |

**总结：**
总共需要实现的预设动画约为 **25-30个**。

#### **3. 技术实现要点**

*   **LVGL分层实现:** 动画系统必须基于多个独立的LVGL `lv_img`对象（`eye_left`, `eye_right`, `mouth`, `blush`等）来实现。
*   **动画描述格式:**
    *   每个动画应被定义为一个由“关键帧指令”组成的数组。
    *   一个指令的结构为：`{ uint16_t time_ms; lv_obj_t* target; animation_action_t action; void* params; }`。
    *   `target`可以指向眼睛、嘴巴等特定图层。
    *   `action`可以是更换Sprite帧、启动属性动画（如淡入淡出）、触发事件等。
*   **动画播放器 (`AnimationPlayer`):**
    *   是本模块的核心实现，负责解析动画描述数据，并通过一个高频`lv_timer`来驱动所有图层的变化。
    *   必须支持并行控制多个LVGL对象，并能在动画的特定时间点触发外部事件（如通知`action_queue`播放声音）。
*   **精灵图 (Spritesheet):**
    *   美术资源必须按**部位**（眼睛、嘴巴）提供独立的精灵图，并使用LVGL官方工具转换为C数组。

#### **4. 函数接口定义 (`animation.h`)**

```c
#ifndef SKILLS_ANIMATION_H_
#define SKILLS_ANIMATION_H_

#include "lvgl.h"

// 所有预设的、声明式的动画ID
typedef enum {
    ANIMATION_IDLE_BREATHING_BLINK,
    ANIMATION_QUICK_BLINK,
    ANIMATION_HAPPY_WIGGLE,
    // ... (包含清单中所有的ID)
} animation_id_t;

/**
 * @brief 初始化视觉动画技能模块
 *        内部会创建并管理所有脸部图层的LVGL对象，并设置动画播放器。
 * @param parent LVGL父对象，动画将被创建于此对象之上
 */
void animation_init(lv_obj_t* parent);

/**
 * @brief 【声明式接口】播放一个预设的、命名好的动画
 *        此函数会立即返回 (非阻塞)。动画将在后台播放。
 *        如果已有动画正在播放，此新动画会立即中断并替换它。
 * 
 * @param id 要播放的动画ID
 */
void animation_play(animation_id_t id);

/**
 * @brief 立即停止当前正在播放的动画
 *        通常会让面部恢复到默认的待机表情。
 */
void animation_stop(void);

#endif // SKILLS_ANIMATION_H_
```