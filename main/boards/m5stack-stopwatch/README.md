# M5Stack StopWatch

-----------
## Overview

- **MCU**: ESP32-S3
- **PSRAM**: 8MB
- **Flash**: 16MB
- **Display**: 1.75-inch Circular AMOLED, 466×466, Capacitive Touch Panel

-----------
## Hardware

* **I2C0**
    * SCL -- G48 （时钟 / SCL）
    * SDA -- G47 （数据 / SDA）
* **PMIC: M5PM1**
    * Interface: I2C0 @0x6E （电源管理芯片 / Power Management IC）
* **IO Expander: M5IOE1**
    * Interface: I2C0 @0x6F （IO 扩展芯片 / IO Expander）
* **Display（显示屏）**
    * Power -- M5IOE1_G8 高电平开（Power Enable, Active High）
    * Reset -- M5IOE1_G5 低电平复位（Reset, Active Low）
    * Driver: CO5300 （显示驱动 IC）
    * Interface: QSPI
        * SCK -- G40
        * D0  -- G41
        * D1  -- G42
        * D2  -- G46
        * D3  -- G45
    * TE (Tearing Effect) -- G38（No Use / 未使用）
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
        * Control -- M5IOE1_G10
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