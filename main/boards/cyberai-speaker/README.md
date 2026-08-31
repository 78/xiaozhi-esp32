# CyberAI Speaker (赛博AI SPK S3)

ESP32-S3 Wi-Fi 智能音箱板，带 NV3022B 横屏 LCD、ES8311 音频编解码器、TF 卡槽、电池监测与 WS2812 状态灯。

## 硬件概要

- 芯片：ESP32-S3
- 屏幕：296×240 NV3022B SPI LCD
- 音频：ES8311（I2S + I2C）
- 按键：BOOT、音量加、音量减
- 存储：TF 卡（1 线 SDMMC）
- 其他：电池 ADC 监测、充电检测（CHRG）、WS2812（GPIO45）

`config.h` 中 `USE_OLD_BOARD` 可在新旧硬件版本间切换引脚定义，默认使用新版（`USE_OLD_BOARD 0`）。

## 构建

```bash
python scripts/build.py cyberai-speaker --name cyberai-spk-s3-wifi
```

或在 menuconfig 中选择 **Xiaozhi Assistant → Board Type → CyberAI Speaker (赛博AI SPK S3)**。

## 可选功能

### SD 卡长文件名（中文文件名）

`config.json` 已默认启用以下 FAT 配置，支持 TF 卡中文长文件名：

- **Long filename support**：`Long filename support in heap`
- **OEM Code Page**：`Simplified Chinese (DBCS) (CP936)`
- **API character encoding**：`API uses UTF-8 encoding`

如需手动调整，可在 menuconfig 的 **Component config → FAT Filesystem support** 中修改。


## 引脚说明

详见 [`config.h`](config.h)。
