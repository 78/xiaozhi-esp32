# M5Stack CoreP4

-----------
## Overview

- **MCU**: ESP32-P4
- **PSRAM**: 8MB
- **Flash**: 16MB
- **Wi-Fi**: ESP32-C6
- **Display**: 2.41-inch Rect LCD, 480×480, Capacitive Touch Panel

-----------
## Hardware

* **I2C0**
    * SCL -- G9 （时钟 / SCL）
    * SDA -- G11 （数据 / SDA）
* **PMIC: M5PM1**
    * Interface: I2C0 @0x6E （电源管理芯片 / Power Management IC）
* **Display（显示屏）**
    * Power -- M5IOE1_G8 高电平开（Power Enable, Active High）
    * Reset -- M5IOE1_G5 低电平复位（Reset, Active Low）
    * Driver: ST7102 （显示驱动 IC）
    * Interface: MIPI
* **Audio（音频）**
    * Power -- M5IOE1_G3 高电平开（Power Enable）
    * CODEC: ES8311
        * Control Interface: I2C0 @0x30（控制接口 / Control）
        * Data Interface: I2S0（音频数据接口 / Audio Data）
            * MCLK -- G18
            * BCLK -- G17
            * WS   -- G15
            * DOUT -- G21
            * DIN  -- G16
    * PA: AW8378（功放 / Power Amplifier）
        * Control -- G14
* **Button（按键）**
    * KEY1 -- G2
    * KEY2 -- G1


---------------
## Build & Test

### Configuration

```shell
idf.py menuconfig
```

### release firmware

```shell
python scripts/release.py m5stack-stopwatch
```