# FogSeek通用组件

本目录包含FogSeek系列开发板的通用组件实现，用于避免在多个fogseek开头的开发板中重复编写相似代码。

## 目录结构

```
fogseek_common/
├── led_controller.h            # LED控制器头文件
├── led_controller.cc           # LED控制器实现
├── power_manager.h             # 电源管理器头文件
├── power_manager.cc            # 电源管理器实现
├── display_manager.h           # 显示管理器头文件
├── display_manager.cc          # 显示管理器实现
├── mcp_tools.h                 # MCP工具头文件
└── mcp_tools.cc                # MCP工具实现
```

## 设计说明

这些组件为FogSeek系列开发板提供了通用的实现，所有fogseek开头的开发板都可以直接使用这些组件，避免重复实现相似功能。

### FogSeekLedController
- 提供LED控制的通用实现
- 包含基本的闪烁功能
- 提供设备状态和电池状态的处理实现
- 支持RGB灯带控制

### FogSeekPowerManager
- 定义电源管理的通用操作实现
- 提供电源状态查询功能
- 包含电池电量监控相关实现
- 提供低电量警告和自动关机功能

### FogSeekDisplayManager
- 提供显示管理的通用实现
- 包含多种屏幕初始化命令
- 提供电池状态、设备状态和消息显示实现

### McpTools
- 提供MCP协议相关的工具函数
- 包含灯光控制等通用MCP工具实现

## 使用方法

在具体的fogseek系列开发板中，可以直接包含这些通用组件的头文件并创建实例，无需重复实现相似功能。

例如：
```cpp
#include "power_manager.h"
#include "led_controller.h"

class FogSeekBoard : public WifiBoard
{
private:
    PowerManager power_manager_;
    LedController led_controller_;
    // ...
};
```

## 维护说明

当需要修改通用功能时，只需修改本目录下的文件，所有使用这些组件的开发板都会自动获得更新。
这样可以确保代码一致性并简化维护工作。