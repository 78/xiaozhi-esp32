# M5Stack StopWatch

* MCU: ESP32-S3
* PSRAM: 8MB
* Flash: 16MB
* Display: 1.75Inch, 466x466 AMOLED, CTP.


-----------
## Hardware

* I2C0
    * SCL -- G48
    * SDA -- G47
* PMIC: M5PM1
    * Interface: I2C0@0x6E
* IO Expander: M5IOE1
    * Interface: I2C0@0x6F
* Display
    * Power -- M5IOE1_G8 高电平开
    * Reset -- M5IOE1_G5 低电平复位
    * Driver: CO5300
    * Interface: QSPI
        * SCK -- G40
        * D0  -- G41
        * D1  -- G42
        * D2  -- G46
        * D3  -- G45
    * TE(Tearing Effect) -- G38(No Use)
* Audio
    * Power -- M5IOE1_G3 高电平开
    * CODEC: ES8311
    * Control Interface: I2C0@0x30
    * Data Interface: I2S0
        * MCLK -- G18
        * BCLK -- G17
        * WS   -- G15
        * DOUT -- G21
        * DIN  -- G16
    * PA: AW8378
        * Control -- G14
* Button
    * KEY1 -- G2
    * KEY2 -- G1


----------------
## Test

```shell
idf.py menuconfig

```

* release firmware

```shell
python scripts/release.py m5stack-stopwatch
```