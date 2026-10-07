# ESP-Mosaico

<div align="center">
    <a href="https://mosaico.espressif.com">
        <b> ESP-Mosaico 网站 (文档等) </b>
    </a>
    |
    <a href="./README.md">
        <b> English </b>
    </a>
</div>

## 简介

ESP-Mosaico 是乐鑫基于 ESP32-S31 打造的可扩展智能交互开发套件，面向磁吸扩展、运动感知、方形触摸屏与端侧多媒体等应用场景。设备搭载 480 × 480 QSPI 方形触摸屏、音频编解码与功放、六轴 IMU、双磁力计、SPI NAND flash，并提供左右两个 2 × 10P、2.54 mm 标准间距端子的模块接口，可用于摄像头等功能扩展。

ESP-Mosaico 有不同的硬件版本，当前版本的小智固件已支持 ESP-Mosaico "Early Makers" (`v1.0`) 和 ESP-Mosaico 正式版 (`v1.2`)，支持 OV3640 和 SC101IOT 两种摄像头子版。

> [!NOTE]
>
> 当前版本的小智固件暂不支持子版热插拔，**如需使用摄像头功能，请在启动前插入摄像头子版**。

## 编译

目前 ESP-IDF 暂未发布 v6.2 正式版，在 ESP-IDF v6.2 正式发布前，推荐使用 `master` 分支 `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe` commit 的 ESP-IDF 编译。

```sh
source /path/to/esp-idf-master/export.sh
python3 scripts/build.py espressif/esp-mosaico
```
