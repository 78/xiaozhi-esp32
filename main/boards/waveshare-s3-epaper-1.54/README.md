# 产品链接

[微雪电子 ESP32-S3-ePaper-1.54](https://www.waveshare.net/shop/ESP32-S3-ePaper-1.54.htm)

```bash
esptool.py flash_id
V1: 4MB Flash, 2MB PSRAM
V2: 8MB Flash, 8MB PSRAM
```

# 编译配置命令

**克隆工程**

```bash
git clone https://github.com/78/xiaozhi-esp32.git
```

**进入工程**

```bash
cd xiaozhi-esp32
```

**配置编译目标为 ESP32S3**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig**

```bash
idf.py menuconfig
```

**选择板子**

```bash
Xiaozhi Assistant -> Board Type -> Waveshare ESP32-S3-ePaper-1.54_v2
```

**编译**

```bash
python ./scripts/release.py --name waveshare-s3-epaper-1.54-v2 waveshare-s3-epaper-1.54
```

**下载并打开串口终端**

```bash
idf.py flash monitor
```

