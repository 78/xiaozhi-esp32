# ESP-BOX-3

## 简介

<div align="center">
    <a href="https://github.com/espressif/esp-box"><b> ESP-BOX GitHub </b></a>
</div>

ESP-BOX-3 是乐鑫官方开发的 AIoT 开发套件，搭载 ESP32-S3-WROOM-1 模组，配备 2.4 英寸 320x240 ILI9341 显示屏，双麦克风阵列，支持离线语音唤醒与设备端回声消除（AEC）功能。

## 硬件特性

- **主控**: ESP32-S3-WROOM-1 (16MB Flash, 8MB PSRAM)
- **显示屏**: 2.4 英寸 IPS LCD (320x240, ILI9341)
- **音频**: ES8311 音频 Codec + ES7210 双麦 ADC
- **音频功能**: 支持设备端 AEC (回声消除)
- **按键**: Boot 按键 (单击/双击功能)
- **其他**: USB-C 供电与通信

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
- `Xiaozhi Assistant` → `Board Type` → 选择 `ESP BOX 3`

### UI风格选择

ESP-BOX-3 支持多种不同的 UI 显示风格，通过 menuconfig 配置选择：

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
   https://dl.espressif.com/AE/wn9_nihaoxiaozhi_tts-font_puhui_common_20_4-esp-box-3.bin
   ```

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

### 音频功能配置

#### 设备端回声消除 (AEC)
- `Xiaozhi Assistant` → `Enable Device-Side AEC` → 启用

ESP-BOX-3 硬件支持设备端 AEC 功能，可有效消除扬声器播放声音对麦克风的干扰，提升语音识别准确率。

**运行时切换**: 双击 Boot 按键可在运行时开启/关闭 AEC 功能。

> **说明**: 设备端 AEC 需要干净的扬声器输出参考路径和良好的麦克风与扬声器物理隔离才能正常工作。ESP-BOX-3 硬件已做优化设计。

### 唤醒词配置

ESP-BOX-3 支持多种唤醒词实现方式：

- `Xiaozhi Assistant` → `Wake Word Implementation Type` → 选择唤醒词类型

推荐选择：
- **Wakenet model with AFE** (`USE_AFE_WAKE_WORD`) - 支持 AEC 的唤醒词检测

按 `S` 保存，按 `Q` 退出。

**编译**

```bash
idf.py build
```

**烧录**

将 ESP-BOX-3 连接至电脑，并运行：

```bash
idf.py flash
```

## 按键说明

### Boot 按键功能

#### 单击
- **配网状态**: 进入 WiFi 配置模式
- **空闲状态**: 开始对话
- **对话中**: 打断或停止当前对话

#### 双击 (需启用设备端 AEC)
- **空闲状态**: 切换 AEC 开启/关闭

## 常见问题

### 1. 为什么需要设备端 AEC？
设备端 AEC 可以在本地实时消除扬声器播放声音对麦克风的干扰，在播放音乐或 TTS 回复时仍能准确识别语音指令。

### 2. 表情动画风格无法显示？
请确保已经配置了正确的自定义资源文件地址，并且设备能够访问该 URL 下载资源。

### 3. 如何恢复出厂设置？
长按 Boot 按键 3 秒以上，设备会清除所有配置并重启。
