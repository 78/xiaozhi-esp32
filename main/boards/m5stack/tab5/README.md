# 使用说明 

* [M5Stack Tab5 docs](https://docs.m5stack.com/zh_CN/core/Tab5)

## 快速体验

到 [M5Burner](https://docs.m5stack.com/zh_CN/uiflow/m5burner/intro) 选择 Tab5 搜索小智下载固件

## 基础使用

* ESP-IDF 最低版本：v6.0.1，推荐使用 `release/v6.1` 分支。

* ESP32-P4 的依赖要求 `esp_video >= 2.6.0, < 3.0.0`，由其引入 `esp_ipa 2.5.x`。
  `esp_ipa 2.5.0` 开始提供 Rev < 3 的专用预编译库，并根据
  `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` 选择对应库，用于处理
  [#2278](https://github.com/78/xiaozhi-esp32/issues/2278) 中摄像头初始化时的非法指令问题。
  仍需在受影响的 Rev 1.x 真机上验证启动和摄像头功能。

`build.py` 通过 variant 选择芯片版本：

- `m5stack-tab5` 面向 Rev < 3，并包含 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` 和 `CONFIG_ESP32P4_REV_MIN_100=y`。
- `m5stack-tab5-p4x` 面向 Rev >= 3。
- ESP-IDF 6 构建需要 `esp-sr` 2.4.7 或更高版本，两个芯片修订版均可用。
- 两个 variant 都启用 `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y`，避免 onboard C6 的 ESP-Hosted SDIO
  缓冲池在 ESP-IDF 6 上耗尽 internal RAM。

不要把 `m5stack-tab5-p4x` 固件强制刷入 Rev 1.x 芯片；应改用无后缀的 `m5stack-tab5` 固件。误用 P4X 固件时，正常情况下烧录工具会报告：`bootloader/bootloader.bin requires chip revision in range [v3.0 - v3.99] (this chip is revision v1.x)`。

Tab5 可能搭载 ILI9881C、ST7121 或 ST7123 面板。ST7121 与 ST7123 使用相同的触摸 I2C 地址，
固件会读取触摸控制器的 firmware version 来选择对应面板驱动和时序。板级初始化还会在探测面板前，
通过 PI4IOE 脉冲 `LCD_RST` 和 `TP_RST`。

1. 使用 build.py 编译

```shell
python ./scripts/build.py m5stack-tab5
```

测试 Rev 1.x 启动问题时，请确认生成的 `dependencies.lock` 中 `esp_video` 至少为 2.6.0、
`esp_ipa` 至少为 2.5.0，`sdkconfig` 包含 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`，
并在 `build/xiaozhi.map` 中确认链接的是 `libesp_ipa_p4_less_v3.a`
（使用 Newlib 时为 `libesp_ipa_p4_less_v3_newlib.a`）。请记录芯片 revision、
显示/触摸面板型号和完整启动日志，并测试冷启动、复位、摄像头拍照及语音对话。

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
