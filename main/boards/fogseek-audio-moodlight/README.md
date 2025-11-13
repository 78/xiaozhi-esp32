## 产品简介

雾岸科技 MoodLight 是一款智能氛围灯，具有以下特点：

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

## 快速构建

推荐使用以下命令一键构建固件，该方式会自动应用所有板子特定配置：

```bash
python scripts/release.py fogseek-audio-moodlight
```

此命令会自动完成以下操作：
1. 设置目标芯片为 ESP32-S3
2. 应用 [config.json](config.json) 中定义的配置选项
3. 编译项目
4. 生成固件包并保存到 [releases/](file:///home/lee/xiaozhi-esp32/releases/) 目录

构建完成后，固件包将生成在项目根目录的 [releases/](file:///home/lee/xiaozhi-esp32/releases/) 文件夹中。

## 手动构建（可选）

如果你希望手动配置和构建项目，可以按照以下步骤操作：

1. **配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

2. **打开 menuconfig 并选择板子：**

```bash
idf.py menuconfig
```

3. **在 menuconfig 中选择：**
```
Xiaozhi Assistant -> Board Type -> 雾岸科技 MoodLight
```

4. **编译：**

```bash
idf.py build
```

## 应用场景

这款开发板适用于以下应用场景：
- 智能家居氛围灯
- 情绪调节照明设备
- 语音控制照明系统
- 个性化装饰灯