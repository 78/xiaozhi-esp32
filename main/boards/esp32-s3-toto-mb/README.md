# TONGTONG AI盒子

# 特性
* 使用PDM麦克风
* 使用LED矩阵屏显示表情

## 按键配置
* BUTTON3：短按-打断/唤醒

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
Xiaozhi Assistant -> Board Type -> esp32-s3-toto-mb
```

**修改 psram 配置：**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Octal Mode PSRAM
```

**编译：**

```bash
idf.py build
```