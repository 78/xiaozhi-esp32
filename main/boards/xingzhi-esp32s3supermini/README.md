# 星智 ESP32-S3 SuperMini 开发板

基于 ESP32-S3 SuperMini 的面包板开发方案，使用 INMP441 全向麦克风 + MAX98357A I2S 功放 + SSD1306 OLED 屏幕。

## 硬件清单

| 组件 | 型号 | 说明 |
|---|---|---|
| 主控 | ESP32-S3 SuperMini | 4MB Flash, WiFi/BLE |
| 麦克风 | INMP441 | I2S 全向 MEMS 麦克风 |
| 功放 | MAX98357A | I2S D 类功放 (内置 PA) |
| 显示屏 | SSD1306 | I2C OLED (128x32 或 128x64) |

## 接线图

```
ESP32-S3 SuperMini (左排引脚)     INMP441           MAX98357A        SSD1306 OLED
──────────────────────────       ────────           ──────────       ──────────
  3.3V ──────────────────► VDD ─────────────────────────► VDD ───────► VCC
  GND  ──────────────────► GND ─────────────────────────► GND ───────► GND

  GPIO1 ───────────────────────────────────────────────────────────────► SDA
  GPIO2 ───────────────────────────────────────────────────────────────► SCL

  GPIO4 ──────────────────► WS ──────────────────────────► LRC
  GPIO5 ──────────────────► SCK──────────────────────────► BCLK
  GPIO6 ◄────────────────── DOUT                        ◄──────────── DIN
         L/R ──► GND (左声道)    SD ──► VCC    GAIN ── 悬空 (15dB)
```

## 引脚分配 (全部在左排 GPIO1~8, 面包板无需跨排)

| GPIO | 功能 | 连接目标 |
|---|---|---|
| GPIO0 | BOOT 按钮 | 板载 (内部) |
| GPIO1 | I2C SDA | SSD1306 SDA |
| GPIO2 | I2C SCL | SSD1306 SCL |
| GPIO4 | I2S WS | INMP441 WS + MAX98357A LRC (共享) |
| GPIO5 | I2S BCLK | INMP441 SCK + MAX98357A BCLK (共享) |
| GPIO6 | I2S DIN | INMP441 DOUT (麦克风数据输入) |
| GPIO7 | I2S DOUT | MAX98357A DIN (喇叭数据输出) |
| GPIO8 | 空闲 | 可接按钮或传感器 |

## 音频模式

默认使用 **Duplex 模式**（共享 I2S 总线），只需左排 6 根 GPIO。
如需切换到 **Simplex 模式**（独立 I2S 总线，录音播放互不干扰），需额外接 GPIO15/16（对面一排），在 config.h 中取消 `AUDIO_I2S_METHOD_SIMPLEX` 的注释。

## 编译

### 方法一：release.py 脚本（推荐）

```bash
python scripts/release.py xingzhi-esp32s3supermini
```

### 方法二：idf.py 手动编译

```bash
idf.py set-target esp32s3
idf.py fullclean
idf.py menuconfig
# 进入 Xiaozhi Assistant → Board Type → 选择 "无名科技星智ESP32S3SuperMini"
idf.py build
idf.py flash monitor
```

```
python3 scripts/release.py xingzhi-esp32s3supermini
idf.py -p /dev/ttyACM0 flash monitor
```

## 配置说明

- 如果你的 OLED 是 **128x32**，修改 `config.h` 中 `DISPLAY_HEIGHT` 为 `32`
- 如果你的 OLED 地址是 **0x3D**，修改 `supermini_board.cc` 中 `dev_addr` 为 `0x3D`
