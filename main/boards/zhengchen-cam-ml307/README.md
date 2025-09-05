# 产品相关介绍网址

https://e.tb.cn/h.6Gl2LC7rsrswQZp?tk=qFuaV9hzh0k CZ356
```
【淘宝】 「小智AI带摄像头支持识物双麦克风打断 ESP32S3N16R8开发板表情包」
https://e.tb.cn/h.hBc8Gcx9cUluJJO?tk=YW5C4LPixKg



## 配置、编译命令

由于此项目需要配置较多的 sdkconfig 选项，推荐使用编译脚本编译。

**编译**

```bash
python ./scripts/release.py zhengchen-cam
```

如需手动编译，请参考 `zhengchen-cam/config.json` 修改 menuconfig 对应选项。

**烧录**

```bash
idf.py flash


```

MCP Tool：
self.get_device_status
self.audio_speaker.set_volume
self.screen.set_brightness
self.screen.set_theme
self.gif.set_gif_mode
self.display.set_mode
self.camera.take_photo       
self.AEC.set_mode
self.AEC.get_mode
self.res.esp_restart