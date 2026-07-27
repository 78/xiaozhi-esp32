# LCD Wiki ES3C35P

3.5 寸 ESP32-S3 智能显示模块

资料文档: [https://www.lcdwiki.com/zh/3.5inch_ESP32-S3_Display](https://www.lcdwiki.com/zh/3.5inch_ESP32-S3_Display)

## 产品规格

| 项目 | 参数 |
|------|------|
| SKU | ES3C35P (带喇叭) / ES3C35P-NS (无喇叭) |
| 主控芯片 | ESP32-S3 Xtensa LX7 双核 240MHz |
| 存储 | 16MB QSPI Flash + 8MB OPI PSRAM (N16R8) |
| 无线 | Wi-Fi 2.4GHz 802.11b/g/n, 蓝牙 5.0 BLE |
| 工作电压 | 5V (USB Type-C 供电) |
| 电池 | 支持 3.7V 锂电池, TP4054 充电管理 |

### 显示屏

| 项目 | 参数 |
|------|------|
| 屏幕类型 | 3.5" IPS TFT |
| 分辨率 | 320×480 像素 |
| 色彩 | 65K RGB565 |
| 驱动 IC | ST77922 TDDI (触显一体) |
| 显示接口 | QSPI (接 ESP32-S3) |
| 触摸屏 | 电容触摸 (全贴合), I2C 0x55 |
| 背光 | PWM 控制, 亮度 300 cd/m² |

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
| RGB LED | WS2812B ×1 (IO40) |
| MicroSD | SDIO 4-bit |
| 充电管理 | TP4054, 电池 ADC (IO8) |
| 按键 | BOOT (IO0), RESET (EN) |

## 完整引脚分配

| GPIO | 功能 | 说明 |
|------|------|------|
| IO0 | BOOT 按键 | 开机按下=下载模式, 运行中=切换对话 |
| IO1 | PA 使能 | 低电平使能功放 |
| IO2 | SD D2 | SDIO DATA2 |
| IO3 | SD D3 | SDIO DATA3 |
| IO4 | SD CMD | SDIO 命令 |
| IO5 | SD CLK | SDIO 时钟 |
| IO6 | SD D0 | SDIO DATA0 |
| IO7 | SD D1 | SDIO DATA1 |
| IO8 | 电池 ADC | 电压检测 |
| IO9 | LCD D3 | QSPI 数据 D3 |
| IO10 | LCD CS | QSPI 片选 (低有效) |
| IO11 | LCD D0 | QSPI 数据 D0 |
| IO12 | LCD CLK | QSPI 时钟 |
| IO13 | LCD D1 | QSPI 数据 D1 |
| IO14 | LCD D2 | QSPI 数据 D2 |
| IO15 | I2S DOUT | 音频输出 |
| IO16 | I2S DIN | 音频输入 (MEMS 麦克风) |
| IO17 | I2S MCLK | I2S 主时钟 |
| IO18 | I2S BCLK | I2S 位时钟 |
| IO21 | I2S WS | I2S 左右声道 |
| IO38 | I2C SDA | 音频+触摸共用 |
| IO39 | I2C SCL | 音频+触摸共用 |
| IO40 | RGB LED | WS2812B 单线 |
| IO41 | 背光 | PWM 高电平点亮 |
| IO43 | UART RXD0 | 串口/普通 IO |
| IO44 | UART TXD0 | 串口/普通 IO |
| IO45 | GPIO45 | 扩展 IO |
| IO46 | GPIO46 | 扩展 IO |
| IO47 | 触摸 INT | 触摸中断 (低有效) |
| IO48 | 触摸 RST | 触摸复位 (低复位) |
| EN | LCD RST | 与 ESP32-S3 共用 |

### 显示 QSPI 引脚组

```
CS    = IO10          D0 = IO11
CLK   = IO12          D1 = IO13
                      D2 = IO14
                      D3 = IO9
```

### I2C 设备地址

| 设备 | 地址 | 总线 |
|------|------|------|
| ES8311 音频 | ES8311_CODEC_DEFAULT_ADDR | I2C0 |
| ST77922 触摸 | 0x55 | I2C0 (共享) |

## 编译

```bash
# 一键编译打包 (推荐)
python scripts/release.py lcdwiki-es3c35p

# 或手动编译
idf.py set-target esp32s3
idf.py menuconfig
# Xiaozhi Assistant -> Board Type -> LCD Wiki ES3C35P (3.5寸 ESP32-S3)
idf.py build flash monitor
```

## 关键配置说明

### 显示 ST77922 QSPI

| 配置项 | 值 | 原因 |
|--------|-----|------|
| `DISPLAY_WIDTH/HEIGHT` | 320×480 | 原生竖屏分辨率 |
| `DISPLAY_RGB_ORDER` | BGR | 面板实际色彩顺序 |
| `SPI_DMA_CH_AUTO` + `max_transfer_sz` | 51KB | SPI 总线 DMA，缓冲在 SRAM 避免 PSRAM 中转 |
| `buff_dma=1, buff_spiram=0` | SRAM DMA | SRAM 可直接 DMA 访问，无需中转缓冲 |
| `buffer_size` | width×80 (51KB) | 部分刷新缓冲 |
| `st77922_rounder_cb` | x 坐标 4 对齐 | ST77922 硬件要求，否则花屏 |

### 触摸 ST77922 内置

| 配置项 | 值 |
|--------|-----|
| I2C 地址 | 0x55 |
| 寄存器位宽 | 16 位 |
| 单点数据 | 7 字节 |
| 读取方式 | 必须一次性读 `7 × max_points` 字节清空缓冲 |

### 音频 ES8311

| 配置项 | 值 |
|--------|-----|
| PA 引脚 | IO1 |
| PA 极性 | 低电平使能 (`pa_inverted=true`) |
| I2C 总线 | I2C0 (与触摸共享 IO38/39) |
| 采样率 | 24000Hz 输入/输出 |

## 已知限制

1. **ST77922 不支持 `swap_xy`** — 只能使用原生 320×480 竖屏
2. **ST77922 要求 x 坐标 4 对齐** — 已通过 rounder_cb 自动处理
3. **触摸无 DMA/中断** — 使用轮询方式读取，约 30fps
4. **LVGL 缓冲在 SRAM** — 占用约 51KB，剩余 ~180KB 给其他模块

## 文件结构

```
main/boards/lcdwiki-es3c35p/
  config.h          — 引脚/硬件配置宏
  config.json       — 构建目标 (esp32s3, 16MB Flash)
  lcdwiki-es3c35p.cc — 板级初始化 (QSPI 显示 + I2C 触摸 + ES8311 音频 + LED)
  README.md         — 本文档
```
