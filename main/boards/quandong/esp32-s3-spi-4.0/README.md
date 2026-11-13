# LCD Wiki ES3C40P

4.0 寸 ESP32-S3 智能显示模块

资料文档: [https://www.lcdwiki.com/zh/4.0inch_ESP32-S3_Display](https://www.lcdwiki.com/zh/4.0inch_ESP32-S3_Display)

## 产品规格

| 项目 | 参数 |
|------|------|
| SKU | ES3C40P |
| 主控芯片 | ESP32-S3 Xtensa LX7 双核 240MHz |
| 存储 | 16MB QSPI Flash + 8MB OPI PSRAM (N16R8) |
| 无线 | Wi-Fi 2.4GHz 802.11b/g/n, 蓝牙 5.0 BR/EDR + LE |
| 工作电压 | 5V (USB Type-C 供电) |
| 电池 | 支持 3.7V 锂电池, TP4054 充电管理 (最大 500mA, 实际约 290mA) |
| 模块尺寸 | 61 × 112 × 10.8 mm |

### 显示屏

| 项目 | 参数 |
|------|------|
| 屏幕类型 | 4.0" IPS TFT |
| 分辨率 | 320×480 像素 |
| 色彩 | 65K RGB565 |
| 驱动 IC | ST7796S |
| 显示接口 | 4-Line SPI (接 ESP32-S3) |
| 有效显示区 | 55.98 × 83.82 mm |
| 背光 | White LED×8, 典型亮度 450 cd/m² |

### 触摸屏

| 项目 | 参数 |
|------|------|
| 触摸类型 | 电容触摸屏 |
| 驱动 IC | FT6336U |
| 通信接口 | I2C (与 ES8311、IIC 外设接口共用 IO15/IO16) |
| 触摸分辨率 | 320×480 |
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
| IO13 | LCD MISO | SPI 读数据 |
| IO14 | 扩展 IO | 1.25mm 4P 座子 |
| IO15 | I2C SCL | 触摸+音频+外设共用 |
| IO16 | I2C SDA | 触摸+音频+外设共用 |
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
| EN | LCD / ESP RST | 液晶屏与主控共用复位 |

### 显示 SPI 引脚组

```
CS    = IO10          DC   = IO46
SCK   = IO12          MOSI = IO11
                      MISO = IO13
```

### I2C 设备地址

| 设备 | 地址 | 总线 |
|------|------|------|
| ES8311 音频 | ES8311_CODEC_DEFAULT_ADDR | I2C0 |
| FT6336U 触摸 | 0x38 (7 位) | I2C0 (共享) |

## 编译

```bash
# 一键编译打包 (推荐)
python scripts/build.py quandong/esp32-s3-spi-4.0 --zip

# 或手动编译
idf.py set-target esp32s3
idf.py menuconfig
# Xiaozhi Assistant -> Board Type -> Quandong ESP32-S3 4.0-inch SPI LCD (全动 4.0寸)
idf.py build flash monitor
```

## 关键配置说明

### 显示 ST7796S SPI

| 配置项 | 值 | 原因 |
|--------|-----|------|
| `DISPLAY_WIDTH/HEIGHT` | 320×480 | 原生竖屏分辨率 (`SWAP_XY=false`) |
| `DISPLAY_MIRROR_X/Y` | true / false | 面板实际画面方向 |
| SPI 时钟 | 40MHz | `spi_mode=0`, 8-bit 命令/参数 |
| 初始化序列 | 组件内置 ST7796 | 未使用 `vendor_config` 自定义命令 |
| `rgb_ele_order` | BGR | 面板实际色彩顺序 |
| `invert_color` | true | 面板需要反显 |
| `reset_gpio_num` | NC | 屏复位与 ESP32 EN 共用 |
| MISO | IO13 | 4 线 SPI, 总线保留读通道 |

### 触摸 FT6336U

| 配置项 | 值 |
|--------|-----|
| 驱动 | `esp_lcd_touch_ft5x06` (FT5x06 系列) |
| I2C 地址 | 0x38 (7 位, 驱动默认) |
| I2C 速率 | 400kHz |
| 坐标范围 | 320×480 |
| 坐标变换 | `swap_xy=0`, `mirror_x=0`, `mirror_y=0` |

### 音频 ES8311

| 配置项 | 值 |
|--------|-----|
| PA 引脚 | IO1 |
| PA 极性 | 低电平使能 (`InitializeAudioEnable()` 拉低) |
| I2C 总线 | I2C0 (与触摸共享 IO15/IO16) |
| 采样率 | 24000Hz 输入/输出 |

## 已知限制

1. **单按键交互** — 只有 BOOT 键, 无按住说话和物理音量键
2. **RGB LED / MicroSD / 电池 ADC 未启用** — 引脚已引出, 固件暂未驱动
3. **无独立屏复位** — 液晶屏复位与 ESP32 EN 共用
4. **I2C0 多设备共享** — ES8311、FT6336U 和 IIC 外设接口共用 IO15/IO16

## 文件结构

```
main/boards/quandong/esp32-s3-spi-4.0/
  config.h            — 引脚/硬件配置宏
  config.json         — 构建目标 (esp32s3, esp32-s3-spi-4.0)
  qd_4_0_lcd_board.cc — 板级初始化 (SPI 显示 + I2C 触摸 + ES8311 音频 + BOOT)
  README.md           — 本文档
```
