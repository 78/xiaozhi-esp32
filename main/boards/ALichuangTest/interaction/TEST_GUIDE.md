# 事件处理策略测试指南

## 当前配置（基于 event_config.json）

### 触摸事件策略

1. **TOUCH_TAP (单击)**
   - 策略：`MERGE` 
   - 合并窗口：1500ms
   - 冷却时间：500ms
   - 说明：1.5秒内的多次点击会被合并成一个事件，处理后500ms内不接受新事件

2. **TOUCH_LONG_PRESS (长按)**
   - 策略：`COOLDOWN`
   - 冷却时间：1000ms
   - 说明：每次长按后需要等待1秒才能触发下一次

### 运动事件策略

1. **MOTION_SHAKE (摇晃)**
   - 策略：`THROTTLE`
   - 节流时间：2000ms
   - 说明：2秒内只处理第一次摇晃，后续的会被丢弃

2. **MOTION_FREE_FALL (自由落体)**
   - 策略：`IMMEDIATE`
   - 允许中断：true
   - 说明：立即处理，可以中断其他事件

## 测试场景

### 场景1：测试 MERGE 策略（连续点击）
**操作步骤：**
1. 快速点击左侧铜箔3次（1秒内完成）
2. 等待2秒

**预期日志：**
```
[接收] Event type 23 (TOUCH_TAP), strategy: MERGE
[MERGE] Event merged, total 1 events in window
[接收] Event type 23 (TOUCH_TAP), strategy: MERGE  
[MERGE] Event merged, total 2 events in window, merged count: 1
[接收] Event type 23 (TOUCH_TAP), strategy: MERGE
[MERGE] Event merged, total 3 events in window, merged count: 2
// 1.5秒后窗口结束
[处理] Event type 23 processed, touch count: 3
```

**实际效果：**
- 3次点击被合并为一个事件，touch_data.y 显示点击次数为3

### 场景2：测试 COOLDOWN 策略（连续触发）
**操作步骤：**
1. 点击左侧铜箔
2. 立即再次点击（500ms内）
3. 等待600ms后再点击

**预期日志：**
```
[接收] Event type 23 (TOUCH_TAP), strategy: COOLDOWN
[处理] Event type 23 processed
[接收] Event type 23 (TOUCH_TAP), strategy: COOLDOWN
[COOLDOWN] Event in cooldown, 400ms remaining
[丢弃] Event type 23 dropped by COOLDOWN strategy
// 600ms后
[接收] Event type 23 (TOUCH_TAP), strategy: COOLDOWN
[处理] Event type 23 processed
```

### 场景3：测试 THROTTLE 策略（运动事件）
**操作步骤：**
1. 摇晃设备
2. 1秒内继续摇晃
3. 等待2秒后再摇晃

**预期日志：**
```
[接收] Event type 17 (MOTION_SHAKE), strategy: THROTTLE
[处理] Event type 17 processed
[接收] Event type 17 (MOTION_SHAKE), strategy: THROTTLE
[THROTTLE] Event throttled, 1000ms remaining
[丢弃] Event type 17 dropped by THROTTLE strategy
// 2秒后
[接收] Event type 17 (MOTION_SHAKE), strategy: THROTTLE
[THROTTLE] Event allowed after 2000ms
[处理] Event type 17 processed
```

### 场景4：混合测试（多种策略同时工作）
**操作步骤：**
1. 快速点击左侧3次
2. 同时点击右侧2次
3. 摇晃设备

**预期日志：**
- 左侧3次点击被合并
- 右侧2次点击也被合并（如果在同一窗口）
- 摇晃事件独立处理（THROTTLE策略）

## 日志级别说明

- **ESP_LOGI**（信息级）：
  - `[接收]`：接收到新事件
  - `[处理]`：事件被成功处理
  - `[MERGE]`：事件被合并
  - `[COOLDOWN]`：冷却期拒绝

- **ESP_LOGW**（警告级）：
  - `[丢弃]`：事件被策略丢弃

- **ESP_LOGD**（调试级）：
  - 策略内部详细信息
  - 时间计算细节

## 统计信息

每个事件类型都有独立的统计：
- `processed_count`：成功处理的事件数
- `dropped_count`：被丢弃的事件数  
- `merged_count`：被合并的事件数

可以通过以下方式获取：
```cpp
auto stats = event_engine_->GetEventStats(EventType::TOUCH_TAP);
ESP_LOGI(TAG, "TAP stats - processed: %ld, dropped: %ld, merged: %ld",
         stats.processed_count, stats.dropped_count, stats.merged_count);
```

## 配置调整

### 动态调整策略
```cpp
// 改变触摸点击为立即处理
EventProcessingConfig config;
config.strategy = EventProcessingStrategy::IMMEDIATE;
event_engine_->ConfigureEventProcessing(EventType::TOUCH_TAP, config);

// 改变为防抖策略
config.strategy = EventProcessingStrategy::DEBOUNCE;
config.interval_ms = 300;
event_engine_->ConfigureEventProcessing(EventType::TOUCH_TAP, config);
```

### 通过配置文件调整
修改 `/spiffs/event_config.json` 或 `event_config.json` 文件，重启后生效。

## 问题排查

1. **事件没有响应**
   - 检查是否在冷却期：查看 `[COOLDOWN]` 日志
   - 检查是否被节流：查看 `[THROTTLE]` 日志
   - 检查合并窗口：查看 `[MERGE]` 日志

2. **事件响应过于频繁**
   - 增加 `interval_ms` 值
   - 改用 THROTTLE 或 COOLDOWN 策略

3. **多次点击只响应一次**
   - 检查 MERGE 策略的 `merge_window_ms`
   - 可能需要缩短合并窗口或改用其他策略

## 性能监控

日志会显示每个策略的处理时间和状态，帮助优化配置：
- 合并窗口是否太长导致响应延迟
- 冷却时间是否影响用户体验
- 节流是否丢失重要事件