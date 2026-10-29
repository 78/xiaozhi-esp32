# 赛博喵伴 V2.0

## 简介

赛博喵伴 V2.0 是一款基于 ESP32-S3 的 AI 语音交互设备，配备 360×360 圆形触摸屏，
支持 Wi-Fi 联网、语音对话、离线语音唤醒配置和表情动画交互。

## 硬件与功能

- **主控**：ESP32-S3，16 MB Flash，支持 PSRAM。
- **音频**：ES8311 音频编解码器、ES7210 音频 ADC，24 kHz 语音采集与播放，提供回声参考通道。
- **显示**：ST77916 QSPI 圆形屏幕，360×360 分辨率，支持背光亮度调节。
- **触摸屏**：CST816S，触摸屏幕可切换对话状态。
- **头部触摸**：触摸后显示开心表情。
- **运动交互**：QMI8658A 运动传感器，摇晃后显示困惑表情。
- **电池状态**：电量和充放电状态显示，充电动画及 10% / 5% 低电量提醒。
- **按键**：BOOT 按键切换对话，电源键开关机。

V2.0 支持选配 USB UVC 摄像头，默认关闭。

引脚定义见本目录的 [config.h](config.h)。16 MB Flash 使用 `partitions/v2/16m.csv` 分区表，
用于存放应用程序、OTA 固件及资源文件。

## 配置、编译命令

使用 ESP-IDF `release/v6.1`，最低支持 v6.0.1。先加载 ESP-IDF 环境：

```bash
source /path/to/esp-idf/export.sh
idf.py --version
```

以下命令均在工程根目录执行。

### 配置编译目标

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

### 基本配置

在 `Xiaozhi Assistant` 菜单中设置：

- `Target Board` → `Cyberai-Vocat-V2.0`。
- `Default Language` → 选择所需语言，例如 `Chinese`。

### UI 风格选择

通过 `Xiaozhi Assistant` → `Select display style` 选择以下三种风格之一。
风格在编译时确定，切换后需要重新编译并烧录程序和资源。

#### 表情动画风格（Emote animation style）

显示眼睛动画、交互表情和状态图标。板级构建脚本默认使用此风格。

- `Select display style` → `Emote animation style`。
- `Flash Assets` → `Flash Emote Assets`。
- 搜索 `MMAP_FILE_NAME_LENGTH`，将 `Max file name length` 设置为 `32`。

构建时会自动生成表情资源并打包所选唤醒词模型，无需填写自定义资源下载地址。

#### 默认消息风格（Enable default message style）

使用标准的表情、状态栏和对话文字界面。

- `Select display style` → `Enable default message style`。
- `Flash Assets` → `Flash Default Assets`。

#### 微信消息风格（Enable WeChat Message Style）

使用聊天气泡形式显示对话。

- `Select display style` → `Enable WeChat Message Style`。
- `Flash Assets` → `Flash Default Assets`。

选择完成后，按 `S` 保存，按 `Q` 退出。

### 编译与烧录

```bash
idf.py build
idf.py flash monitor
```

烧录时连接 USB，并确保设备已开机。`idf.py flash` 会一起烧录程序和匹配的资源。
如需指定串口，可使用 `idf.py -p /dev/ttyACM0 flash monitor`，串口名称以实际设备为准。

### 使用板级构建脚本

也可以直接使用构建脚本生成表情动画风格的中文固件：

```bash
python3 scripts/build.py cyberai/cyber-vocat-v2.0 --name cyber-vocat-v2.0 --language zh-CN
idf.py flash monitor
```

构建脚本会重新生成板级配置，并自动选择兼容的唤醒词实现和模型。
通过 `menuconfig` 修改风格后，使用 `idf.py build` 保留手动配置。

## 使用说明

### 开关机

长按电源键开机，待设备启动后松开。
开机按键松开后，再次长按电源键约 2 秒即可关机，达到长按时间即触发，短按不关机。

### 低电自动关机

设备正常放电时，电量不高于 1% 且电压不高于 3.5 V，连续满足约 10 秒后自动关机。
充电、读数失败、电量或电压恢复时会取消计时；采样间隔超过 3 秒也会重新计时。
10% / 5% 低电量提醒继续保留。阈值可在 `config.h` 的 `LOW_BATTERY_SHUTDOWN_*` 中调整。

自动关机与长按关机使用相同的电源控制方式。USB 或调试电源可能使设备继续运行，
此时只执行一次关机动作，不反复触发。请在电池供电下验证实际断电效果。

### 语音与交互

- 根据屏幕提示完成 Wi-Fi 配网，联网后使用语音对话。
- BOOT 按键或触摸屏幕可切换对话状态；启动阶段触发可进入 Wi-Fi 配网。
- 触摸头部显示开心表情，摇晃设备显示困惑表情。
- 连接充电电源时显示充电状态，低电量时显示提醒；专用动画在表情动画风格下使用。
