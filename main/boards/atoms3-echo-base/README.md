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
Xiaozhi Assistant -> Board Type -> AtomS3 + Echo Base
```

**关闭语音唤醒：**

```
Xiaozhi Assistant -> [ ] 启用语音唤醒与音频处理 -> Unselect
```

**修改 flash 大小：**

```
Serial flasher config -> Flash size -> 8 MB
```

**修改分区表：**

```
Partition Table -> Custom partition CSV file -> partitions/v2/8m.csv
```

**关闭片外 PSRAM：**

```
Component config -> ESP PSRAM -> [ ] Support for external, SPI-connected RAM -> Unselect
```

**编译：**

```bash
idf.py build
```