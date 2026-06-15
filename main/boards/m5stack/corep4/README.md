# M5Stack CoreP4


- **MCU**: ESP32-P4
- **PSRAM**: 8MB
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
            * DOUT -- G3
            * DIN  -- G5
    * PA: AW8737A
        * EN -- M5IOE1_G3 (Active High)
* **IMU: BMI270**
    * Interface: I2C1 @0x68
    * INT -- G0
* **RTC: RX8130CE**
    * Interface: I2C1 @0x51
    * INT -- G4

-----------
## 构建

CoreP4 使用 ESP32-C6 作为 Wi-Fi 协处理器，通过 ESP-Hosted SDIO 连接。板级配置已包含实际使用的 SDIO 引脚和复位引脚。

早期 ESP32-P4 芯片使用 ESP-IDF 5.5：

```bash
python3 scripts/build.py m5stack/corep4 --name m5stack-corep4 --zip
```

ESP32-P4 v3.x（P4X）使用 ESP-IDF 6.0 或更高版本：

```bash
python3 scripts/build.py m5stack/corep4 --name m5stack-corep4x --zip
```

构建脚本会生成合并固件和 release ZIP。烧录前请确认设备使用的 ESP32-P4 芯片版本与构建变体一致。
