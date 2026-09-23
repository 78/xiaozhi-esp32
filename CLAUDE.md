# CLAUDE.md — xiaozhi-esp32 (cx-esp32s3 精简分支)

基于 MCP 协议的 ESP32 语音助手固件（AI 聊天机器人）。C++17 + ESP-IDF，设备端负责采集/编码音频、
离线唤醒词与硬件控制；ASR/LLM/TTS 全部在服务器侧完成。

**本仓库已被裁剪为只服务一块自研板 `cx-esp32s3`**（上游的 90+ 板卡、显示、摄像头、
多语言等均已删除，见文末"本仓库的本地改动"）。

- 工程名 `xiaozhi`，版本见根 `CMakeLists.txt` 的 `PROJECT_VER`（当前 2.2.3）
- 要求 ESP-IDF >= 5.5.2；**只支持 ESP32-S3**
- 分区表只用 `partitions/v2/16m.csv`（v1 已删除，且 v1/v2 互不兼容、无法跨版本 OTA）

## 构建与烧录

**必须用 PowerShell，不能在 Git Bash 里跑**（ESP-IDF 硬性拒绝 MSys/Mingw）。
从 Git Bash 调用时要先清掉 `MSYSTEM`，否则 `idf.py` 会静默不干活：

```bash
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "\$env:MSYSTEM=\$null; . 'C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1' | Out-Null; Set-Location 'D:\work_space\AI\xiaozhi-esp32\xiaozhi-esp32'; idf.py build"
```

在 PowerShell 里则直接：

```powershell
. C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

按板卡名自动编译（读 `main/boards/<name>/config.json` 的 target 与 sdkconfig_append）：

```bash
python scripts/release.py --list-boards      # 列出所有板卡/变体
python scripts/release.py cx-esp32s3         # 编译并打包，产物在 releases/
```

本仓库无单元测试框架；验证手段是编译 + 真机烧录观察串口日志。

## 目录结构

| 路径 | 作用 |
|---|---|
| `main/main.cc` | `app_main`：初始化 NVS，启动 Application |
| `main/application.{h,cc}` | **核心**。单例，持有事件组主循环、协议、音频服务、状态机 |
| `main/device_state_machine.{h,cc}` | 设备状态机（starting/idle/listening/speaking/...）与合法迁移表 |
| `main/boards/common/` | 板级基类 `Board` → `WifiBoard`，及 button/i2c/knob 等公共组件 |
| `main/boards/cx-esp32s3/` | **本仓库唯一的板卡**（见其 README.md） |
| `main/audio/` | `AudioService`（Opus 编解码 + 队列任务）、`codecs/`、`processors/`、`wake_words/` |
| `main/protocols/` | `Protocol` 基类 + WebSocket / MQTT+UDP 两种实现 |
| `main/display/` | 只剩 `Display` 基类 + `NoDisplay`（本板无屏） |
| `main/mcp_server.{h,cc}` | MCP 服务器：工具注册与 JSON-RPC 分发 |
| `main/ota.cc` | 激活、版本检查、固件升级、服务器下发配置 |
| `main/assets.{h,cc}` | 挂载 `assets` 分区（**唤醒词模型** + 音效） |
| `scripts/` | `release.py` 构建发布、`gen_lang.py` 生成多语言头文件、`build_default_assets.py` 打包资源 |
| `docs/` | `custom-board.md`（新增板卡必读）、协议文档、`code_style.md` |
| `partitions/v2/16m.csv` | 唯一在用的分区表 |

## 代码风格

遵循 `docs/code_style.md`，clang-format 配置见根目录 `.clang-format`（基于 Google 风格，4 空格缩进）。

- 文件名 `snake_case.cc`，类名 `PascalCase`，成员变量带尾下划线 `xxx_`
- 头文件用 `#ifndef _XXX_H_` 形式的 include guard
- 板级类通过文件末尾的 `DECLARE_BOARD(ClassName)` 宏注册
- 注释和日志中英文混用很常见；**源文件必须保存为 UTF-8**
- 提交信息风格：`feat:` / `fix:` / `chore:` 前缀，中英文都有

## 关键约定

### 线程模型
- 只有 **Application 主任务**可以改设备状态和界面。其他线程一律用
  `Application::GetInstance().Schedule([]{...})` 投递回主任务
- MCP 工具回调**已在主任务上下文执行**（`mcp_server.cc` 内部走 `Schedule`）
- 本板无屏，`Board::GetDisplay()` 返回基类默认的 `NoDisplay`（所有方法为空实现）

