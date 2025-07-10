# 由于原来的麦克风型号停产，2025年7月之后的太极派（JC3636W518）更换了麦克风，并且更换了屏幕玻璃，所以在产品标签上批次号大于2528的用户请选择I2S Type PDM,

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
Xiaozhi Assistant -> Board Type -> 太极小派esp32s3

Xiaozhi Assistant -> taiji-pi-S3 I2S Type -> I2S Type PDM
```

**修改PSRAM配置：**

```
component config -> ESP PSRAM -> SPI RAM config -> Try to allocate memories of WiFi and LWIP in SPIRAM firstly. If failed, allocate internal memory

```

**编译：**

```bash
idf.py build
```
