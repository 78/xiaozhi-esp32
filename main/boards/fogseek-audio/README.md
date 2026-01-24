# 雾岸科技 Audio 开发板

## 产品简介

雾岸科技 Audio 是适用于audio开发板系列的核心板型，包括Audio核心模组、kit语音核心板、Kit语音套件等产品。这是一款高度集成麦克风与扬声器的核心板，板载LDO供电，可以帮助AI与DIY爱好者快速开发属于自己的智能硬件。

### 特殊引脚定义

该核心板与ESP32-S3-WROOM-1U高度兼容，使用原核心板不可使用的IO35、IO36、IO37引脚作为音频电源与扬声器输出。

| 原引脚           | 功能占用            | 备注                 |
| ---------------- | ------------------- | -------------------- |
| IO35(内部 PSRAM) | VDD_AU              | 模组/音频供电        |
| IO36(内部 PSRAM) | OUTP                | 音频输出P            |
| IO37(内部 PSRAM) | OUTN                | 音频输出N            |
| **已占用引脚**   |                     |                      |
| IO1              | AUDIO_I2S_GPIO_DOUT | 保证天线净空区未引出 |
| IO2              | AUDIO_I2S_GPIO_DIN  | 保证天线净空区未引出 |
| IO38（仍引出）   | AUDIO_I2S_GPIO_WS   |                      |
| IO39（仍引出）   | AUDIO_I2S_GPIO_BCLK |                      |

**注意事项**

模组自带LDO，无需3V3输入，其3V3输出能力有限，建议不超过100mA。

模组MIC收音为底部收音，注意挖孔以免影响收音质量。

## 快速构建

推荐使用以下命令一键构建固件，该方式会自动应用所有板子特定配置：

```bash
python scripts/release.py fogseek-audio
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
Xiaozhi Assistant -> Board Type -> 雾岸科技 Audio
```

4. **编译：**

```bash
idf.py build
```

## 扩展功能

由于是核心板，目前只实现了基础语音功能。开发者可以根据自己的需求添加各种外设功能，如：
- 连接不同类型的显示屏
- 添加传感器模块
- 扩展GPIO功能
- 集成更多音频处理功能