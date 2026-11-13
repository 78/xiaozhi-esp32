# LCD Wiki ES3C28P / ES3N28P

2.8 寸 ESP32-S3 智能显示模块

资料文档: [https://www.lcdwiki.com/zh/2.8inch_ESP32-S3_Display](https://www.lcdwiki.com/zh/2.8inch_ESP32-S3_Display)

## 产品规格

| 项目 | 参数 |
|------|------|
| SKU | ES3C28P (带触摸) / ES3N28P (无触摸) |
| 主控芯片 | ESP32-S3 Xtensa LX7 双核 240MHz |
| 存储 | 16MB QSPI Flash + 8MB OPI PSRAM (N16R8) |
| 无线 | Wi-Fi 2.4GHz 802.11b/g/n, 蓝牙 5.0 BR/EDR + LE |
| 工作电压 | 5V (USB Type-C 供电) |
| 电池 | 支持 3.7V 锂电池, TP4054 充电管理 |
| 模块尺寸 | 50.0 × 86.0 × 10.6 mm (带触摸) |

### 显示屏

| 项目 | 参数 |
|------|------|
| 屏幕类型 | 2.8" IPS TFT |
| 分辨率 | 240×320 像素 |
| 色彩 | 65K RGB565 (最大 262K RGB666) |
| 驱动 IC | ILI9341V |
| 显示接口 | 4-Line SPI (接 ESP32-S3) |
| 有效显示区 | 43.20 × 57.60 mm |
| 背光 | White LED×4, 典型亮度 280 cd/m² |

### 触摸屏

| 项目 | 参数 |
|------|------|
| 触摸类型 | 电容触摸屏 (ES3C28P) |
| 驱动 IC | FT6336G |
| 通信接口 | I2C (与 I2C 外设接口共用 IO15/IO16) |
| 触摸分辨率 | 240×320 |
| 复位/中断 | RST = IO18, INT = IO17 |

### 音频

| 项目 | 参数 |
|------|------|
| 编解码芯片 | ES8311 |
| 功放 | FM8002E |
| 麦克风 | MEMS 麦克风 (LMA2718B381) |
| 音频接口 | I2S (双工: 输入+输出) |
| 采样率 | 输入/输出 24000Hz |

### 外设

| 项目 | 型号/接口 |
|------|-----------|
| RGB LED | WS2812B ×1 (IO42) |
| MicroSD | SDIO 4-bit |
| 充电管理 | TP4054, 电池 ADC (IO9) |
| 按键 | BOOT (IO0), RESET (EN) |
| 扩展接口 | IO2 / IO3 / IO14 / IO21 |

## 完整引脚分配

| GPIO | 功能 | 说明 |
|------|------|------|
| IO0 | BOOT 按键 | 开机按下=下载模式, 运行中=切换对话 |
| IO1 | PA 使能 | 低电平使能功放 |
| IO2 | 扩展 IO | 1.25mm 4P 座子 |
| IO3 | 扩展 IO | 1.25mm 4P 座子 |
| IO4 | I2S MCLK | I2S 主时钟 |
| IO5 | I2S BCLK | I2S 位时钟 |
| IO6 | I2S DIN | 音频输入 (MEMS 麦克风) |
| IO7 | I2S WS | I2S 左右声道 |
| IO8 | I2S DOUT | 音频输出 |
| IO9 | 电池 ADC | 电压检测 |
| IO10 | LCD CS | SPI 片选 (低有效) |
| IO11 | LCD MOSI | SPI 写数据 |
| IO12 | LCD SCK | SPI 时钟 |
| IO13 | LCD MISO | SPI 读数据 (本固件未使用) |
| IO14 | 扩展 IO | 1.25mm 4P 座子 |
| IO15 | I2C SCL | 音频+触摸+外设共用 |
| IO16 | I2C SDA | 音频+触摸+外设共用 |
| IO17 | 触摸 INT | 触摸中断 (低有效) |
| IO18 | 触摸 RST | 触摸复位 (低复位) |
| IO21 | 扩展 IO | 1.25mm 4P 座子 |
| IO38 | SD CLK | SDIO 时钟 |
| IO39 | SD D0 | SDIO DATA0 |
| IO40 | SD CMD | SDIO 命令 |
| IO41 | SD D1 | SDIO DATA1 |
| IO42 | RGB LED | WS2812B 单线 |
| IO43 | UART RXD0 | 串口/普通 IO |
| IO44 | UART TXD0 | 串口/普通 IO |
| IO45 | 背光 | PWM 高电平点亮 |
| IO46 | LCD DC | 命令/数据选择 |
| IO47 | SD D3 | SDIO DATA3 |
| IO48 | SD D2 | SDIO DATA2 |
| EN | LCD RST | 与 ESP32-S3 共用 |

### 显示 SPI 引脚组

```
CS    = IO10          DC   = IO46
SCK   = IO12          MOSI = IO11
                      MISO = IO13 (本固件未使用)
```

### I2C 设备地址

| 设备 | 地址 | 总线 |
|------|------|------|
| ES8311 音频 | ES8311_CODEC_DEFAULT_ADDR | I2C0 |
| FT6336G 触摸 | 0x38 (7 位) | I2C0 (硬件共用, 本固件未启用) |

## 编译

```bash
# 一键编译打包 (推荐)
python scripts/build.py quandong/esp32-s3-spi-2.8 --zip

# 或手动编译
idf.py set-target esp32s3
idf.py menuconfig
# Xiaozhi Assistant -> Board Type -> Quandong ESP32-S3 2.8-inch SPI LCD (全动 2.8寸)
idf.py build flash monitor
```

## 关键配置说明

### 显示 ILI9341V SPI

| 配置项 | 值 | 原因 |
|--------|-----|------|
| `DISPLAY_WIDTH/HEIGHT` | 320×240 | 默认横屏 (`SWAP_XY=true`, `MIRROR_Y=true`) |
| 竖屏模式 | 240×320 | 将 `config.h` 中横屏宏注释掉, 启用竖屏宏即可 |
| SPI 时钟 | 40MHz | `spi_mode=0`, 8-bit 命令/参数 |
| 初始化序列 | 厂家自定义 `vendor_specific_init` | gamma / power 时序 |
| `rgb_ele_order` | RGB | 面板实际色彩顺序 |
| `invert_color` | true | 面板需要反显 |
| `reset_gpio_num` | NC | 屏复位与 ESP32 EN 共用 |
| MISO | 未使用 | 只写显示, SPI 总线 `miso_io_num=NC` |

### 音频 ES8311

| 配置项 | 值 |
|--------|-----|
| PA 引脚 | IO1 |
| PA 极性 | 低电平使能 (`InitializeAudioPaEnable()` 拉低) |
| I2C 总线 | I2C0 (IO15/IO16) |
| 采样率 | 24000Hz 输入/输出 |

## 已知限制

1. **触摸未接入固件** — ES3C28P 板载 FT6336G, 但当前板级代码未初始化触摸 (`config.h` 无 TOUCH 宏), 仅作显示使用
2. **RGB LED / MicroSD / 电池 ADC 未启用** — 引脚已引出, 固件暂未驱动
3. **单按键交互** — 只有 BOOT 键, 无按住说话和物理音量键
4. **MISO 未使用** — 显示只走写通道, 不影响刷新

## 文件结构

```
main/boards/quandong/esp32-s3-spi-2.8/
  config.h                 — 引脚/硬件配置宏
  config.json              — 构建目标 (esp32s3, quandong-s3-dev-2.8-lcd)
  quandong_s3_dev_board.cc — 板级初始化 (SPI 显示 + ES8311 音频 + BOOT)
  HARDWARE_DIFF.md         — 与 bread-compact-wifi 的硬件差异说明
  README.md                — 本文档
```
