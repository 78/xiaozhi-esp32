# 日程提醒功能使用指南

## 概述

日程提醒功能为 xiaozhi-esp32 项目添加了定时提醒功能，支持单次和重复提醒，可以通过 MCP 协议远程管理。

## 功能特性

- ✅ 单次和重复提醒
- ✅ 通过 MCP 协议远程管理
- ✅ 线程安全设计
- ✅ 持久化存储
- ✅ 完整的错误处理
- ✅ 与现有通知系统集成

## 配置方法

### 1. 启用 MCP 协议

首先需要确保 MCP 协议已启用：

```bash
idf.py menuconfig
```

导航到：
```
Component config → MCP Server Configuration
```

配置选项：
- [*] **Enable MCP Server** - 启用 MCP 服务器（必需）
- **MCP Server Port** - MCP 服务器端口（默认 8080）
- [*] **Enable MCP Tools** - 启用 MCP 工具（必需）

### 2. 启用日程提醒功能

继续在 menuconfig 中配置：

```
Component config → Schedule Reminder Features
```

配置选项：
- [ ] **Enable Schedule Reminder** - 启用日程提醒功能（默认关闭）
- **Maximum schedule items** - 最大日程数量（默认20，范围1-50）
- **Schedule check interval (seconds)** - 检查间隔（默认30秒，范围10-300）
- [*] **Enable Schedule MCP Tools** - 启用 MCP 管理工具（默认开启）

### 3. 编译项目

```bash
idf.py build
idf.py flash monitor
```

### 4. 理解 MCP 架构

在 xiaozhi-esp32 项目中，**MCP 服务器是运行在 ESP32 设备上的**，不需要在您的电脑上部署额外的服务器软件。

**架构说明：**
- **MCP 服务器**：运行在 ESP32 设备上（您编译的固件）
- **MCP 客户端**：运行在您的电脑上（浏览器、命令行工具等）
- **通信方式**：通过 WebSocket 或 HTTP 协议

### 5. 连接 MCP 客户端

设备启动后，可以通过以下方式连接 MCP 客户端：

#### 方法一：使用浏览器开发者工具（推荐）

1. **打开浏览器**：Chrome、Firefox 或 Edge
2. **打开开发者工具**：按 F12 或右键 → 检查
3. **切换到 Console 标签**
4. **执行以下 JavaScript 代码**：

```javascript
// 替换为您的 ESP32 设备 IP 地址
const deviceIP = '192.168.1.100'; // 修改为实际设备 IP
const ws = new WebSocket(`ws://${deviceIP}:8080/mcp`);

ws.onopen = function() {
    console.log('MCP 连接已建立');
    
    // 发送日程管理命令
    const message = {
        type: 'mcp',
        payload: {
            tool: 'schedule.add',
            parameters: {
                title: "测试提醒",
                description: "这是一个测试提醒",
                trigger_time: Math.floor(Date.now() / 1000) + 3600, // 1小时后
                recurring: false
            }
        }
    };
    
    ws.send(JSON.stringify(message));
    console.log('命令已发送');
};

ws.onmessage = function(event) {
    console.log('收到响应:', event.data);
};

ws.onerror = function(error) {
    console.error('连接错误:', error);
};
```

#### 方法二：使用命令行工具

```bash
# 替换为您的 ESP32 设备 IP 地址
DEVICE_IP="192.168.1.100"  # 修改为实际设备 IP

# 使用 curl 发送 MCP 命令
curl -X POST http://${DEVICE_IP}:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "type": "mcp",
    "payload": {
      "tool": "schedule.add",
      "parameters": {
        "title": "测试提醒",
        "description": "这是一个测试提醒",
        "trigger_time": 1730822400,
        "recurring": false
      }
    }
  }'
```

#### 方法三：使用专门的 WebSocket 客户端工具

**推荐工具：**
1. **WebSocket King**（Chrome 扩展）
   - 下载：Chrome 网上应用店搜索 "WebSocket King"
   - 使用：输入 `ws://设备IP:8080/mcp` 连接

