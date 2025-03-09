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

**编译：**

```bash
idf.py build
```