---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '5e56f402-6bab-4b0b-b865-2db9638f9259'
  PropagateID: '5e56f402-6bab-4b0b-b865-2db9638f9259'
  ReservedCode1: 'd8c65e5a-b5b0-47dd-bd2d-b6e5e997a455'
  ReservedCode2: 'd8c65e5a-b5b0-47dd-bd2d-b6e5e997a455'
---

# 征辰科技 Minicam ML307

## 简介
征辰科技 Minicam ML307 是征辰科技推出的小智 AI 迷你摄像头开发板，支持 ML307 4G 模组联网和 Wi-Fi 双网络切换、摄像头识物、语音唤醒、语音打断、OTA 等功能。

## 硬件特性
- ESP32-S3 N16R8
- ES8388 音频编解码器
- OV系列摄像头
- ST7789 240x320 LCD 显示屏
- ML307 4G 模组（双网络切换）
- ADC 按键音量控制
- 电池电量检测

## 编译命令

```bash
python ./scripts/build.py zhengchen-minicam-ml307
```

如需手动编译，请参考 `config.json` 修改 menuconfig 对应选项。

## 烧录

```bash
idf.py flash
```

## MCP Tool
self.get_device_status
self.audio_speaker.set_volume
self.screen.set_brightness
self.screen.set_theme
self.display.set_mode
self.camera.capture
self.AEC.set_mode
self.AEC.get_mode
self.res.esp_restart