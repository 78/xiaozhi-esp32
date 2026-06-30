# M5Stack Chain Captain


-----------
## Hardware

* 硬件连接
    * MCU: ESP32S3
        * Power -- M1PM1_DCDC_3V3
        * Flash: 16MB
        * PSRAM: 8MB
    * SYS_I2C(I2C1)
        * SCL -- MCU_G2
        * SDA -- MCU_G3
    * Button
        * K1  -- MCU_G1
        * K2  -- MCU_G4
        * K3  -- MCU_G5
        * PWR -- M5PM1 电源按键（单按复位，双击关机，长按进入下载模式）
    * PMIC: M5PM1
        * Interface: SYS_I2C@0x6E
        * IRQ_OUT/M5PM1_G1（配置为中断信号输出） -- MCU_G14
    * IO Exapnder: M5IOE1
        * Interface: SYS_I2C@0x6F
    * Charger: LGS4056
        * Enable（充电使能） -- M5PM1_CHG_EN
        * State（充电状态：充电时低电平） -- M5PM1_G0
    * RGB LED
        * RED -- M5IOE1_G10
        * GREEN -- M5IOE1_G8
        * BLUE -- M5IOE1_G9
    * Dispaly
        * Driver IC: ST7789
        * SPI
            * MOSI -- MCU_G16
            * SCK -- MCU_G15
            * CS -- MCU_G45
        * D/C -- MCU_G46
        * RST -- M5IOE1_G1
        * Backlight -- M5IOE1_G11/PWM_CH3
    * IMU: BMI270
        * Power -- M5PM1_LDO_3V3
        * Interface: SYS_I2C@0x68
        * INT -- M5PM1_G4
    * RTC: RX8130CE
        * Power -- 3V3_L0（常电）
        * Interface: SYS_I2C@0x32
        * INT -- M5PM1_G2
    * Audio
        * Power -- M5IOE1_G5（高电平使能）
        * CODEC -- ES8311
            * Control Interface: SYS_I2C@0x18
            * Data Interface: I2S1
                * MCLK -- MCU_G40
                * WS   -- MCU_G41
                * BCLK -- MCU_G38
                * DIN  -- MCU_G39
                * DOUT -- MCU_G42
        * PA -- AW8737A
            * EN -- MCU_G21
    * IR
        * Power -- M5IOE1_G2
        * TX -- MCU_G48
        * RX -- MCU_G47
    * Header 拓展排母 15PIN
        * 5VIN
        * VBAT
        * 3V3_L2
        * ESP32 CHIP_EN
        * MCU_G0_BOOT
        * MCU_G44_U0RXD
        * MCU_G43_U0TXD
        * MCU_G8 ~ MCU_G13
        * 5VOUT
            * Power -- M5PM1_G3
        * GND
    * Grove
        * PORT.A
            * Power -- M5PM1_BOOST5V_EN_PP
            * MCU_G6
            * MCU_G7
        * PORT.B
            * Power -- M5PM1_G3
            * MCU_G17
            * MCU_G18
