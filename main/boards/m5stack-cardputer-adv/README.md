# M5Stack Cardputer Adv

-----------
## Overview

- **MCU**: ESP32-S3
- **PSRAM**: 8MB
- **Flash**: 8MB
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

### Configuration

```shell
idf.py menuconfig
```

### release firmware

```shell
python scripts/release.py m5stack-cardputer-adv
```

