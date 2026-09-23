# CX-ESP32S3

自研开发板，基于 ESP32-S3。**无显示屏（headless）**。

## 硬件特性

- **主控**: ESP32-S3（含 PSRAM）
- **音频输出**: NS4168 功放（I2S）
- **音频输入**: PDM 数字麦克风
- **NFC**: MFRC522 读卡器（SPI）
- **交互**: 1 个主按键（KEY_M）+ 1 个指示灯
- **其他**: USB Type-C 供电与通信

## 引脚分配

引脚真值来源为项目外的 `pinout_0805_latest.md`（与仓库同级目录）。以下为当前代码实际使用的引脚：

| 功能 | 信号 | GPIO | 说明 |
|---|---|---|---|
| 音频输出 | I2S_BCLK | IO46 | → NS4168 BCLK |
| 音频输出 | I2S_LRCK | IO9 | → NS4168 LRCLK |
| 音频输出 | I2S_DOUT | IO8 | → NS4168 SDATA |
| 功放使能 | AMP_CTRL | IO10 | 代码中置高 |
| 麦克风 | PDM_CLK | IO3 | → 数字麦克风 CLK |
| 麦克风 | PDM_DATA | IO42 | ← 数字麦克风 DATA |
| 按键 | KEY_M | IO15 | 内部上拉，按下接地 |
| 指示灯 | LED_CTRL | IO18 | 低电平点亮 |
| NFC | CS / SCK / MOSI / MISO | IO11 / IO12 / IO13 / IO14 | SPI |
| NFC | RST / IRQ | IO21 / IO16 | |
| USB | D- / D+ | IO19 / IO20 | 经 33Ω 电阻 |
| 系统 | BOOT | IO0 | 下载模式 |
| PSRAM | — | IO35 / IO36 / IO37 | **严禁外部引用** |

闲置引脚：IO1、IO2、IO4、IO5、IO6、IO7、IO17、IO38、IO39、IO40、IO41、IO45、IO47、IO48。

## 音频实现

使用 `NoAudioCodecSimplexPdm`（见 `main/audio/codecs/no_audio_codec.h`），
播放走标准 I2S、采集走 PDM，无外部编解码芯片。

构造参数顺序为 `(输入采样率, 输出采样率, spk_bclk, spk_ws, spk_dout, spk_slot_mask, mic_sck, mic_din)`，
对应 `GPIO_NUM_46, GPIO_NUM_9, GPIO_NUM_8, I2S_STD_SLOT_RIGHT, GPIO_NUM_3, GPIO_NUM_42`。

## 编译烧录

本机 ESP-IDF 装在 `C:\esp\v5.5.5\esp-idf`。**注意 IDF 不能在 Git Bash 里跑**，需用 PowerShell：

```powershell
. C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## 已知问题

- **主按键接错引脚**：`config.h` 里 `BOOT_BUTTON_GPIO` 仍是 `GPIO_NUM_0`（ESP-BOX-3 的遗留值），
  所以单击切对话实际挂在 IO0（下载键）上，板上真正的 KEY_M（IO15）未被使用。待修。
- `config.h` 整体是 ESP-BOX-3 的遗留文件，其中 `AUDIO_I2S_*`、`AUDIO_CODEC_*`、`DISPLAY_*` 宏
  均未被 `.cc` 引用；真正生效的只有 `AUDIO_INPUT_SAMPLE_RATE`、`AUDIO_OUTPUT_SAMPLE_RATE`、
  `BOOT_BUTTON_GPIO`。建议清理。
