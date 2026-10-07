# ESP-Mosaico

<div align="center">
    <a href="https://mosaico.espressif.com">
        <b> ESP-Mosaico Website (Docs, etc.) </b>
    </a>
    |
    <a href="./README_CN.md">
        <b> 简体中文 </b>
    </a>
</div>

## Introduction

ESP-Mosaico is an expandable smart interaction development kit from Espressif, built on the ESP32-S31, targeting applications such as magnetic expansion modules, motion sensing, square touch screens, and on-device multimedia. The device features a 480 × 480 QSPI square touch screen, audio codec and amplifier, 6-axis IMU, dual magnetometers, SPI NAND flash, and left/right module interfaces with two 2 × 10P, 2.54 mm standard-pitch connectors for expansion such as cameras.

ESP-Mosaico is available in different hardware revisions. The current XiaoZhi firmware supports ESP-Mosaico "Early Makers" (`v1.0`) and the ESP-Mosaico production version (`v1.2`), with support for OV3640 and SC101IOT camera module.

> [!NOTE]
>
> The current XiaoZhi firmware does not support hot-plugging modules yet. **To use the camera, insert the camera modules before startup.**

## Build

ESP-IDF v6.2 has not been officially released yet. Until it is, build with ESP-IDF `master` at commit `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`.

```sh
source /path/to/esp-idf-master/export.sh
python3 scripts/build.py espressif/esp-mosaico
```
