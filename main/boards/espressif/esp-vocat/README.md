# ESP-VoCat 喵伴

## 简介

<div align="center">
    <a href="https://oshwhub.com/esp-college/echoear"><b> 立创开源平台 </b></a>
</div>

ESP-VoCat 喵伴是一款智能 AI 开发套件，搭载 ESP32-S3-WROOM-1 模组，1.85 寸 QSPI 圆形触摸屏，双麦阵列，支持离线语音唤醒与声源定位算法。硬件详情等可查看[立创开源项目](https://oshwhub.com/esp-college/echoear)。

## 配置、编译命令

**配置编译目标为 ESP32S3**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig 并配置**

```bash
idf.py menuconfig
```

分别配置如下选项：

### 基本配置
- `Xiaozhi Assistant` → `Board Type` → 选择 `Espressif ESP-VoCat`

### UI风格选择

ESP-VoCat 支持多种不同的 UI 显示风格，通过 menuconfig 配置选择：

- `Xiaozhi Assistant` → `Select display style` → 选择显示风格

#### 可选风格

##### 表情动画风格 (Emote animation style) - 推荐
- **配置选项**: `USE_EMOTE_MESSAGE_STYLE`
- **特点**: 使用自定义的 `EmoteDisplay` 表情显示系统
- **功能**: 支持丰富的表情动画、眼睛动画、状态图标显示
- **适用**: 智能助手场景，提供更生动的人机交互体验
- **类**: `emote::EmoteDisplay`

**⚠️ 重要**: 选择此风格需要额外配置自定义资源文件：
1. `Xiaozhi Assistant` → `Flash Assets` → 选择 `Flash Custom Assets`
2. `Xiaozhi Assistant` → `Custom Assets File` → 填入资源文件地址：
   ```
   https://dl.espressif.com/AE/wn9_nihaoxiaozhi_tts-font_puhui_common_20_4-echoear.bin
   ```

   这是保留表情动画和模型的历史资源包；新版固件会忽略其中没有 Noto 元数据的旧 Puhui
   字体，并使用内置 Noto basic 字体。待新的 Noto 资源包上传后可直接替换此 URL。

##### 默认消息风格 (Enable default message style)
- **配置选项**: `USE_DEFAULT_MESSAGE_STYLE` (默认)
- **特点**: 使用标准的消息显示界面
- **功能**: 传统的文本和图标显示界面
- **适用**: 标准的对话场景
- **类**: `SpiLcdDisplay`

##### 微信消息风格 (Enable WeChat Message Style)
- **配置选项**: `USE_WECHAT_MESSAGE_STYLE`
- **特点**: 仿微信聊天界面风格
- **功能**: 类似微信的消息气泡显示
- **适用**: 喜欢微信风格的用户
- **类**: `SpiLcdDisplay`

> **说明**: ESP-VoCat 喵伴使用16MB Flash，需要使用专门的分区表配置来合理分配存储空间给应用程序、OTA更新、资源文件等。

按 `S` 保存，按 `Q` 退出。

**编译**

```bash
idf.py build
```

**烧录**

将 ESP-VoCat 喵伴连接至电脑，**注意打开电源**，并运行：

```bash
idf.py flash
```

## Peripheral I2C error handling

Battery register reads run on the battery worker, with at most three attempts
for timeout/invalid-response/transfer failures, a 50 ms transfer timeout and
10/20 ms retry delays. Other errors return immediately. Successful sampling
runs every two seconds; a failed cycle backs off for five seconds and does not
trigger battery emotes. The worker's own failure warning is limited to once
per ten seconds; driver logging is unchanged.

Battery status queries use a mutex-protected, complete last-good snapshot
instead of performing transfers in UI callbacks. Samples expire after ten
seconds; absent/stale readings return `false`, never a fabricated empty battery.
Touch performs one checked transfer and skips gesture processing on failure,
so a failed read cannot synthesize a release. The first valid battery sample
establishes the initial charging state without a false charger-insertion event.

This contains peripheral read errors; it does not diagnose power instability,
reset the shared bus, change codec startup or disable watchdog/brownout checks.
Temperature sensor handling is unchanged. Host regression tests use an I2C
failure double and do not replace physical fault injection:

```bash
python3 -m unittest discover -s scripts/tests -p test_esp_vocat_i2c.py -v
```
