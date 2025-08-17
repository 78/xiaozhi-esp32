## 产品简介

FogSeek品牌情绪灯是一款智能氛围灯，具有以下特点：

## 主要功能

1. **冷暖色温调节**：独立控制冷光(47 GPIO)和暖光(48 GPIO)LED，实现色温调节
2. **情绪感知照明**：根据用户语音指令和情绪状态自动调节灯光效果
3. **音频处理**：支持语音交互功能
4. **电池管理**：内置电池充电管理功能
5. **WiFi连接**：支持WiFi网络连接


## 硬件特性

- ESP32-S3主控芯片
- 冷暖双色LED灯带
- 电源按键
- 红色和绿色状态指示灯
- 锂电池充电管理
- 音频输入输出接口

## MCP工具

设备通过MCP协议提供以下控制接口：

### 灯光控制工具

| 工具名称 | 描述 | 参数 |
|---------|------|------|
| `self.light.get_status` | 获取当前灯的状态 | 无 |
| `self.light.set_brightness` | 设置冷暖灯光亮度 | `cold_brightness`(0-100), `warm_brightness`(0-100) |

## 编译配置命令

**配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> 雾岸科技 MoodLight
```

**编译：**

```bash
idf.py build
```