2. **Postman**（支持 WebSocket）
   - 下载：https://www.postman.com/downloads/
   - 使用：新建 WebSocket 请求

3. **wscat**（命令行工具）
   ```bash
   # 安装
   npm install -g wscat
   
   # 连接
   wscat -c ws://设备IP:8080/mcp
   ```

### 6. 获取设备 IP 地址

设备启动后，在串口监视器中查看日志：

```bash
idf.py monitor
```

查找类似这样的日志：
```
I (1234) wifi: connected to SSID, ip: 192.168.1.100
```

**或者使用网络扫描工具：**
```bash
# 在 Linux/Mac 上
nmap -sn 192.168.1.0/24

# 在 Windows 上
arp -a
```

## 使用方式

### 1. 通过 MCP 工具管理

#### 添加日程

```json
{
  "title": "会议提醒",
  "description": "每周团队会议",
  "trigger_time": 1730822400,
  "recurring": true,
  "repeat_interval": 604800
}
```

**参数说明：**
- `title` (必需): 提醒标题
- `description` (可选): 详细描述
- `trigger_time` (必需): 触发时间（Unix 时间戳）
- `recurring` (可选): 是否重复（默认 false）
- `repeat_interval` (可选): 重复间隔（秒）

#### 列出所有日程

```json
{}
```

#### 删除日程

```json
{
  "id": "1730822400"
}
```

#### 更新日程

```json
{
  "id": "1730822400",
  "title": "更新后的会议提醒",
  "trigger_time": 1730908800,
  "enabled": true
}
```

### 2. 通过代码 API 使用

#### 初始化

```cpp
#include "features/schedule_reminder/schedule_reminder.h"

auto& schedule_reminder = ScheduleReminder::GetInstance();
if (!schedule_reminder.Initialize()) {
    ESP_LOGE(TAG, "日程提醒初始化失败");
    return;
}
```

#### 添加日程

```cpp
ScheduleItem item;
item.id = std::to_string(time(nullptr));
item.title = "吃药提醒";
item.description = "记得按时吃药";
item.trigger_time = time(nullptr) + 3600; // 1小时后
item.recurring = true;
item.repeat_interval = 86400; // 每天重复
item.created_at = std::to_string(time(nullptr));

ScheduleError result = schedule_reminder.AddSchedule(item);
switch (result) {
    case ScheduleError::kSuccess:
        ESP_LOGI(TAG, "日程添加成功");
        break;
    case ScheduleError::kMaxItemsReached:
        ESP_LOGE(TAG, "达到最大日程数量限制");
        break;
    case ScheduleError::kDuplicateId:
        ESP_LOGE(TAG, "日程ID重复");
        break;
    // ... 其他错误处理
}
```

#### 设置提醒回调

```cpp
schedule_reminder.SetReminderCallback([](const ScheduleItem& item) {
    // 使用系统 Alert 显示提醒
    char message[256];
    if (item.description.empty()) {
        snprintf(message, sizeof(message), "提醒: %s", item.title.c_str());
    } else {
        snprintf(message, sizeof(message), "提醒: %s - %s", 
                 item.title.c_str(), item.description.c_str());
    }
    
    // 调用系统通知
    Alert("日程提醒", message, "bell", Lang::Sounds::OGG_NOTIFICATION);
    
    ESP_LOGI(TAG, "日程触发: %s", item.title.c_str());
});
```

#### 其他操作

```cpp
// 获取所有日程
auto schedules = schedule_reminder.GetSchedules();

// 获取特定日程
ScheduleItem* item = schedule_reminder.GetSchedule("schedule_id");

// 删除日程
ScheduleError result = schedule_reminder.RemoveSchedule("schedule_id");

// 更新日程
ScheduleError result = schedule_reminder.UpdateSchedule("schedule_id", updated_item);
```

