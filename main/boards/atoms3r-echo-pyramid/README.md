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
Xiaozhi Assistant -> Board Type -> M5Stack AtomS3R + Echo Pyramid
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

## 使用说明

Echo Pyramid 正常运行时请从 Pyramid 底座的 USB-C 口供电；AtomS3R 的 USB-C 口主要用于烧录。

# 参考资料

https://github.com/m5stack/M5Echo-Pyramid