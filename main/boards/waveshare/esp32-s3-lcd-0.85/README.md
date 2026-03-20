# 产品链接

[微雪电子 ESP32-S3-LCD-0.85](https://www.waveshare.net/shop/ESP32-S3-LCD-0.85.htm)

# 编译配置命令

**克隆工程**

```bash
git clone https://github.com/78/xiaozhi-esp32.git
```

**进入工程**

```bash
cd xiaozhi-esp32
```

**配置编译目标为 ESP32C6**

```bash
idf.py set-target esp32S3
```

**打开 menuconfig**

```bash
idf.py menuconfig
```

**选择板子**

```bash
Xiaozhi Assistant -> Board Type -> Waveshare ESP32-S3-LCD-0.85
```

**修改 flash 大小：**

```
Serial flasher config -> Flash size -> 8 MB
```

**修改分区表：**

```
Partition Table -> Custom partition CSV file -> partitions/v2/8m.csv
```

**编译**

```ba
idf.py build
```

**下载并打开串口终端**

```bash
idf.py build flash monitor
```

