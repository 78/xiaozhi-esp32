# 编译配置命令

**配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> AtomS3R + Echo Base
```

**修改 flash 大小：**

```
Serial flasher config -> Flash size -> 8 MB
```

**修改分区表：**

```
Partition Table -> Custom partition CSV file -> partitions/v2/8m.csv
```

**修改 psram 配置：**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
```

**编译：**

```bash
idf.py build
```