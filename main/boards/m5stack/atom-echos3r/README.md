# AtomEchoS3R
## 简介

AtomEchoS3R 是 M5Stack 推出的基于 ESP32-S3-PICO-1-N8R8 的物联网可编程控制器，采用了 ES8311 单声道音频解码器、MEMS 麦克风和 NS4150B 功率放大器的集成方案。

开发版**不带屏幕、不带额外按键**，需要使用语音唤醒。必要时，需要使用 `idf.py monitor` 查看 log 以确定运行状态。

## 配置、编译命令

**配置编译目标为 ESP32S3**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig 并配置**

```bash
idf.py menuconfig
```

分别配置如下选项：

- `Xiaozhi Assistant` → `Board Type` → 选择 `AtomEchoS3R`
- `Partition Table` → `Custom partition CSV file` → 删除原有内容，输入 `partitions/v2/8m.csv`
- `Serial flasher config` → `Flash size` → 选择 `8 MB`
- `Component config` → `ESP PSRAM` → `Support for external, SPI-connected RAM` → `SPI RAM config` → 选择 `Octal Mode PSRAM`

按 `S` 保存，按 `Q` 退出。

**编译**

```bash
idf.py build
```

**烧录**

将 AtomEchoS3R 连接到电脑，按住侧面 RESET 按键，直到 RESET 按键下方绿灯闪烁。

```bash
idf.py flash
```

烧录完毕后，按一下 RESET 按钮重启设备。
