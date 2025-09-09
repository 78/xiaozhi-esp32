# AtomS3R CAM/M12 + Echo Base

## 简介

<div align="center">
    <a href="https://docs.m5stack.com/zh_CN/core/AtomS3R%20Cam"><b> AtomS3R CAM 产品主页 </b></a>
    |
    <a href="https://docs.m5stack.com/zh_CN/core/AtomS3R-M12"><b> AtomS3R M12 产品主页 </b></a>
    |
    <a href="https://docs.m5stack.com/zh_CN/atom/Atomic%20Echo%20Base"><b> Echo Base 产品主页 </b></a>
</div>

AtomS3R CAM、AtomS3R M12 是 M5Stack 推出的基于 ESP32-S3-PICO-1-N8R8 的物联网可编程控制器，搭载了摄像头。Atomic Echo Base 是一款专为 M5 Atom 系列主机设计的语音识别底座，采用了 ES8311 单声道音频解码器、MEMS 麦克风和 NS4150B 功率放大器的集成方案。

两款开发版均**不带屏幕、不带额外按键**，需要使用语音唤醒。必要时，需要使用 `idf.py monitor` 查看 log 以确定运行状态。

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

- `Xiaozhi Assistant` → `Board Type` → 选择 `AtomS3R CAM/M12 + Echo Base`
- `Xiaozhi Assistant` → `IoT Protocol` → 选择 `MCP协议` 可开启摄像头识别功能
- `Partition Table` → `Custom partition CSV file` → 删除原有内容，输入 `partitions/v2/8m.csv`
- `Serial flasher config` → `Flash size` → 选择 `8 MB`

按 `S` 保存，按 `Q` 退出。

**编译**

```bash
idf.py build
```

**烧录**

将 AtomS3R CAM/M12 连接到电脑，按住侧面 RESET 按键，直到 RESET 按键下方绿灯闪烁。

```bash
idf.py flash
```

烧录完毕后，按一下 RESET 按钮重启。
