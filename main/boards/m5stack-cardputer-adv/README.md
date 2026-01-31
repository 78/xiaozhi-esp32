# M5Stack Cardputer Adv

-----------
## Overview

- **MCU**: ESP32-S3
- **Flash**: 8MB
- **PSRAM**: None
- **Display**: 1.14-inch LCD, 135×240

-----------
## Hardware

* **I2C0**
    * SCL -- G9
    * SDA -- G8
* **Display**
    * Driver: ST7789
    * Interface: SPI
        * MOSI -- G35
        * SCLK -- G36
        * CS   -- G37
        * RS   -- G34
        * RST  -- G33
    * BL -- G38
* **Audio**
    * CODEC: ES8311
        * Control Interface: I2C0 @0x18
        * Data Interface: I2S0
            * MCLK -- NC
            * BCLK -- G41
            * WS   -- G43
            * DOUT -- G42
            * DIN  -- G46
    * PA -- NC
* **Button**
    * KEY1 -- G0

---------------
## Build & Test

1. configuration

```shell
idf.py menuconfig

```

2. compile

```shell
python scripts/release.py m5stack-cardputer-adv
```

3. flash firmware

```shell
python -m esptool --before default_reset --after hard_reset write_flash -z 0 build/merged-binary.bin
```

