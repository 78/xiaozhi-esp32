好的，遵照同样的方法，我们来为**`vibration.h`**模块制定一份详尽的、由上层体验需求驱动的**开发需求文档 (PRD)**。

---

### **`vibration.h` 模块开发需求文档 (PRD)**

#### **1. 模块概述**

`vibration.h`是原子技能层中负责**所有触觉反馈**的核心模块。它通过控制振动马达，为用户的物理交互和设备的情感表达提供丰富的体感。

模块的核心是提供一个**声明式接口 (`vibration_play`)**，用于调用预设的、命名好的、富有节奏感的振动模式。所有振动模式的播放都必须是**异步非阻塞**的，通过一个专门的FreeRTOS后台任务 (`vibration_task`) 来实现，以确保不会影响主程序的响应性。

#### **2. 预设振动模式清单 (`vibration_id_t`)**

通过对《事件交互设计总表》中所有反应池的`vibrate`调用进行全面梳理，我们提炼出以下**必须实现**的预设振动模式清单。

| 振动ID (vibration_id_t 枚举) | 引用场景 | 振动描述 (面向设计师) | 技术实现要点 (面向工程师) |
| :--- | :--- | :--- | :--- |
| **`VIBRATION_SHORT_BUZZ`** | 轻抚头部 | 一次非常短促、清脆的“嗡”声，作为快速的确认反馈。 | 单次、高强度、持续约50-100ms的振动。 |
| **`VIBRATION_PURR_SHORT`** | 轻抚头部 | 模拟猫咪满足时发出的、短促的“咕噜”声。 | 低强度、带有轻微频率变化的、持续约300-500ms的振动。 |
| **`VIBRATION_PURR_PATTERN`** | 按住头部 | 持续的、令人舒适的“咕噜咕噜”声。 | 连续的、低强度、带有轻微频率变化的循环振动。 |
| **`VIBRATION_GENTLE_HEARTBEAT`** | 按住头部 / 被拥抱 | 模拟缓慢、平稳的心跳，传递温暖和安全感。 | 两次短促的中等强度振动为一组，组间有较长的停顿，如 `(强, 弱) --- (强, 弱) ---`。 |
| **`VIBRATION_STRUGGLE_PATTERN`** | 按住头部 / 被倒置 | 表达“挣扎”的、不规则、用力的振动。 | 强度和间隔都无规律的、强烈的脉冲式振动。 |
| **`VIBRATION_SHARP_BUZZ`** | 轻触身体 | 表达“被打扰”或“警告”的、尖锐的振动。 | 单次、高强度、但比`SHORT_BUZZ`稍长的振动，约150ms。 |
| **`VIBRATION_TREMBLE_PATTERN`** | 被拿起 (不开心时) | 表达“害怕”的、轻微但持续的颤抖。 | 持续的、极低强度的、高频的微小振动。 |
| **`VIBRATION_GIGGLE_PATTERN`** | 被挠痒痒 | 模拟“笑到发抖”的、欢快的、有节奏的连续振动。 | 一系列快速、短促、中等强度的振动，节奏欢快。 |
| **`VIBRATION_HEARTBEAT_STRONG`** | (掌心的约定) | 表达“力量”和“信念”的、缓慢、深沉、有力的心跳。 | `GENTLE_HEARTBEAT`的变种，但强度更高，节奏更慢、更庄重。 |
| **`VIBRATION_ERRATIC_STRONG`** | 被剧烈晃动 | 表达“眩晕”的、混乱、强烈的振动。 | 强度和间隔都混乱的、非常强烈的脉冲式振动，比`STRUGGLE`更无序。 |

**总结：**
总共需要实现的预设振动模式为 **10个**。

#### **3. 技术实现要点**

*   **数据结构:**
    *   每个振动模式应被定义为一个由多个“振动关键帧”组成的数组。
    *   一个关键帧的结构为：`{ uint8_t strength; uint16_t duration_ms; }`，分别代表该步骤的振动强度(0-255)和持续时间(毫秒)。
    *   这些数组应被定义为`static const`，存储在Flash中以节省RAM。

*   **后台任务 (`vibration_task`):**
    *   该任务的核心是一个循环，它会阻塞等待一个来自消息队列的`vibration_id_t`。
    *   收到ID后，任务会根据ID查找对应的关键帧数组，并按顺序遍历。
    *   在每一步，它会调用底层驱动设置PWM占空比（强度），然后使用`vTaskDelay`来等待相应的持续时间。
    *   整个模式播放完毕后，确保将振动强度设为0，然后返回循环等待下一个指令。

#### **4. 函数接口定义 (`vibration.h`)**

```c
#ifndef SKILLS_VIBRATION_H_
#define SKILLS_VIBRATION_H_

#include <stdint.h>

// 所有预设的、声明式的振动模式ID
typedef enum {
    VIBRATION_SHORT_BUZZ,
    VIBRATION_PURR_SHORT,
    VIBRATION_PURR_PATTERN,
    VIBRATION_GENTLE_HEARTBEAT,
    VIBRATION_STRUGGLE_PATTERN,
    VIBRATION_SHARP_BUZZ,
    VIBRATION_TREMBLE_PATTERN,
    VIBRATION_GIGGLE_PATTERN,
    VIBRATION_HEARTBEAT_STRONG,
    VIBRATION_ERRATIC_STRONG,
    // ...可继续扩展
} vibration_id_t;

/**
 * @brief 初始化触觉振动技能模块
 *        内部会创建并启动一个后台FreeRTOS任务来处理所有振动模式的播放
 */
void vibration_init(void);

/**
 * @brief 【声明式接口】播放一个预设的、命名好的振动模式
 *        此函数会立即返回 (非阻塞)。振动模式将在后台任务中播放。
 *        如果后台任务正在播放上一个模式，此新请求会进入队列排队等待。
 *        (可根据产品需求调整为可中断模式)
 * 
 * @param id 要播放的振动模式ID
 */
void vibration_play(vibration_id_t id);

/**
 * @brief 立即停止当前所有正在播放或排队的振动模式
 */
void vibration_stop(void);

#endif // SKILLS_VIBRATION_H_
```

这份PRD为`vibration.h`模块提供了清晰的设计蓝图。它明确了需要实现的**10个核心振动模式**，定义了**技术实现的关键路径**，并提供了一份**标准化的头文件接口**。开发工程师可以基于此进行独立的模块开发和测试，而上层应用开发者则可以同步地在他们的逻辑中调用这些接口。