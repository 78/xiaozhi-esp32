# Seeed Studio XIAO ESP32S3 Sense

## 介绍
Seeed Studio XIAO 系列是迷你型开发板，具有类似的硬件结构，尺寸仅为拇指大小。代码名称 "XIAO" 代表其特点之一“微小”，而另一特点则是“强大”。
Seeed Studio XIAO ESP32S3 Sense 集成了摄像头传感器、数字麦克风和支持 SD 卡的功能。结合嵌入式机器学习计算能力和摄影功能，这款开发板是您入门智能语音和视觉 AI 的绝佳工具。
![](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32S3/img/xiaoesp32s3sense.jpg)

[点击查看 Seeed Studio XIAO ESP32S3 Sense 详细介绍](https://wiki.seeedstudio.com/cn/xiao_esp32s3_getting_started/)


本固件需要搭配 Seeed Studio XIAO 扩展底座 和 MAX98357A 功放模块使用：
* Seeed Studio XIAO 扩展底座：[点击查看详情](https://wiki.seeedstudio.com/cn/Seeeduino-XIAO-Expansion-Board/)
![](https://files.seeedstudio.com/wiki/Seeeduino-XIAO-Expansion-Board/Update_pic/zheng1.jpg)

* MAX98357A 功放模块：如图焊接可直插在扩展底座上使用，否则请按照硬件连接做好连线

| ![](https://raw.githubusercontent.com/HonestQiao/xiaozhi-esp32/refs/heads/xiao-esp32s3-sense/docs/v1/max98357a_1.jpg) | ![](https://raw.githubusercontent.com/HonestQiao/xiaozhi-esp32/refs/heads/xiao-esp32s3-sense/docs/v1/max98357a_2.jpg) |
|---|---|





## 特性
* 使用PDM麦克风
* 板载 OV2640 摄像头
* 扩展底座板载 128x64 OLED显示屏

## 硬件连接

| Seeed Studio XIAO 扩展底座 | MAX98357A |
|:--------------------------:|:---------:|
| USB5V                      | VCC       |
| G                          | GND       |
|                            | SD        |
|                            | GAIN      |
| 9                          | DIN       |
| 8                          | BCLK      |
| 7                          | LRC       |

![](https://files.seeedstudio.com/wiki/Seeeduino-XIAO-Expansion-Board/XIAO-to-board.png)
![](https://raw.githubusercontent.com/HonestQiao/xiaozhi-esp32/refs/heads/xiao-esp32s3-sense/docs/v1/xiao-esp32s3-sense.jpg)


## 按键配置
* Button(D1,扩展底座按键)：短按-打断/唤醒、开机配置网络

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
Xiaozhi Assistant -> Board Type -> Seeed Studio XIAO ESP32S3 Sense
```

**修改 psram 配置：**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
```

**修改 flash 大小：**

```
Serial flasher config -> Flash size -> 8 MB
```

**修改分区表：**

```
Partition Table -> Custom partition CSV file -> partitions/v1/8m.csv
```

**编译：**

```bash
idf.py build
```

**烧录：**

```bash
idf.py flash
```

**查看日志：**

```bash
idf.py monitor
```