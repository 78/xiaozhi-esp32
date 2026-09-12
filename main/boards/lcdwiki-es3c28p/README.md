# LCD Wiki ES3C28P

2.8 寸 ESP32-S3 智能显示模块

资料文档: [https://www.lcdwiki.com/zh/2.8inch_ESP32-S3_Display](https://www.lcdwiki.com/zh/2.8inch_ESP32-S3_Display)

## 产品规格

| 项目 | 参数 |
|------|------|
| SKU | ES3C28P (带触摸屏) / ES3N28P (无触摸屏) |
| 主控芯片 | ESP32-S3 Xtensa LX7 双核 240MHz |
| 存储 | 16MB QSPI Flash + 8MB OPI PSRAM (N16R8) |
| 无线 | Wi-Fi 2.4GHz 802.11b/g/n, 蓝牙 5.0 BLE |
| 工作电压 | 5V (USB Type-C 供电) |
| 电池 | 支持 3.7V 锂电池, TP4054 充电管理 |

### 显示屏

| 项目 | 参数 |
|------|------|
| 屏幕类型 | 2.8" IPS TFT |
| 分辨率 | 240×320 像素 |
| 色彩 | 65K RGB565 (最大 262K RGB666) |
| 驱动 IC | ILI9341V |
| 显示接口 | 4-Line SPI (接 ESP32-S3) |
| 触摸屏 | FT6336G 电容触摸, I2C 0x38 |
| 背光 | PWM 控制, 亮度 230~280 cd/m² |

### 音频

| 项目 | 参数 |
|------|------|
| 编解码芯片 | ES8311 (I2C 0x18) |
| 功放 | SC8002B |
| 麦克风 | MEMS 麦克风 (LMA2718B381-OA7) |
| 音频接口 | I2S (双工: 输入+输出) |
| 采样率 | 输入/输出 24000Hz |
| 喇叭 | 1.25mm 2P 座子外接 |

### 外设

| 项目 | 型号/接口 |
|------|-----------|
| RGB LED | 单线 RGB 三色灯 ×1 (IO42) |
| MicroSD | SDIO 4-bit |
| 充电管理 | TP4054, 电池 ADC (IO9) |
| 按键 | BOOT (IO0), RESET (EN) |
| 串口 | 1.25mm 4P 座子 (IO43/IO44) |

## 完整引脚分配

| GPIO | 功能 | 说明 |
|------|------|------|
| IO0 | BOOT 按键 | 开机按下=下载模式, 运行中=切换对话 |
| IO1 | PA 使能 | 低电平使能功放 |
| IO2 | 扩展 IO | 1.25mm 4P 扩展座子 |
| IO3 | 扩展 IO | 1.25mm 4P 扩展座子 |
| IO4 | 音频 MCLK | I2S 主时钟 |
| IO5 | 音频 BCLK | I2S 位时钟 |
| IO6 | 音频 ASDOUT | ES8311 ADC → ESP32 (麦克风输入) |
| IO7 | 音频 LRCK | I2S 左右声道选择 |
| IO8 | 音频 DSDIN | ESP32 → ES8311 DAC (喇叭输出) |
| IO9 | 电池 ADC | ADC1_CH8, 200k/200k 分压 |
| IO10 | LCD CS | 片选, 低电平有效 |
| IO11 | LCD MOSI | SPI 写数据 |
| IO12 | LCD SCK | SPI 时钟 |
| IO13 | LCD MISO | SPI 读数据 |
| IO14 | 扩展 IO | 1.25mm 4P 扩展座子 |
| IO15 | I2C SCL | ES8311 / FT6336G / 扩展 I2C 共用 |
| IO16 | I2C SDA | ES8311 / FT6336G / 扩展 I2C 共用 |
| IO17 | 触摸 INT | 触摸事件时输入低电平 |
| IO18 | 触摸 RST | 低电平复位 |
| IO21 | 扩展 IO | 1.25mm 4P 扩展座子 |
| IO38 | SD CLK | SDIO 时钟 |
| IO39 | SD D0 | SDIO DATA0 |
| IO40 | SD CMD | SDIO 命令 |
| IO41 | SD D1 | SDIO DATA1 |
| IO42 | RGB LED | 单线 RGB 三色灯 |
| IO43 | UART RXD0 | 串口接收 |
| IO44 | UART TXD0 | 串口发送 |
| IO45 | LCD 背光 | 高电平点亮 |
| IO46 | LCD DC | 高=数据, 低=命令 |
| IO47 | SD D3 | SDIO DATA3 |
| IO48 | SD D2 | SDIO DATA2 |
| EN | 复位 | ESP32-S3 与 LCD 共用复位 |

> **注意**: LCD 复位与 ESP32-S3 复位共用 EN 引脚, 因此 `DISPLAY_RST_PIN` 配置为 `GPIO_NUM_NC`。

## 编译

```bash
python scripts/build.py lcdwiki-es3c28p --name lcdwiki-es3c28p
```

## 与 ES3C35P 的差异

同厂商的 3.5 寸模块 (`lcdwiki-es3c35p`) 使用 ST77922 QSPI 触显一体屏, 引脚分配完全不同,
因此两者是独立的板子定义, 不可混用固件。

| 项目 | ES3C28P (2.8") | ES3C35P (3.5") |
|------|----------------|----------------|
| 分辨率 | 240×320 | 320×480 |
| 驱动 IC | ILI9341V | ST77922 |
| 显示总线 | 4-Line SPI | QSPI |
| 触摸 IC | FT6336G (0x38) | ST77922 内置 (0x55) |
| I2C 引脚 | SDA=IO16, SCL=IO15 | SDA=IO38, SCL=IO39 |
| 音频 MCLK/BCLK/WS | IO4 / IO5 / IO7 | IO17 / IO18 / IO21 |
| RGB LED | IO42 | IO40 |
| 背光 | IO45 | IO41 |
