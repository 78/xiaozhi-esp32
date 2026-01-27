# DFRobot 行空板 K10

## 按键配置
* A：短按-打断/唤醒，长按1s-音量调大
* B：短按-打断/唤醒，长按1s-音量调小

## 编译配置命令

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
Xiaozhi Assistant -> Board Type -> DFRobot 行空板 K10
```

**修改 psram 配置：**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
```

**使能摄像头缓冲区大小端交换：**

```
Xiaozhi Assistant -> Camera Configuration -> Enable software camera buffer endianness swapping
```

**配置摄像头：**
```
Component config -> Espressif Camera Sensors Configurations -> Camera Sensor Configuration -> Select and Set Camera Sensor -> GC2145 ->  Auto detect GC2145

```

```
Component config -> Espressif Camera Sensors Configurations -> Camera Sensor Configuration -> Select and Set Camera Sensor -> GC2145 ->  Select default output format for DVP interface (RGB565 800x600 20fps, DVP 8-bit, 20M input) -> RGB565 800x600 20fps, DVP 8-bit, 20M input

```

**编译：**

```bash
idf.py build
```



