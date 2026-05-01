# 使用说明 

* [M5Stack Tab5 docs](https://docs.m5stack.com/zh_CN/core/Tab5)

## 快速体验

到 [M5Burner](https://docs.m5stack.com/zh_CN/uiflow/m5burner/intro) 选择 Tab5 搜索小智下载固件

## 基础使用

* idf version: v5.5.2 or above (recommended: v6.0-dev)

* No dependency override needed — the project already specifies the correct `esp_video` and `esp_ipa` versions in `main/idf_component.yml`. Do NOT change the dependency versions unless you are also modifying the source code to match the older API.

针对 ESP32-P4 Rev <3.0 用户:
确保你的 sdkconfig.defaults 包含:

CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y

否则烧写的时候会出现：'bootloader/bootloader.bin' requires chip revision in range [v3.0 - v3.99] (this chip is revision v1.x)

1. 使用 release.py 编译

```shell
python ./scripts/release.py m5stack-tab5
```

如需手动编译，请参考 `m5stack-tab5/config.json` 修改 menuconfig 对应选项。

2. 编译烧录程序

```shell
idf.py flash monitor
```

> [!NOTE]
> 进入下载模式：长按复位按键（约 2 秒），直至内部绿色 LED 指示灯开始快速闪烁，松开按键。


## log

@2025/05/17 测试问题

1. listening... 需要等几秒才能获取语音输入???
2. 亮度调节不对
3. 音量调节不对
