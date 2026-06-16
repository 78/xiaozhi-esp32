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

## 快速构建

推荐使用 release 脚本生成完整固件包：

```bash
python scripts/release.py m5stack-stick-s3 --name m5stack-stick-s3
```

生成的固件压缩包位于：

```text
releases/v2.2.6_m5stack-stick-s3.zip
```

`config.json` 中的关键构建配置：

```text
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/8m.csv"
CONFIG_SPIRAM=y
```

## 手动配置

配置编译目标：

```bash
idf.py set-target esp32s3
```

打开配置菜单：

```bash
idf.py menuconfig
```

选择板卡：

```text
Xiaozhi Assistant -> Board Type -> M5Stack StickS3
```

配置 Flash 大小：

```text
Serial flasher config -> Flash size -> 8 MB
```

配置分区表：

```text
Partition Table -> Custom partition CSV file -> partitions/v2/8m.csv
```

配置 PSRAM：

```text
Component config -> ESP PSRAM -> Support for external, SPI-connected RAM -> Select
```

编译：

```bash
idf.py build
```

## 合并固件

手动构建后，可使用以下命令合并烧录固件：

```bash
esptool.py --chip esp32s3 merge_bin \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 8MB \
    0x0 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0xd000 build/ota_data_initial.bin \
    0x20000 build/xiaozhi.bin \
    0x600000 build/generated_assets.bin \
    -o M5Stack-StickS3-XiaoZhi-v2.2.6_0x00.bin
```

烧录合并后的固件：

```bash
esptool.py -b 1500000 write_flash -z 0 M5Stack-StickS3-XiaoZhi-v2.2.6_0x00.bin
```
