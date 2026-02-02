# AtomS3R + EchoPyramid

----------
## 快速体验

下载、安装 [M5Burner](https://docs.m5stack.com/zh_CN/uiflow/m5burner/intro) ，打开 M5Burner 搜索 EchoPyramid 下载小智固件，烧录。

----------
## 编译固件

**配置编译目标为 ESP32S3 **

```bash
idf.py set-target esp32s3
```

**配置**

```bash
idf.py menuconfig
```

**选择板子**

```
Xiaozhi Assistant -> Board Type -> AtomS3R + Echo Pyramid
```

**修改 flash 大小**

```
Serial flasher config -> Flash size -> 8 MB
```

**选择分区表**

```
Partition Table -> Custom partition CSV file -> partitions/v2/8m.csv
```

**修改 psram 配置**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
```

**编译烧录固件**

```bash
idf.py build flash monitor
```
