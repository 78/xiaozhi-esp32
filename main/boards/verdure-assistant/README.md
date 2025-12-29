<p align="center">
  <img width="80%" align="center" src="../../../docs/V1/verdure-assistant.jpg"alt="logo">
</p>
  <h1 align="center">
  绿荫搭子 (Verdure Assistant)
</h1>

## 简介

绿荫搭子是一款基于 ESP32-S3 的智能语音助手开发板，基于立创·实战派 ESP32-S3 开发板修改而来。

主要是为实战派添加了电源管理模块，和重新设计了外壳，以及添加了电源管理的代码，后期可以进一步拓展功能。

## 项目地址

- 开源硬件平台: [https://oshwhub.com/greenshade/verdure-assistant](https://oshwhub.com/greenshade/verdure-assistant)
- 
## B站复刻演示视频

-  [https://www.bilibili.com/video/BV1x5vyBsEZj/](https://www.bilibili.com/video/BV1x5vyBsEZj/)

## 硬件特性

- **主控芯片**: ESP32-S3
- **显示屏**: 2.4寸 ST7789 LCD (320x240)
- **触摸屏**: FT5x06 电容触摸
- **音频编解码**: ES8311 + ES7210
- **摄像头**: GC0308 (可选)
- **扩展芯片**: PCA9557 GPIO 扩展器
- **电量计**: BQ27220 电池电量管理芯片

## 引脚配置

### 音频 I2S 引脚
| 功能 | GPIO |
|------|------|
| MCLK | GPIO38 |
| BCLK | GPIO14 |
| WS   | GPIO13 |
| DIN  | GPIO12 |
| DOUT | GPIO45 |

### I2C 引脚
| 功能 | GPIO |
|------|------|
| SDA  | GPIO1 |
| SCL  | GPIO2 |

### 显示屏 SPI 引脚
| 功能 | GPIO |
|------|------|
| MOSI | GPIO40 |
| SCLK | GPIO41 |
| DC   | GPIO39 |
| 背光 | GPIO42 |

### 其他引脚
| 功能 | GPIO |
|------|------|
| BOOT 按钮 | GPIO0 |
| LED  | GPIO48 |

### BQ27220 电量计
| 功能 | 配置 |
|------|------|
| I2C 地址 | 0x55 |
| SDA  | GPIO1 (与主 I2C 共用) |
| SCL  | GPIO2 (与主 I2C 共用) |

## 编译方法

### 方法一：使用 release.py 脚本（推荐）

```bash
python scripts/release.py verdure-assistant
```

### 方法二：使用 idf.py 手动配置

1. 设置目标芯片:
   ```bash
   idf.py set-target esp32s3
   ```

2. 配置开发板类型:
   ```bash
   idf.py menuconfig
   ```
   导航到: `Xiaozhi Assistant` -> `Board Type` -> 选择 `绿荫搭子 Verdure Assistant`

3. 编译和烧录:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

## 功能支持

- ✅ Wi-Fi 连接
- ✅ 语音交互
- ✅ LCD 显示
- ✅ 触摸屏输入
- ✅ 摄像头 (可选)
- ✅ 设备端 AEC (回声消除)
- ✅ Emote 动画表情
- ✅ BQ27220 电池电量监测

## 特殊说明

- 本开发板使用 PCA9557 GPIO 扩展器控制功放使能和摄像头电源
- 支持设备端 AEC (Acoustic Echo Cancellation)
- 默认启用 GC0308 摄像头驱动
- 集成 BQ27220 电量计，支持以下功能：
  - 电池电量百分比 (SOC)
  - 电池健康度 (SOH)
  - 电压、电流、温度监测
  - 剩余容量、满充容量
  - 充放电状态检测
  - 预估剩余使用/充电时间
  - 充放电循环次数

## 致谢

感谢立创开源硬件平台和实战派开发板提供的硬件基础。
