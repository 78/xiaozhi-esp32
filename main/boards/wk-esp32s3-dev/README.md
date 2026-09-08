# WK ESP32-S3 Dev Board（维控智能 ESP32-S3 开发板）

WK 是维控智能的开发板品牌/板卡系列名称。WK ESP32-S3 Dev Board 是一款基于 ESP32-S3 的通用开发板，面向智能语音、AI 交互和物联网应用场景设计。

该开发板集成麦克风、音频功放、显示屏接口以及常用外设接口，并引出了大部分 GPIO，方便用户连接传感器、执行器和其他功能模块，用于语音交互、屏幕显示、联网控制等功能测试。

## 产品链接

[维控智能 WK ESP32-S3 Dev Board](https://item.taobao.com/item.htm?id=922489696592)

## Hardware

- 平台: ESP32-S3N16R8
- 显示屏: 支持常用 LCD 和 OLED 显示屏
- 音频：板载麦克风和功放
- 板子所在目录: `main/boards/wk-esp32s3-dev`


# 编译配置命令


**进入工程**

```bash
cd xiaozhi-esp32
```

**配置编译目标为 ESP32S3**

```bash
idf.py set-target esp32S3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> 维控智能开发板
```

**编译：**

```bash
idf.py build
```

