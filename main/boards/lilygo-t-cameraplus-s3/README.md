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
Xiaozhi Assistant -> Board Type -> LILYGO T-CameraPlus-S3_V1_0_V1_1或LILYGO T-CameraPlus-S3_V1_2
```

**修改 psram 配置：**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Quad Mode PSRAM
```

**编译：**

```bash
idf.py build
```

<a href="https://github.com/Xinyuan-LilyGO/T-CameraPlus-S3" target="_blank" title="LILYGO T-CameraPlus-S3">LILYGO T-CameraPlus-S3</a>