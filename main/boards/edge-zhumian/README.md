# 雾岸科技 BubblePal 开发板

## 产品概述

FogSeek BubblePal 是基于 fogseek-edge 开发板的产品，集成了全彩 LED 灯光源板，可以根据用户的场景和对话情绪显示出非常丰富的全彩灯光效果。

## 架构设计

### 核心板
核心板集成了以下关键组件：
- **ESP32S3 主控芯片**：高性能主控单元
- **16MB Flash + 8MB PSRAM**：大容量存储空间
- **WiFi/BLE 无线通信**：支持多种无线连接方式
- **电源管理**：高效能电源管理系统
- **音频编解码 ES8311**：集成板载麦克风和 3W 音频功放
- **基础交互**：开关/功能按键、LED 指示灯
- **基础接口**：麦克风咪头引出、扬声器接口、锂电池接口、FPC 排线接口、Type-C 接口

### 扩展板
扩展板通过 FPC 排线与核心板连接，BubblePal 配置提供以下特定功能：
- **全彩 LED 灯光源板**：支持丰富的 RGB 灯光效果
- **双色 LED 指示灯**：红色和绿色 LED 指示灯
- **用户按键**：BOOT 按键 (GPIO0) 和控制按键 (GPIO15)
- **电源管理接口**：电源保持、充电状态检测、电池电压检测
- **音频接口**：I2C 控制接口、I2S 音频接口

## 设计理念

1. **模块化设计**：核心板与扩展板分离，实现功能的灵活组合
2. **最小化设计**：核心板追求最小化、稳定性和低成本生产
3. **极致灵活性**：扩展板可根据具体应用场景集成各类传感器、执行器和接口
4. **快速响应**：支持快速迭代和市场需求变化

## 使用方法

### 快速构建
```bash
python scripts/release.py fogseek-edge-bubblepal
```

### menuconfig 配置
在菜单配置中选择 "雾岸科技 BubblePal" 板子类型。

## 技术规格
- 主控芯片：ESP32S3
- 存储：16MB Flash + 8MB PSRAM
- 音频：ES8311 编解码器，3W 功放
- 通信：WiFi/BLE
- 接口：FPC 排线、Type-C、I2C 总线
- 电源：锂电池供电，支持电源管理
- 按键：BOOT 按键 (GPIO0)、控制按键 (GPIO15)
- LED：红色 LED (GPIO47)、绿色 LED (GPIO48)、全彩 LED 灯带

## 注意事项
- 核心板与扩展板通过 FPC 排线连接，请确保连接牢固
- 扩展板可根据实际需求进行定制
- 建议使用官方提供的扩展板以保证兼容性

## 硬件特性

- 主控芯片：ESP32-S3
- Flash大小：8MB
- 音频编解码器：ES8311
- 音频功放：NS4150B
- 电池充电管理
- 双色LED指示灯（红/绿）
- 两个用户按键

## 引脚分配

### 按键
- BOOT按键: GPIO0
- 控制按键: GPIO15

### 电源管理
- 电源保持: GPIO39
- 充电完成检测: GPIO21
- 充电中检测: GPIO18
- 电池电压检测: GPIO10 (ADC1_CH9)

### LED指示灯
- 红色LED: GPIO47
- 绿色LED: GPIO48

### 音频接口
- I2C时钟线: GPIO8
- I2C数据线: GPIO9
- I2S主时钟: GPIO38
- I2S位时钟: GPIO16
- I2S帧时钟: GPIO14
- I2S数据输出: GPIO13
- I2S数据输入: GPIO17
- 功放使能: GPIO41

## 快速构建

推荐使用以下命令一键构建固件，该方式会自动应用所有板子特定配置：

```bash
python scripts/release.py fogseek-edge-bubblepal
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
Xiaozhi Assistant -> Board Type -> 雾岸科技 BubblePal
```

4. **编译：**

```bash
idf.py build
```

## 应用场景
- 儿童玩具
- 情绪感知灯具
- 智能陪伴机器人
- 教育娱乐设备
- 个性化装饰灯