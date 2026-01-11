# M5Stack Sticks3

## 硬件配置

* SYS_I2C/I2C0
    * SCL -- G48
    * SDA -- G47
* 按键
    * KEY1 -- G11
* LCD
    * Power: PM1_G2 高电平使能
    * Resolution: 135x240
    * Driver IC: ST7789
    * MOSI -- G39
    * SCLK -- G40
    * CS   -- G41
    * RS   -- G45
    * BL   -- G38
* IMU
    * BMI270@0x68
        * Interface: SYS_I2C
* IR
    * TX -- G46
    * RX -- G42
* Audio
    * Power: PM1_G2 高电平使能
    * ES8311@0x18
        * Control Interface: SYS_I2C
        * Interface: I2S0
            * MCLK   -- G18
            * BCLK   -- G17
            * ASDOUT -- G16
            * LRCK   -- G15
            * DSDIN  -- G14
    * PA -- PM1_G3
* PMIC
    * PM1@0x6E (SYS_I2C)

## 编译固件

**配置编译目标为 ESP32S3**

```bash
idf.py set-target esp32s3
```

**配置**

```bash
idf.py menuconfig
```

**选择板子**

```
Xiaozhi Assistant -> Board Type -> M5Stack Sticks3
```

**修改 flash 大小**

```
Serial flasher config -> Flash size -> 8 MB
```

**选择分区表**

```
Partition Table -> Custom partition CSV file -> partitions/v2/8m.csv
```

**编译烧录固件**

```bash
idf.py build flash monitor
```

**打包固件**

编译完成后，使用以下命令合并固件：

**方法一：使用 idf.py（推荐）**

```bash
idf.py merge-bin
```

合并后的固件文件位于 `build/merged-binary.bin`，可用于 OTA 升级或直接烧录。

**方法二：使用 esptool.py**

```bash
esptool.py merge_bin -o build/xiaozhi-sticks3.bin \
    0x0 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0x20000 build/xiaozhi.bin \
    0x600000 build/assets.bin
```

**使用 release 脚本打包（推荐）**

也可以使用 release 脚本自动打包固件：

```bash
# 打包当前配置的固件
python scripts/release.py

# 或者指定板型打包
python scripts/release.py m5stack-sticks3
```

打包后的固件会生成在 `releases/` 目录下，文件名为 `v{版本号}_{板型名称}.zip`。
