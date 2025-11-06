## 产品简介

雾岸科技 ESP32-S3 EDGE LCD1.8 是一款高度集成的开发板，搭载了1.8寸圆形LCD屏幕，基于ESP32-S3核心和ES8311音频编解码器，支持语音交互和图形显示功能。

<img src="../../../docs/v1/fogseek-esp32s3-audio.jpg" alt="fogseek-esp32s3-edge-lcd1.8" style="zoom: 25%;" />

### 特殊引脚定义

该开发板与ESP32-S3-WROOM-1U高度兼容，除了音频相关引脚外，还引出了SPI接口用于连接LCD屏幕。

| 引脚           | 功能占用            | 备注                 |
| -------------- | ------------------- | -------------------- |
| IO1            | LCD_BL_GPIO         | LCD背光控制          |
| IO2            | LCD_CS_GPIO         | LCD片选信号          |
| IO3            | LCD_SCL_GPIO        | LCD时钟信号          |
| IO6            | LCD_RESET_GPIO      | LCD复位信号          |
| IO7            | LCD_IO3_GPIO        | LCD数据线3           |
| IO8            | LCD_IO2_GPIO        | LCD数据线2           |
| IO9            | LCD_IO1_GPIO        | LCD数据线1           |
| IO10           | LCD_IO0_GPIO        | LCD数据线0           |

**音频相关引脚**
| 引脚           | 功能占用            | 备注                 |
| -------------- | ------------------- | -------------------- |
| IO35(内部 PSRAM) | VDD_AU              | 模组/音频供电        |
| IO36(内部 PSRAM) | OUTP                | 音频输出P            |
| IO37(内部 PSRAM) | OUTN                | 音频输出N            |
| IO12           | AUDIO_I2S_GPIO_DOUT | 音频输出（扬声器）   |
| IO14           | AUDIO_I2S_GPIO_DIN  | 音频输入（麦克风）   |
| IO17           | AUDIO_I2S_GPIO_BCLK | 音频位时钟           |
| IO13           | AUDIO_I2S_GPIO_WS   | 音频帧时钟           |
| IO38           | AUDIO_I2S_GPIO_MCLK | 音频主时钟           |

## 快速构建

推荐使用以下命令一键构建固件，该方式会自动应用所有板子特定配置：

```bash
python scripts/release.py fogseek-esp32s3-edge-lcd1.8
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
Xiaozhi Assistant -> Board Type -> 雾岸科技 ESP32-S3 EDGE LCD1.8
```

4. **编译：**

```bash
idf.py build
```

## 显示屏规格

该开发板搭载1.8寸圆形LCD屏幕，分辨率为360x360像素，支持16位色深，通过QSPI接口连接，提供流畅的图形显示效果。