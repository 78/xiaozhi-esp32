# ESP-Spot S3

## 简介

<div align="center">
    <a href="https://oshwhub.com/esp-college/esp-spot"><b> 立创开源平台 </b></a>
    |
    <a href="https://www.bilibili.com/video/BV1ekRAYVEZ1/"><b> Bilibili Demo </b></a>
</div>

ESP-Spot 是 ESP Friends 开源的一款智能语音交互盒子，内置麦克风、扬声器、IMU 惯性传感器，可使用电池供电。ESP-Spot 不带屏幕，带有一个 RGB 指示灯和两个按钮。硬件详情可查看[立创开源项目](https://oshwhub.com/esp-college/esp-spot)。

ESP-Spot 开源项目采用 ESP32-S3-WROOM-1-N16R8 模组。如在复刻时使用了其他大小的 Flash，需修改对应的参数。


## 配置、编译命令

**配置编译目标为 ESP32S3**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig 并配置**

```bash
idf.py menuconfig
```

分别配置如下选项：

- `Xiaozhi Assistant` → `Board Type` → 选择 `ESP-Spot-S3`
- `Partition Table` → `Custom partition CSV file` → 输入 `partitions.csv`
- `Serial flasher config` → `Flash size` → 选择 `16 MB`

按 `S` 保存，按 `Q` 退出。

**编译**

```bash
idf.py build
```

**烧录**

```bash
idf.py flash
```

> [!TIP]
>
> **若电脑始终无法找到 ESP-Spot 串口，可尝试如下步骤**
> 1. 打开前盖；
> 2. 拔出带有模组的 PCB 板；
> 3. 按住 <kbd>BOOT</kbd> 同时插回 PCB 版，注意不要颠倒；
> 
> 此时， ESP-Spot 应当已进入下载模式。在烧录完成后，可能需要重新插拔 PCB 板。
