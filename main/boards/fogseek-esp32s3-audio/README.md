## 产品简介

雾岸科技AUS3邮票板是高度集成麦克风与扬声器的核心板，板载LDO供电，可以帮助AI与DIY爱好者快速开发属于自己的智能硬件。

<img src="../../../docs/v1/fogseek-esp32s3-audio.jpg" alt="fogseek-esp32s3-audio" style="zoom: 25%;" />

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

## TODO

由于是核心板，目前只实现了基础语音功能，应有自定义引脚与外设功能，后续预计开发灵活配置功能，兼容绝大部分外设。

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
Xiaozhi Assistant -> Board Type -> 雾岸科技 ESP32-S3-Audio 
```

**编译：**

```bash
idf.py build
```

