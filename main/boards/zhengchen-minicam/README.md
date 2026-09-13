---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '9ddb9977-3f86-4355-928d-21420c288f38'
  PropagateID: '9ddb9977-3f86-4355-928d-21420c288f38'
  ReservedCode1: 'ebf111d2-df34-48cc-9522-9e537f7e0408'
  ReservedCode2: 'ebf111d2-df34-48cc-9522-9e537f7e0408'
---

# 征辰科技 Minicam Wi-Fi

## 简介
征辰科技 Minicam Wi-Fi 是征辰科技推出的小智 AI 迷你摄像头开发板，支持 Wi-Fi 联网、摄像头识物、语音唤醒、语音打断、OTA 等功能。

## 硬件特性
- ESP32-S3 N16R8
- ES8388 音频编解码器
- OV系列摄像头
- ST7789 240x320 LCD 显示屏
- ADC 按键音量控制
- 电池电量检测

## 编译命令

```bash
python ./scripts/build.py zhengchen-minicam-wifi
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