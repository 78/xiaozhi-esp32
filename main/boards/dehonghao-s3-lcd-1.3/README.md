# Dehonghao S3 LCD 1.3 开发板

## 硬件规格

- **主控芯片**: ESP32-S3-N16R8
- **显示屏**: 1.3寸 ST7789 IPS彩色屏幕
- **分辨率**: 240x240像素
- **接口**: SPI
- **音频**: 自带麦克风和扬声器
- **连接**: Type-C USB接口

## 硬件连接

### 显示屏连接 (SPI)
- **MOSI**: GPIO 4
- **SCLK**: GPIO 15  
- **CS**: GPIO 22
- **DC**: GPIO 21
- **RST**: GPIO 18
- **BL**: GPIO 23 (背光控制)

### 音频连接 (I2S)
- **麦克风**:
  - WS: GPIO 25
  - SCK: GPIO 26
  - DIN: GPIO 32

- **扬声器**:
  - BCLK: GPIO 14
  - LRCK: GPIO 27
  - DOUT: GPIO 33

### 按钮连接
- **Boot按钮**: GPIO 0
- **触摸按钮**: GPIO 5
- **ASR按钮**: GPIO 19
- **内置LED**: GPIO 2

### MCP设备
- **LED灯**: GPIO 12

## 编译和烧录

### 编译固件
```bash
python scripts/release.py dehonghao-s3-lcd-1.3
```

### 烧录固件
```bash
# 使用生成的烧录脚本
./scripts/flash.sh
```

## 功能特性

1. **显示屏**: 1.3寸240x240 IPS彩色屏幕，支持中文显示
2. **音频**: 支持语音输入和输出，使用NoAudioCodecSimplex
3. **按钮控制**:
   - Boot按钮: 切换聊天状态
   - 触摸按钮: 语音识别控制
   - ASR按钮: 唤醒词触发
4. **MCP支持**: 支持LED灯控制等MCP设备
5. **WiFi连接**: 支持WiFi网络连接

## 配置说明

### 显示屏配置
- 驱动芯片: ST7789
- 分辨率: 240x240
- 颜色格式: RGB565
- SPI模式: 0
- 时钟频率: 40MHz

### 音频配置
- 输入采样率: 16kHz
- 输出采样率: 24kHz
- 模式: Simplex (麦克风和扬声器分离)

## 注意事项

1. 确保使用正确的ESP32S3开发板配置
2. 显示屏的SPI引脚连接正确
3. 音频I2S引脚配置正确
4. 首次使用需要配置WiFi网络

## 故障排除

### 显示屏不显示
- 检查SPI引脚连接
- 确认显示屏电源供电
- 检查背光控制引脚

### 音频无输出
- 检查I2S引脚连接
- 确认扬声器连接
- 检查音频编解码器配置

### 无法连接WiFi
- 检查WiFi凭据配置
- 确认网络环境
- 检查天线连接

## 开发者信息

- **开发者**: 郝德宏 (RogerHao)
- **基于**: bread-compact-esp32-lcd
- **适配**: ESP32S3 + 1.3寸ST7789显示屏 