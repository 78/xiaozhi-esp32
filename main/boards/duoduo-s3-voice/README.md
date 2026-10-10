# Duoduo S3 Voice（多多语音板）

A compact ESP32-S3 voice board with expansion headers for a TFT display, a DVP
camera and an optional 4G Cat.1 modem — all three are plug-in modules; the
board works as a plain voice assistant with none of them attached. Audio needs
no codec chip: a PDM MEMS microphone and an I2S DAC + 5W class-D amplifier are
wired directly to the ESP32-S3, keeping the BOM minimal.

## 硬件组成

- 模块：ESP32-S3-WROOM-1，PCB 同一脚位兼容 N8R2 / N8R8 / N16R8（GPIO33-37 不引出，留给八线 PSRAM 模组），按所焊模组刷对应变体
- 麦克风：MP34DT05-A PDM 硅麦（64× 过采样，捕获 32kHz，固件重采样到 16kHz 供唤醒词引擎使用）
- 音频输出：PT8211 I2S DAC + NS4160 5W D 类功放（4Ω 喇叭，CTRL 引脚由 GPIO 控制静音）
- USB：Type-C 一线通——供电、烧录、调试（USB-JTAG console）
- 显示：8P 排针，SPI TFT 模块直插（ST7789/ILI9341/GC9A01，Kconfig 选型）
- 摄像头：2×8 排针，OV2640 等 8bit DVP 模块直插（默认 QVGA，R8 变体可提 VGA）
- 4G：排针接 ML307/EC801E Cat.1 模块（UART1），WiFi 默认、运行时可切换
- 指示灯：单色 LED；按键：BOOT（GPIO0，对地触发）

## 引脚定义

| 功能 | GPIO |
|---|---|
| 麦克风 PDM CLK / DATA | 4 / 5 |
| DAC BCLK / LRCK / DIN | 15 / 16 / 17 |
| 功放 CTRL（使能） | 6 |
| BOOT 按键 | 0 |
| 板载 LED | 45 |
| USB D- / D+ | 19 / 20 |
| 4G 模块 TX / RX | 43 / 44 |
| 摄像头 D0-D7 | 11 / 10 / 9 / 8 / 12 / 18 / 21 / 14 |
| 摄像头 XCLK / PCLK | 1 / 2 |
| 摄像头 VSYNC / HREF | 48 / 7 |
| 摄像头 SIOD / SIOC | 13 / 47 |
| 屏幕 MOSI / CLK / DC / CS / RST | 38 / 39 / 40 / 41 / 42 |
| 屏幕背光 | 46 |

GPIO26-32 为模组内 Flash；GPIO33-37 保留给 N8R8 八线 PSRAM，两者均不引出。其余 GPIO 全部分配完毕，仅 GPIO3 空闲（strap，预留测试点，上电后可作普通 GPIO/ADC 复用）。

## 构建与烧录

```sh
python3 scripts/build.py duoduo-s3-voice --name duoduo-s3-voice        # N8R2（Quad PSRAM, 8MB）
python3 scripts/build.py duoduo-s3-voice --name duoduo-s3-voice-r8     # N8R8（Octal PSRAM, 8MB）
python3 scripts/build.py duoduo-s3-voice --name duoduo-s3-voice-n16r8  # N16R8（Octal PSRAM, 16MB）
```

PSRAM 模式必须与模组一致（Quad 模组刷 Octal 固件会报 "PSRAM chip is not connected or wrong line mode"）；16MB flash 也可刷 8MB 变体（只用低 8MB）。

控制台走 USB-JTAG，`idf.py -p /dev/tty.usbmodem* flash monitor` 一根 Type-C 线完成。

## 行为说明

- BOOT 单击：未配网时进入配网模式；配网后切换对话状态
- BOOT 双击：启动/配网阶段在 WiFi 与 4G（ML307）之间切换（WiFi 为默认，选择持久化）
- 功放控制：NS4160 的 CTRL 引脚在 I2S 启动后拉高、停止前拉低，避免开关机爆音
- 摄像头默认 QVGA，兼顾 N8R2 的 2MB PSRAM（LVGL 缓冲 + AFE 模型）
- **模块全部选配**：未插摄像头仅打印错误日志；未插 4G 模块则始终走 WiFi（ML307 只有手动切换时才被探测）；未插屏幕则纯语音 + LED 状态，互不影响
