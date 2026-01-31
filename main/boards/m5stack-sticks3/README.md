# M5Stack Sticks3

* MCU: ESP32-S3
* PSRAM: 8MB
* Flash: 8MB
* Display: 1.14Inch, 135x240 LCD.


-----------
## Hardware

* SYS_I2C/I2C0
    * SCL -- G48
    * SDA -- G47
* PMIC: M5PM1
    * Interface: I2C0@0x6E
* LCD
    * Power: M5PM1_G2 高电平使能
    * Resolution: 135x240
    * Driver: CO5300
    * Interface: SPI
        * MOSI -- G39
        * SCLK -- G40
        * CS   -- G41
        * RS   -- G45
    * BL -- G38
* Audio
    * Power: M5PM1_G2 高电平使能
    * ES8311@0x18
        * Control Interface: SYS_I2C
        * Data Interface: I2S0
            * MCLK   -- G18
            * BCLK   -- G17
            * ASDOUT -- G16
            * LRCK   -- G15
            * DSDIN  -- G14
    * PA -- PM1_G3
* Key
    * KEY1 -- G11
* IMU
    * BMI270@0x68
        * Interface: SYS_I2C
* IR
    * TX -- G46
    * RX -- G42

---------------
## Build & Test

1. configuration

```shell
idf.py menuconfig

```

2. compile

```shell
python scripts/release.py m5stack-sticks3
```

3. flash firmware

```shell
python -m esptool --before default_reset --after hard_reset write_flash -z 0 build/merged-binary.bin
```

