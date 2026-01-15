# nano-glowbies AI语音玩具开发板

## 产品概述

nano-glowbies 是一款专为AI语音交互设计的智能玩具开发板，集成了全彩RGB灯带，使嘴巴和眼睛能够根据对话情绪和场景发出丰富的彩色光芒，增强与用户的互动体验。

## 架构设计

### 核心板
核心板集成了以下关键组件：
- **ESP32S3 主控芯片**：高性能主控单元
- **8MB Flash + 8MB PSRAM**：大容量存储空间
- **WiFi/BLE 无线通信**：支持多种无线连接方式
- **电源管理**：高效能电源管理系统
- **音频编解码 ES8389**：集成板载麦克风和 3W 音频功放，支持实时打断
- **基础交互**：开关/功能按键、LED 指示灯
- **基础接口**：麦克风咪头引出、扬声器接口、锂电池接口、FPC 排线接口、Type-C 接口

### 功能特色
nano-glowbies 配置提供以下特定功能：
- **全彩RGB灯带**：支持丰富的灯光效果，可控制嘴巴和眼睛发光
- **双色 LED 指示灯**：红色和绿色 LED 指示灯
- **用户按键**：BOOT 按键 (GPIO0) 和控制按键 (GPIO15)
- **电源管理接口**：电源保持、充电状态检测、电池电压检测
- **音频接口**：I2C 控制接口、I2S 音频接口

## 设计理念

1. **模块化设计**：核心板与扩展板分离，实现功能的灵活组合
2. **最小化设计**：核心板追求最小化、稳定性和低成本生产
3. **极致灵活性**：扩展板可根据具体应用场景集成各类传感器、执行器和接口
4. **快速响应**：支持快速迭代和市场需求变化
5. **情感化交互**：通过RGB灯光反馈增强人机交互的情感体验

## 使用方法

### 快速构建
```
python scripts/release.py nano-glowbies
```

### menuconfig 配置
在菜单配置中选择 "nano-glowbies" 板子类型。

## 技术规格
- 主控芯片：ESP32S3
- 存储：8MB Flash + 8MB PSRAM
- 音频：ES8311 编解码器，3W 功放
- 通信：WiFi/BLE
- 接口：FPC 排线、Type-C、I2C 总线
- 电源：锂电池供电，支持电源管理
- 按键：BOOT 按键 (GPIO0)、控制按键 (GPIO15)
- LED：红色 LED (GPIO47)、绿色 LED (GPIO48)、全彩RGB灯带

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
- 全彩RGB灯带（用于模拟嘴巴和眼睛发光）

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

### RGB灯带
- 嘴巴和眼睛 GPIO_NUM_3

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

```
python scripts/release.py nano-glowbies
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

```
idf.py set-target esp32s3
```

2. **打开 menuconfig 并选择板子：**

```
idf.py menuconfig
```

3. **在 menuconfig 中选择：**
```
Xiaozhi Assistant -> Board Type -> nano-glowbies
```

4. **编译：**

```
idf.py build
```

## 应用场景
- AI语音玩具
- 儿童智能陪伴
- 情绪感知灯具
- 智能语音助手
- 教育娱乐设备
- 个性化装饰灯