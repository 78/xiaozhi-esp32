## 目标
- 保持短按逻辑：按下→释放后，根据按下时长与防抖判定短按并执行
- 改造长按逻辑：当按住时间超过 `KEY_LONG_PRESS_TIME_MS`，无需等待释放，立即执行长按业务代码，且只触发一次

## 现状
- 事件模型：按键电平变化时向队列 `key_event_queue_` 发送 `PRESS/RELEASE` 事件（ai_martube_esp32s3.cc:1299–1314）
- 处理函数：`ProcessKeyEvent()` 仅在收到 `RELEASE` 时计算 `press_duration` 判定长按或短按（ai_martube_esp32s3.cc:374–407）
- 问题：按住期间没有新事件，因而无法在超过阈值时即时触发长按

## 改造方案
1. 状态变量
- 新增 `bool key_long_press_triggered_`，表示当前按下周期中长按是否已触发；在初始化与按键释放时复位

2. 处理流程更新（修改 `ProcessKeyEvent()`）
- 事件处理：保留原有 `PRESS/RELEASE` 入队与处理逻辑
- 即时长按检测：在处理完队列后，若 `key_is_pressed_==true` 且 `!key_long_press_triggered_` 且 `now - key_press_time_ >= KEY_LONG_PRESS_TIME_MS*1000`，立即调用 `on_long_press_callback_()` 并置 `key_long_press_triggered_=true`
- 释放处理：
  - 若 `key_long_press_triggered_==true`，释放时不再执行短按或长按（避免二次触发），仅复位 `key_is_pressed_` 与 `key_long_press_triggered_`
  - 若 `key_long_press_triggered_==false`，按原逻辑判定短按（`press_duration >= KEY_DEBOUNCE_TIME_MS*1000`）

3. 初始化
- 在 `InitializeKeyInput()` 处将 `key_long_press_triggered_` 初始化为 `false`

## 验证
- 短按：按下后快速释放，打印“Short press detected”，业务回调执行一次
- 长按：持续按住超过阈值，立刻打印“Long press detected”并执行业务；松开时不重复触发任何动作
- 防抖：短按仍受 `KEY_DEBOUNCE_TIME_MS` 限制；长按越过阈值才触发

## 改动文件与位置
- `main/boards/ai-martube-esp32s3/ai_martube_esp32s3.cc`
  - 增加成员 `key_long_press_triggered_`（与 `key_is_pressed_` 同区域，约行 100–110）
  - 初始化于 `InitializeKeyInput()`（ai_martube_esp32s3.cc:356–364）
  - 更新 `ProcessKeyEvent()` 判定与即时触发（ai_martube_esp32s3.cc:374–407），并在队列处理之后加入“按住中的即时长按检测”分支

## 兼容性
- 不影响关机按键 `ProcessShutdownEvent()` 的逻辑；仅改 `KEY_INPUT_GPIO` 的通用按键行为

确认后我将按上述方案修改代码并提供测试日志输出以便你验证。