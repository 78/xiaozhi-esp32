# M5Stack Cardputer Adv

M5Stack Cardputer Adv 是一款基于 ESP32-S3FN8 (Stamp-S3A) 的卡片式电脑。

## 硬件规格

| 组件 | 规格 |
|------|------|
| MCU | ESP32-S3FN8 @ 240MHz |
| Flash | 8MB |
| 显示屏 | ST7789V2 1.14" 240x135 |
| 音频编解码 | ES8311 |
| 功放 | NS4150B |
| 麦克风 | MEMS |
| 键盘 | 56键 (TCA8418) |
| IMU | BMI270 |
| 电池 | 1750mAh |

## 引脚定义

### 显示屏 (ST7789V2)
| 功能 | GPIO |
|------|------|
| MOSI | GPIO35 |
| SCLK | GPIO36 |
| CS | GPIO37 |
| DC | GPIO34 |
| RST | GPIO33 |
| BL | GPIO38 |

### 音频 (ES8311)
| 功能 | GPIO |
|------|------|
| I2C SDA | GPIO8 |
| I2C SCL | GPIO9 |
| I2S BCLK | GPIO41 |
| I2S LRCK | GPIO43 |
| I2S DOUT | GPIO46 |
| I2S DIN | GPIO42 |

## 使用方法

1. 按下 BOOT 按钮进入配网模式
2. 连接 WiFi 后即可使用语音助手功能

## 烧录参数

芯片: ESP32-S3, Flash: 8MB, 模式: DIO, 频率: 80MHz

| 地址 | 文件 |
|------|------|
| 0x0 | bootloader/bootloader.bin |
| 0x8000 | partition_table/partition-table.bin |
| 0xd000 | ota_data_initial.bin |
| 0x20000 | xiaozhi.bin |
| 0x600000 | generated_assets.bin |

烧录命令 (build 目录为 `build-cardputer-adv`):

```bash
python -m esptool --chip esp32s3 -b 460800 -p PORT \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m \
  0x0 build-cardputer-adv/bootloader/bootloader.bin \
  0x8000 build-cardputer-adv/partition_table/partition-table.bin \
  0xd000 build-cardputer-adv/ota_data_initial.bin \
  0x20000 build-cardputer-adv/xiaozhi.bin \
  0x600000 build-cardputer-adv/generated_assets.bin
```

将 `PORT` 替换为实际串口设备路径（如 `/dev/cu.usbmodem21101`）。

## 参考链接

- [M5Stack Cardputer Adv 官方文档](https://docs.m5stack.com/en/core/Cardputer-Adv)
