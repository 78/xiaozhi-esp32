
---

### **`event_engine.cc` 增量式重构与功能增强计划**

**前提:** 您当前有一个能工作的`event_engine.cc`，它包含了所有的IMU事件检测逻辑。

---
#### **Step 1: 引入“按类型冷却”机制 (提升逻辑健壮性)**

*   **目标:** 替换掉全局的`DEBOUNCE_TIME_US`，避免关键事件被错误抑制。
*   **子步骤:**
    1.  **添加成员变量:** 在`event_engine.h`中，将`int64_t last_event_time_us_;`替换为`std::unordered_map<EventType, int64_t> last_event_times_;`。
    2.  **修改`ProcessMotionDetection()`:**
        *   **移除**函数开头的全局去抖检查代码块。
        *   在**每一个**`if (Detect...())`的判断条件中，**增加**对该事件类型自身冷却时间的检查。例如：
            ```cpp
            // 旧: if (DetectFreeFall(...))
            // 新:
            if (DetectFreeFall(...) && 
                (current_time - last_event_times_[EventType::MOTION_FREE_FALL] > FREE_FALL_COOLDOWN_US)) 
            ```
        *   在**每一个**事件被成功触发并分派**之前**，更新它自己的时间戳。
            ```cpp
            if (motion_type != EventType::MOTION_NONE) {
                last_event_times_[motion_type] = current_time; // 在这里更新
                // last_event_time_us_ = current_time; // 删除这行
                DispatchEvent(event);
            }
            ```
    3.  **定义冷却时间:** 在`.h`或`.cc`的顶部，为不同的事件类型定义不同的冷却宏，如`#define FREE_FALL_COOLDOWN_US (500 * 1000)`。
*   **测试:**
    *   **编译并烧录。**
    *   **验证:** 快速连续地触发**不同**的动作。例如，先做一个`SHAKE`，然后立刻模拟一个`FREE_FALL`（快速将设备向下移动再停住）。您应该能看到`FREE_FALL`事件被**立即触发**，而不会被`SHAKE`的冷却时间所阻塞。
    *   **回归测试:** 验证之前的单个事件检测依然正常。

---
#### **Step 2: 封装“IMU稳定性”判据 (提升代码可维护性)**

*   **目标:** 消除重复代码，将“稳定”的定义集中管理。
*   **子步骤:**
    1.  **创建私有辅助函数:** 在`event_engine.h`中，声明一个`private:`辅助函数`bool IsStable(const ImuData& data, const ImuData& last_data);`。
    2.  **实现该函数:** 在`event_engine.cc`中，将之前`DetectPickup`和`DetectUpsideDown`中的稳定性判断逻辑**合并并移动**到这个新函数中。
    3.  **替换调用:** 在`DetectPickup`和`DetectUpsideDown`中，将原来的多行判断代码，替换为一行简单的`if (IsStable(current_imu_data_, last_imu_data_))`。
*   **测试:**
    *   **编译并烧录。**
    *   **验证:** 重点测试**拿起**和**倒置**这两个功能。确保它们的行为与重构前**完全一致**。这个步骤不应该引入任何功能上的变化。

---
#### **Step 3: 架构解耦 - 剥离`motion_engine` (为扩展做准备)**

*   **目标:** 将所有纯IMU相关的逻辑，从`EventEngine`中分离出去，为后续加入`touch_engine`做好准备。
*   **子步骤:**
    1.  **创建新文件:** 新建`motion_engine.cc`和`motion_engine.h`。
    2.  **代码“搬家”:**
        *   将`event_engine.h`中所有与IMU数据、检测函数（如`DetectPickup`, `IsStable`）和IMU相关的成员变量（如`imu_`, `in_free_fall_`等），**剪切并粘贴**到新的`MotionEngine`类中。
        *   同样地，将`event_engine.cc`中这些函数的**实现**也剪切并粘贴到`motion_engine.cc`中。
    3.  **定义接口:** 在`motion_engine.h`中，定义与`EventEngine`类似的回调注册接口`void RegisterCallback(MotionEventCallback callback);`。`MotionEngine`的`Process`函数现在只负责检测纯运动事件，并通过回调发布它们。
    4.  **修改`EventEngine`:**
        *   `EventEngine`现在变得非常“瘦”。**移除**所有刚刚搬走的IMU相关代码。
        *   在`Initialize`函数中，不再接收`Qmi8658* imu`，而是接收一个`MotionEngine* motion_engine`。
        *   `EventEngine`需要实现一个回调函数`OnMotionEvent`，并通过`motion_engine->RegisterCallback()`将其注册进去。
        *   `EventEngine`的`Process()`函数现在**暂时为空**，或者只负责调用`motion_engine->Process()`。
