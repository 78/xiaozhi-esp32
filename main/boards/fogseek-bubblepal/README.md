# FogSeek BubblePal 开发板

FogSeek BubblePal 是基于 ESP32-S3 的开发板，具有以下特性：

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

## 编译和烧录

使用以下命令编译和烧录固件：

```
idf.py build
idf.py flash
```