## 使用场景示例

### 1. 每日提醒

```json
{
  "title": "晨间锻炼",
  "description": "每天早晨锻炼身体",
  "trigger_time": 1730822400,
  "recurring": true,
  "repeat_interval": 86400
}
```

### 2. 每周会议

```json
{
  "title": "周会",
  "description": "每周团队会议",
  "trigger_time": 1730822400,
  "recurring": true,
  "repeat_interval": 604800
}
```

### 3. 一次性事件

```json
{
  "title": "生日提醒",
  "description": "朋友的生日聚会",
  "trigger_time": 1730822400,
  "recurring": false
}
```

### 4. 定时任务

```json
{
  "title": "浇花提醒",
  "description": "给阳台的花浇水",
  "trigger_time": 1730822400,
  "recurring": true,
  "repeat_interval": 172800  // 每2天
}
```

## 错误处理

### 错误码说明

```cpp
enum class ScheduleError {
    kSuccess = 0,           // 操作成功
    kMaxItemsReached,       // 达到最大日程数量
    kDuplicateId,           // 日程ID重复
    kInvalidTime,           // 无效的触发时间
    kStorageError,          // 存储错误
    kNotFound,              // 日程未找到
    kNotInitialized         // 未初始化
};
```

### 错误处理示例

```cpp
ScheduleError result = schedule_reminder.AddSchedule(item);
if (result != ScheduleError::kSuccess) {
    switch (result) {
        case ScheduleError::kMaxItemsReached:
            // 处理达到最大数量
            break;
        case ScheduleError::kInvalidTime:
            // 处理时间错误
            break;
        case ScheduleError::kStorageError:
            // 处理存储错误
            break;
        default:
            // 处理其他错误
            break;
    }
}
```

## 技术细节

### 存储格式

日程数据以 JSON 格式存储在系统 Settings 中：

```json
{
  "version": 1,
  "schedules": [
    {
      "id": "1730822400",
      "title": "会议提醒",
      "description": "每周团队会议",
      "trigger_time": 1730822400,
      "enabled": true,
      "recurring": true,
      "repeat_interval": 604800,
      "created_at": "1730822400"
    }
  ]
}
```

### 线程安全

所有公共方法都使用互斥锁保护，确保多线程环境下的数据安全。

### 性能考虑

- **检查间隔**：默认30秒，可根据需要调整
- **内存使用**：每个日程约占用200-300字节
- **CPU 使用**：检查逻辑轻量，对系统影响小

## 常见问题

### Q: 如何计算 Unix 时间戳？
A: 可以使用在线工具或编程语言的时间函数计算。例如在 Python 中：
```python
import time
timestamp = int(time.time()) + 3600  # 1小时后
```

### Q: 重复间隔的单位是什么？
A: 单位为秒。常用值：
- 3600 = 1小时
- 86400 = 1天
- 604800 = 1周

### Q: 如何禁用某个提醒？
A: 使用更新工具将 `enabled` 字段设为 false。

### Q: 最大支持多少个日程？
A: 默认20个，可在配置中调整到最多50个。

### Q: 数据会持久化吗？
A: 是的，所有日程都会保存到 flash 中，重启后仍然有效。

## 故障排除

### 1. 提醒未触发
- 检查设备时间是否正确
- 确认日程的 `enabled` 字段为 true
- 查看日志确认检查间隔是否正常

### 2. MCP 工具无法使用
- 确认 `ENABLE_SCHEDULE_MCP_TOOLS` 已启用
- 检查网络连接状态
- 查看 MCP 协议日志

### 3. 存储错误
- 检查 flash 存储空间
- 查看系统 Settings 日志
- 尝试重启设备

## 版本历史

- v1.0: 初始版本
  - 基础日程管理功能
  - MCP 工具支持
  - 持久化存储

---

**注意**: 使用前请确保已正确配置并编译项目。如有问题，请查看系统日志获取详细信息。
