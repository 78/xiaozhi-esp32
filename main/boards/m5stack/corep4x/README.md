# M5Stack CoreP4X


- **MCU**: ESP32-P4
- **PSRAM**: 32MB
- **Flash**: 16MB
- **Wi-Fi**: ESP32-C6, ESP-Hosted SDIO
- **Display**: 480x480 MIPI LCD, Capacitive Touch Panel

-----------
## Hardware

* **I2C1**
    * SCL -- G9
    * SDA -- G11
* **M5STP2 / PM1 (Power)**
    * Interface: I2C1 @**0x6E**
* **IO Expander: M5IOE1**
    * Interface: I2C1 @**0x4F**
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
    * Driver: CST3530
    * Interface: I2C1
    * RST -- M5IOE1_G8
    * INT -- GPIO1
    * Tap anywhere on the screen to start or stop a conversation
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
            * DOUT -- G3
            * DIN  -- G5
    * PA: AW8737A
        * EN -- M5IOE1_G3 (Active High)
* **IMU: LSM6DS3TR-C**
    * Interface: I2C1 @0x6A
    * INT -- G0
* **RTC: RX8130CE**
    * Interface: I2C1 @0x51
    * INT -- G4

-----------
## Build

CoreP4X uses an ESP32-C6 as the Wi-Fi coprocessor over ESP-Hosted SDIO. The board configuration includes the required SDIO and reset pins.

```bash
python3 scripts/build.py m5stack/corep4x --name m5stack-corep4x --zip
```

The build script generates a merged firmware image and a release ZIP archive.