*   **测试:**
    *   **编译并烧录。**
    *   **验证:** 测试所有IMU事件。此时，系统的**外部行为应该与重构前完全一致**。虽然我们对代码结构做了巨大调整，但功能上不应有任何退步。

---
#### **Step 4: 实现并集成`touch_engine` (新增触摸功能)**

*   **目标:** 独立开发触摸事件解释器，并将其作为新的事件源集成到系统中。
*   **子步骤:**
    1.  **独立开发`touch_engine`:**
        *   创建`touch_engine.cc/.h`。
        目前有touch左和touch右，对应于GPIO10和GPIO11。
        * 实现`touch_engine_init`和`touch_task`，专注于将GPIO的`ON/OFF`解释为`SINGLE_TAP`和`HOLD`事件，并通过回调发布。
    2.  **编写独立的测试程序:** 创建一个临时的`main_touch_test.c`，只初始化和调用`touch_engine`。在回调函数中打印出接收到的触摸事件，**确保`touch_engine`自身工作正常**。
    3.  **集成到`EventEngine`:**
        *   在`EventEngine`的`Initialize`中增加一个`TouchEngine* touch_engine`参数。
        *   让`EventEngine`也去订阅`touch_engine`的事件回调 (`OnTouchEvent`)。
        *   现在，`EventEngine`可以同时接收来自`MotionEngine`和`TouchEngine`的事件了。

*   **测试:**
    *   **编译并烧录完整固件。**
    *   **验证:**
        *   触摸头部、左侧、右侧，你应该能在串口日志中看到`EventEngine`打印出接收到的`SINGLE_TAP`或`HOLD`事件。
        *   晃动设备，IMU事件应该也依然正常。此时，我们已经成功地将两个独立的事件源整合到了一起。

---
#### **Step 5: 实现复合事件融合 (最终的魔法)**

*   **目标:** 在`EventEngine`中，实现我们PRD中定义的、真正需要融合两种传感器信息的复合事件。
*   **子步骤:**
    1.  **实现短期事件缓冲区:** 在`EventEngine`中，添加一个`std::vector`或环形缓冲区，用于存储最近几秒内收到的所有低阶事件（来自`OnMotionEvent`和`OnTouchEvent`）。
    2.  **修改`Process()`函数:** `EventEngine`的`Process()`函数现在有了真正的职责：周期性地（在`event_engine_task`中被调用）扫描这个缓冲区，并应用规则。
    3.  **实现规则:**
        *   **`EVENT_PICKED_UP`:** 扫描缓冲区，查找是否在短时间内同时存在`MOTION_LIFT_UP`（由`motion_engine`生成）和`TOUCH`事件。
        *   **`EVENT_TICKLED`:** 扫描缓冲区，计算短时间内的`SINGLE_TAP`事件数量。
        *   ...实现PRD中定义的所有其他复合事件规则。
    4.  **发布高级事件:** 当一个复合事件被识别后，`EventEngine`通过它自己的回调，向最终的应用层（`reaction_engine`等）发布这个**高级事件**。

*   **测试:**
    *   **这是最激动人心的测试！**
    *   **验证:**
        *   先触摸身体，再拿起设备 -> 日志中应该打印出`EVENT_PICKED_UP`。
        *   快速、无规律地触摸所有传感器 -> 日志中应该打印出`EVENT_TICKLED`。
        *   **回归测试:** 确保之前的简单事件（如单独的`SHAKE`或`TOUCH_SINGLE`）仍然能被正确传递。

通过以上五个增量步骤，您可以将现有代码安全、平稳地重构为一个**高度模块化、功能强大且易于扩展**的智能事件系统，并在此过程中不断进行测试和验证，确保每一步都在正确的轨道上。