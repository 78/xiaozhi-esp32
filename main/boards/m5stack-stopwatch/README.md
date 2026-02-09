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
    * SCL -- G48
    * SDA -- G47
* **PMIC: M5PM1**
    * Interface: I2C0 @0x6E
* **IO Expander: M5IOE1**
    * Interface: I2C0 @0x6F
* **Display**
    * Power -- M5IOE1_G8 (Power Enable, Active High)
    * Reset -- M5IOE1_G5 (Reset, Active Low)
    * Driver: CO5300
    * Interface: QSPI
        * SCK -- G40
        * D0  -- G41
        * D1  -- G42
        * D2  -- G46
        * D3  -- G45
    * TE (Tearing Effect) -- G38 (No Use)
* **Audio**
    * Power -- M5IOE1_G3(Power Enable, Active High)
    * CODEC: ES8311
        * Control Interface: I2C0 @0x30
        * Data Interface: I2S0
            * MCLK -- G18
            * BCLK -- G17
            * WS   -- G15
            * DOUT -- G21
            * DIN  -- G16
    * PA: AW837
        * Control -- M5IOE1_G10
* **Button**
    * KEY1 -- G2
    * KEY2 -- G1


---------------
## Build & Test

1. configuration

```shell
idf.py menuconfig
```

2. compile

```shell
python scripts/release.py m5stack-stopwatch
```

3. flash firmware

```shell
python -m esptool --before default_reset --after hard_reset write_flash -z 0 build/merged-binary.bin
```
