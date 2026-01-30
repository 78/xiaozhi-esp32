# M5Stack CoreP4

-----------
## Overview

- **MCU**: ESP32-P4
- **PSRAM**: 8MB
- **Flash**: 16MB
- **Wi-Fi**: ESP32-C6
- **Display**: 480×480 MIPI LCD, Capacitive Touch Panel

-----------
## Hardware

* **I2C1**
    * SCL -- G9
    * SDA -- G11
* **M5STP2 (Power)**
    * Interface: I2C1 @0x6F
* **IO Expander: M5IOE1**
    * Interface: I2C1 @0x6E
* **Wi-Fi**
    * RST -- G42
    * SDIO
        * CMD -- G44
        * CLK -- G43
        * D0 -- G45
        * D1 -- G46
        * D2 -- G47
        * D3 -- G48
* **Display**
    * Driver: ST7102
    * Interface: MIPI
    * PWR_EN -- M5IOE1_G10 (Active High)
    * RST    -- M5IOE1_G11
    * BL     -- M5IOE1_G9/PWM1
* **Touch Panel**
    * Driver: CST3xx
    * Interface: I2C1
    * RST -- M5IOE1_G8
    * INT -- G1
* **Audio**
    * PWR_EN -- M5IOE1_G1 (Active High)
    * ADC: ES7210
        * Control Interface: I2C1
        * Data Interface: I2S0
    * DAC: ES8311
        * Control Interface: I2C1
        * Data Interface: I2S0
            * MCLK -- G2
            * BCLK -- G6
            * WS   -- G4
            * DOUT -- G5
            * DIN  -- G3
    * PA: AW8737A
        * EN -- M5IOE1_G3 (Active High)
* **IMU: BMI270**
    * Interface: I2C1 @0x68
    * INT -- G0
* **RTC: RX8130CE**
    * Interface: I2C1 @0x51
    * INT -- G4

---------------
## Build & Test

### Configuration

```shell
idf.py menuconfig
```

### release firmware

```shell
python scripts/release.py m5stack-corep4
```