### 新增开发板
本仓库只维护一块板，新增板卡需要同时改三处（`Kconfig.projbuild` 的 `choice BOARD_TYPE`、
`main/CMakeLists.txt` 的 `if(CONFIG_BOARD_TYPE_...)` 分支、`main/boards/<name>/` 目录）。
完整步骤见 `docs/custom-board.md`——注意该文档是上游原文，其中显示/字体相关的部分已不适用。

### 音频链路
- 上行：MIC → AudioProcessor(AFE/裸) → Opus 编码 → 发送队列（16kHz 单声道，60ms 帧）
- 下行：解码队列 → Opus 解码 → 播放队列 → 扬声器（采样率由服务器 hello 下发覆盖）
- 板子只需实现 `AudioCodec` 子类并返回；本板用 `NoAudioCodecSimplexPdm`（PDM 采集 + 标准 I2S 播放）
- `CONFIG_USE_AUDIO_PROCESSOR` / `CONFIG_USE_DEVICE_AEC` / `CONFIG_USE_AFE_WAKE_WORD`
  决定编译进哪套 processor 与 wake word 实现

### ⚠️ 唤醒词链路（改 assets / 分区时必读）
`assets` 分区里装的不只是音效，还有**唤醒词模型**。完整链路：
`sdkconfig.defaults.esp32s3` 的 `CONFIG_SR_WN_*` → `scripts/build_default_assets.py` 打包出
`srmodels.bin` → `assets.cc` 的 `LoadSrmodelsFromIndex()` → `AudioService::SetModelsList()`
→ `wake_word_->Initialize()`。

**以下四处任何一处被破坏，设备都会静默地永远唤不醒（不报错）**：`build_default_assets_bin()`、
`Assets::LoadSrmodelsFromIndex`、`CONFIG_FLASH_DEFAULT_ASSETS`（不能设成 `FLASH_NONE_ASSETS`）、
以及 `assets` 分区本身。

### MCP 工具
- 通用工具加在 `mcp_server.cc` 的 `AddCommonTools()`
- 板级工具加在该板的 `InitializeTools()` 里
- 回调签名 `[](const PropertyList& p) -> ReturnValue`，返回 bool/int/string/cJSON*/ImageContent*，
  出错 `throw std::runtime_error(...)`
- 工具按 name 去重，重名只保留第一个

### 协议
- 走 WebSocket 还是 MQTT+UDP 由 OTA 响应下发的配置决定（`Application::InitializeProtocol`）
- 设备↔服务器消息统一带 `type` 字段：`hello`/`tts`/`stt`/`llm`/`mcp`/`system`/`alert`/`custom`
- MCP 复用音频通道的信令通道传输，与音频帧分离

## 本仓库的本地改动

这是上游 `78/xiaozhi-esp32` 的 fork，已裁剪为单板固件：

**板卡**：只保留 `main/boards/cx-esp32s3/`（自研板，上游没有）和 `main/boards/common/`，
其余 90+ 板卡目录连同其 Kconfig 条目与 CMake 分支一并删除。目标芯片只支持 ESP32-S3。

**已删除的功能与代码**：LCD / OLED / 表情动画 / LVGL（`display/` 只剩基类 + `NoDisplay`）、
摄像头（`esp32_camera` / `esp_video`）、I2C codec 芯片驱动（es8311/es8374/es8388/es8389/box）、
LED 驱动（单灯/灯带/GPIO 灯，本板 LED 是构造函数里直接 `gpio_set_level` 驱动）、
4G 模块（ML307/NT26/双网切换）、USB RNDIS 网卡、音频调试器、自定义唤醒词（Multinet）、
蓝牙配网（Blufi）、多语言（`main/assets/locales/` 只剩 `zh-CN` 和 `en-US`，后者是回退必需）、
分区表 v1、以及 `scripts/` 下的资源制作工具。相应的 `main/idf_component.yml` 依赖也已精简。

**保留但未编译**：电源管理三个文件（`adc_battery_monitor` / `axp2101` / `sy6970`）仍在磁盘上，
但在 `main/CMakeLists.txt` 里被注释掉了；需要时取消注释即可恢复（`idf_component.yml` 里的
`espressif/adc_battery_estimation` 也保留着）。

**其它本地改动**：`.github/` CI 配置与 `README_ja.md` / `README_zh.md` 已删除；
`main/audio/codecs/no_audio_codec.{h,cc}` 有本地改动，且 `.h` 被以 **GBK 编码**保存
（其余文件为 UTF-8），编辑时注意别把中文注释搞乱。

**已知待修**：`cx-esp32s3` 的主按键 `BOOT_BUTTON_GPIO` 仍是 ESP-BOX-3 遗留的 `GPIO_NUM_0`，
而真机上 KEY_M 在 IO15。
