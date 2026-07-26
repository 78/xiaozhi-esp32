# StopWatch

-----------
## hardware

* MCU:ESP32-S3
    * Flash: 16MiB
    * PSRAM: 8MiB
    * SYS_I2C/I2C1
        * SCL -- G48
        * SCA -- G47
* PMIC: M5PM1@0x6E
    * Interface: SYS_I2C
* IO Expander: M5IOE1@0x4F
    * Interface: SYS_I2C
* Power
    * Charge Enable -- M5PM1_CHG_EN_PP
    * Charge State -- M5PM1_G2
    * Charge Current Prog -- M5PM1_G3
    * Motor -- M5IOE1_G9
    * Ext output -- M5PM1_BOOST5V_EN_PP
    * Ext output voltage -- M5PM1_5VOUT_ADC_IN
* Display
    * Power -- M5IO1_G8
    * Interface: QSPI
    * Driver: CO5300
    * Resolution: 466x466
    * 圆形显示屏 1.75 Inch Amoled
    * Pin Map
        * RST      -- M5IOE1_G5
        * TE       -- G38
        * QSPI_CS  -- G39
        * QSPI_SCK -- G40
        * QSPI_D0  -- G41
        * QSPI_D1  -- G42
        * QSPI_D2  -- G46
        * QSPI_D3  -- G45
* CTP: CST820B@0x15
    * RST -- M5IOE1_G4
    * INT -- G13
    * Interface: SYS_I2C
* Audio
    * Power -- M5IOE1_G3
    * CODEC: ES8311@0x18
        * Contral Interface: SYS_I2C
        * Data Interface:
            * I2S_MCLK -- G18
            * I2S_BCLK -- G17
            * I2S_LRCK -- G15
            * I2S_DIN  -- G21
            * I2S_DOUT -- G16
    * PA -- M5IOE1_G10
* Button
    * Btuuon1 - G1
    * Button2 - G2
* RTC: RX8130CE@0x32
    * Interface: SYS_I2C
* Grove
    * GND
    * 5V
    * G10
    * G11

-----------
## 快速构建

推荐使用 release 脚本生成完整固件包：

```bash
python scripts/release.py m5stack-stopwatch --name m5stack-stopwatch
```

生成的固件压缩包位于：

```text
releases/v2.2.6_m5stack-stopwatch.zip
```

-----------
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
Xiaozhi Assistant -> Board Type -> M5Stack StopWatch
```

配置 Flash 大小：

```text
Serial flasher config -> Flash size -> 16 MB
```

配置分区表：

```text
Partition Table -> Custom partition CSV file -> partitions/v2/16m.csv
```

配置 PSRAM：

```text
Component config -> ESP PSRAM -> Support for external, SPI-connected RAM -> Select
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
Component config -> ESP PSRAM -> SPI RAM config -> Set RAM clock speed -> 80MHz clock speed
```

配置唤醒词实现方式：

```text
Xiaozhi Assistant -> Wake Word Implementation Type -> Wakenet model with AFE
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
    --flash_size 16MB \
    0x0 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0xd000 build/ota_data_initial.bin \
    0x20000 build/xiaozhi.bin \
    0x800000 build/generated_assets.bin \
    -o M5Stack-StopWatch-XiaoZhi-v2.2.6_0x00.bin
```

烧录合并后的固件：

```bash
esptool.py -b 1500000 write_flash -z 0 M5Stack-StopWatch-XiaoZhi-v2.2.6_0x00.bin
```
