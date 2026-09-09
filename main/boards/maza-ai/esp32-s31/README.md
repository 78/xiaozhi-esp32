# Maza AI ESP32-S31

## 产品介绍

[Maza AI 开发板](https://maza.azurewebsites.net/#board) 集成了语音交互与墨水屏驱动能力，提供墨水屏转接接口。最新基于S31的开发板拥有更快的处理能力和更多的IO接口，还提供了并口墨水屏转接接口。一块板子玩转几乎所有墨水屏，让AI与墨水屏碰撞出有趣的新玩法。

本目录适配 **Maza AI ESP32-S31**，默认使用 A01 2.9 英寸墨水屏，分辨率为 128 × 296。

## 硬件配置

| 模块 | 配置 |
| --- | --- |
| 主控 | ESP32-S31 模组 |
| 音频编解码器 | ES8311 |
| 功放 | NS4150B |
| 麦克风 | ZTS6216 |
| 墨水屏 | A01 2.9 英寸，128 × 296 |
| 墨水屏接口 | 8 Pin、12 Pin FPC 转接接口，并口转接接口 |
| 操作按键 | 控制键 、音量加、音量减 |

## 按键功能

| 按键 | 操作 | 功能 |
| --- | --- | --- |
| 控制键 | 单击 | 开始聆听 / 停止聆听 |
| 音量加 | 单击 / 长按 | 音量增加 10% / 设置为最大音量 |
| 音量减 | 单击 / 长按 | 音量降低 10% / 静音 |

## 编译与烧录

### 准备环境

请先安装并激活 ESP-IDF v6.1 环境，然后克隆本项目：

```bash
git clone https://github.com/78/xiaozhi-esp32.git
cd xiaozhi-esp32
```

### 选择目标与开发板

将编译目标设置为 ESP32-S31：

```bash
idf.py --preview set-target esp32s31
```

> **注意:** 需要用最新的ESP IDF 6.1才能支持ESP32S31。

打开配置菜单：

```bash
idf.py menuconfig
```

在菜单中选择：

```text
Xiaozhi Assistant -> Board Type -> Maza AI ESP32-S31
```

保存配置并退出，然后开始编译：

```bash
idf.py build
```

### 烧录与串口监视

连接开发板后，执行以下命令烧录固件并打开串口监视器：

```bash
idf.py flash monitor
```

如电脑连接了多个串口设备，可通过 `-p` 指定端口，例如：

```bash
idf.py -p COM3 flash monitor
```

按 `Ctrl+]` 可退出串口监视器。
