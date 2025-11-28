## 目标
- 保持短按逻辑：按下→释放后，根据 `SHUTDOWN_DEBOUNCE_TIME_MS` 判定短按并执行现有短按行为
- 改造长按逻辑：当按住时间超过 `SHUTDOWN_LONG_PRESS_TIME_MS`，无需等待释放，立即执行长按业务代码，且只触发一次

## 现状
- 关机按键事件通过 `shutdown_event_queue_` 以 `PRESS/RELEASE` 入队（ai_martube_esp32s3.cc:1319–1337）
- `ProcessShutdownEvent()` 仅在 `RELEASE` 时根据按下时长判定长按（ai_martube_esp32s3.cc:531–565）
- 已有短按分支用于“打断说话并进入聆听”（ai_martube_esp32s3.cc:556–559）

## 改造方案
1. 新增状态变量
- 增加 `bool shutdown_long_press_triggered_`，表示本次按下周期是否已触发过长按；在初始化与按下时重置，在释放时清理

2. 更新 `ProcessShutdownEvent()`
- 处理 `PRESS`：记录时间戳，并将 `shutdown_long_press_triggered_=false`
- 处理 `RELEASE`：
  - 若 `shutdown_long_press_triggered_==true`，释放时不再执行任何动作，仅复位状态
  - 若未触发过长按：
    - `press_duration >= SHUTDOWN_LONG_PRESS_TIME_MS*1000` → 执行长按回调
    - 否则若 `press_duration >= SHUTDOWN_DEBOUNCE_TIME_MS*1000` → 执行短按分支（已存在：打断说话并进入聆听）
- 队列处理后增加“即时长按检测”：
  - 若 `shutdown_is_pressed_==true` 且 `!shutdown_long_press_triggered_`，当 `now - shutdown_press_time_ >= SHUTDOWN_LONG_PRESS_TIME_MS*1000` 时立即调用长按回调并置标记

3. 初始化
- 在初始化处将 `shutdown_long_press_triggered_ = false`

## 验证
- 短按：快速按下释放，执行短按行为
- 长按：持续按住超过阈值，立即执行长按；松开时不重复触发

## 改动文件
- `main/boards/ai-martube-esp32s3/ai_martube_esp32s3.cc`：成员变量区、`InitializeShutdownButton()`、`ProcessShutdownEvent()` 逻辑更新

确认后我将按上述方案实现并提供日志以便验证。