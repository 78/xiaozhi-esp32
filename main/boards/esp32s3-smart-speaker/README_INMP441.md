# INMP441麦克风模块使用指南

## 📋 硬件连接

### ESP32-S3 与 INMP441 接线：
```
INMP441    →    ESP32-S3
VDD       →    3.3V
GND       →    GND
WS        →    GPIO47 (Word Select)
SCK       →    GPIO17 (Serial Clock)  
SD        →    GPIO16 (Serial Data)
```

### 扬声器连接：
```
扬声器     →    ESP32-S3
BCLK      →    GPIO17 (Bit Clock) - 与INMP441 SCK复用
WS        →    GPIO47 (Word Select) - 与INMP441 WS复用
DATA      →    GPIO15 (Data)
```

## 🔧 软件配置

### 1. 音频参数
- **采样率**: 16kHz (推荐)
- **位深度**: 16位 (麦克风) / 32位 (扬声器)
- **声道**: 单声道
- **协议**: PDM (麦克风) / I2S标准 (扬声器)

### 2. 引脚配置
```cpp
// config.h
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_47  // INMP441 Word Select / 扬声器WS
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_17  // INMP441 Serial Clock / 扬声器BCLK
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_16  // INMP441 Serial Data
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_15  // 扬声器数据输出
```

### 3. 使用现有的NoAudioCodecSimplexPdm
```cpp
// esp32s3_smart_speaker.cc
static NoAudioCodecSimplexPdm audio_codec(
    AUDIO_INPUT_SAMPLE_RATE, 
    AUDIO_OUTPUT_SAMPLE_RATE,
    AUDIO_I2S_GPIO_BCLK,  // 扬声器BCLK
    AUDIO_I2S_GPIO_WS,    // 扬声器WS
    AUDIO_I2S_GPIO_DOUT,  // 扬声器DOUT
    AUDIO_I2S_GPIO_BCLK,  // INMP441 SCK引脚
    AUDIO_I2S_GPIO_DIN);  // INMP441 SD引脚
```

## 🎵 音频功能

### 支持的音频功能：
1. **语音输入**: INMP441 PDM麦克风
2. **语音输出**: I2S标准扬声器 (可选)
3. **音频处理**: 降噪、回声消除、VAD
4. **唤醒词检测**: ESP-SR AFE算法
5. **音频编码**: Opus压缩传输

### 音频数据流：
```
INMP441 → PDM解码 → 音频处理 → Opus编码 → 网络传输
网络接收 → Opus解码 → 音频处理 → I2S输出 → 扬声器
```

## 🚀 使用方法

### 1. 编译和烧录
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

### 2. 音频测试
```bash
# 在串口监视器中测试音频功能
audio_test
```

### 3. 音量控制
- **音量+**: GPIO40 短按
- **音量-**: GPIO39 短按
- **静音**: GPIO39 长按
- **最大音量**: GPIO40 长按

## 🔍 调试信息

### 查看音频状态：
```json
{
  "board_type": "esp32s3-smart-speaker",
  "audio_codec": "NoAudioCodecSimplexPdm",
  "audio_method": "software_pdm",
  "microphone": "INMP441",
  "speaker": "I2S_Standard"
}
```

### 常见问题：

1. **无音频输入**
   - 检查INMP441接线
   - 确认3.3V供电
   - 检查GPIO引脚配置

2. **音频质量差**
   - 调整采样率到16kHz
   - 检查电源噪声
   - 确认PDM配置正确

3. **编译错误**
   - 确认包含inmp441_audio_codec.h
   - 检查CMakeLists.txt配置

## 📊 性能参数

- **延迟**: < 100ms
- **功耗**: < 50mA
- **频率响应**: 20Hz - 20kHz
- **信噪比**: > 60dB
- **动态范围**: > 90dB

## 🔧 高级配置

### 自定义音频参数：
```cpp
// 在板子初始化中修改
static NoAudioCodecSimplexPdm audio_codec(
    24000,              // 输入采样率
    24000,              // 输出采样率
    GPIO_NUM_17,        // 扬声器BCLK
    GPIO_NUM_47,        // 扬声器WS
    GPIO_NUM_15,        // 扬声器DATA
    GPIO_NUM_17,        // INMP441 SCK引脚
    GPIO_NUM_16         // INMP441 SD引脚
);
```

### 音频处理配置：
```cpp
// 启用音频处理器
audio_service.EnableVoiceProcessing(true);

// 启用唤醒词检测
audio_service.EnableWakeWordDetection(true);

// 启用设备端回声消除
audio_service.EnableDeviceAec(true);
```
