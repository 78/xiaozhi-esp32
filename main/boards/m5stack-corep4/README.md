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
    * Interface: I2C1 @**0x6F**
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

-----------
## 手动配置

配置编译目标：

```bash
idf.py set-target esp32p4
```

打开配置菜单：

```bash
idf.py menuconfig
```

选择板卡：

```text
Xiaozhi Assistant -> Board Type -> M5Stack CoreP4
```

配置 Flash 大小：

```text
Serial flasher config -> Flash size -> 16 MB
```

配置分区表：

```text
Partition Table -> Custom partition CSV file -> partitions/v2/16m.csv
```

CoreP4 使用 ESP32-C6 作为 Wi-Fi 协处理器，并通过 ESP-Hosted SDIO 连接。构建前需要确认 SDIO 引脚配置和硬件一致，不建议在未确认 SDIO 引脚时直接使用 release 脚本生成固件。

在 `menuconfig` 中配置 ESP-Hosted Wi-Fi，并按 CoreP4 硬件设置 SDIO 引脚：

```text
Component config -> ESP-Hosted config -> Hosted Enable -> Enable
Component config -> ESP-Hosted config -> Transport layer -> SDIO
Component config -> ESP-Hosted config -> Slave chipset target -> ESP32-C6
Component config -> ESP-Hosted config -> SDIO GPIO reset active level -> Active High
Component config -> ESP-Hosted config -> SDIO slot -> Slot 1
Component config -> ESP-Hosted config -> SDIO bus width -> 4-bit
Component config -> ESP-Hosted config -> CMD GPIO number -> 44
Component config -> ESP-Hosted config -> CLK GPIO number -> 43
Component config -> ESP-Hosted config -> D0 GPIO number -> 45
Component config -> ESP-Hosted config -> D1 GPIO number -> 46
Component config -> ESP-Hosted config -> D2 GPIO number -> 47
Component config -> ESP-Hosted config -> D3 GPIO number -> 48
Component config -> ESP-Hosted config -> GPIO pin for Reseting slave ESP -> 42
```

编译：

```bash
idf.py build flash monitor
```

-----------
## 合并固件

手动构建后，可使用以下命令合并烧录固件：

```bash
esptool.py --chip esp32p4 merge_bin \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 16MB \
    0x2000 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0xd000 build/ota_data_initial.bin \
    0x20000 build/xiaozhi.bin \
    0x800000 build/generated_assets.bin \
    -o M5Stack-CoreP4-XiaoZhi-v2.2.6_0x00.bin
```

烧录合并后的固件：

```bash
esptool.py --chip esp32p4 -b 1500000 write_flash -z 0 M5Stack-CoreP4-XiaoZhi-v2.2.6_0x00.bin
```