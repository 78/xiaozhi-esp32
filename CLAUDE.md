# CLAUDE.md — xiaozhi-esp32

基于 MCP 协议的 ESP32 语音助手固件（AI 聊天机器人）。C++17 + ESP-IDF，设备端负责采集/编码音频、
离线唤醒词、显示与硬件控制；ASR/LLM/TTS 全部在服务器侧完成。

- 工程名 `xiaozhi`，版本见根 `CMakeLists.txt` 的 `PROJECT_VER`（当前 2.2.3）
- 要求 ESP-IDF >= 5.5.2；主要目标芯片 ESP32-C3 / S3 / P4（另有 ESP32、C5、C6）
- 分区表 v1/v2 **互不兼容**，无法跨版本 OTA

## 构建与烧录

```bash
idf.py set-target esp32s3
idf.py menuconfig          # Xiaozhi Assistant -> Board Type 选开发板
idf.py build
idf.py flash monitor
```

按板卡名自动编译（读 `main/boards/<name>/config.json` 的 target 与 sdkconfig_append）：

```bash
python scripts/release.py --list-boards      # 列出所有板卡/变体
python scripts/release.py <board-name>       # 编译该板全部变体，产物在 releases/
python scripts/release.py --name <variant>   # 只编指定变体
```

本仓库无单元测试框架；验证手段是编译 + 真机烧录观察串口日志。

## 目录结构

| 路径 | 作用 |
|---|---|
| `main/main.cc` | `app_main`：初始化 NVS，启动 Application |
| `main/application.{h,cc}` | **核心**。单例，持有事件组主循环、协议、音频服务、状态机 |
| `main/device_state*.{h,cc}` | 设备状态机（starting/idle/listening/speaking/...）与合法迁移表 |
| `main/boards/common/` | 板级基类：`Board` → `WifiBoard` / `Ml307Board` / `DualNetworkBoard`，及 button/backlight/i2c 等 |
| `main/boards/<name>/` | 各具体开发板；一板一目录 |
| `main/audio/` | `AudioService`（Opus 编解码 + 队列任务）、`codecs/`、`processors/`、`wake_words/` |
| `main/protocols/` | `Protocol` 基类 + WebSocket / MQTT+UDP 两种实现 |
| `main/display/` | `Display` 基类 + `LcdDisplay`/`OledDisplay`/`emote::EmoteDisplay` |
| `main/mcp_server.{h,cc}` | MCP 服务器：工具注册与 JSON-RPC 分发 |
| `main/ota.cc` | 激活、版本检查、固件升级、服务器下发配置 |
| `main/assets.{h,cc}` | 挂载 `assets` 分区（字体/表情/唤醒词模型/音效） |
| `scripts/` | `release.py` 构建发布、`gen_lang.py` 生成多语言头文件、`build_default_assets.py` 打包资源 |
| `docs/` | `custom-board.md`（新增板卡必读）、协议文档、`code_style.md` |
| `partitions/` | v1 / v2 分区表 CSV |

## 代码风格

遵循 `docs/code_style.md`，clang-format 配置见根目录 `.clang-format`（基于 Google 风格，4 空格缩进）。

- 文件名 `snake_case.cc`，类名 `PascalCase`，成员变量带尾下划线 `xxx_`
- 头文件用 `#ifndef _XXX_H_` 形式的 include guard
- 板级类通过文件末尾的 `DECLARE_BOARD(ClassName)` 宏注册
- 注释和日志中英文混用很常见；**源文件必须保存为 UTF-8**（项目其余文件均为 UTF-8）
- 提交信息风格：`feat:` / `fix:` / `chore:` 前缀，中英文都有

## 关键约定

### 线程模型（改动 UI / 音频时必读）
- 只有 **Application 主任务**可以改设备状态和 UI。其他线程一律用
  `Application::GetInstance().Schedule([]{...})` 投递回主任务
- LVGL 访问必须持锁，统一用 RAII 的 `DisplayLockGuard lock(display)`
- MCP 工具回调**已在主任务上下文执行**（`mcp_server.cc` 内部走 `Schedule`），可直接操作 UI
- `emote::EmoteDisplay` 的 `Lock/Unlock` 是空实现，线程安全由组件内部保证

### 新增开发板
1. 建 `main/boards/<name>/`：`config.h`（引脚/尺寸宏）、`<name>.cc`（板级类 + `DECLARE_BOARD`）、
   `config.json`（target / sdkconfig_append / builds）、`README.md`
2. 在 `main/Kconfig.projbuild` 的 `choice BOARD_TYPE` 内加一项
3. 在 `main/CMakeLists.txt` 的 `if(CONFIG_BOARD_TYPE_...)` 链里加分支，`set(BOARD_TYPE "<目录名>")`
   并选 `BUILTIN_TEXT_FONT` / `BUILTIN_ICON_FONT` / `DEFAULT_EMOJI_COLLECTION`
4. 板卡放在品牌子目录时，CMake 里额外 `set(MANUFACTURER "<品牌>")`，且 `config.json` 要写 `"manufacturer"`
5. 完整步骤见 `docs/custom-board.md`

### 音频链路
- 上行：MIC → AudioProcessor(AFE/裸) → Opus 编码 → 发送队列（16kHz 单声道，60ms 帧）
- 下行：解码队列 → Opus 解码 → 播放队列 → 扬声器（采样率由服务器 hello 下发覆盖）
- 板子只需实现 `AudioCodec` 子类并返回；编解码器在 `main/audio/codecs/`
- `CONFIG_USE_AUDIO_PROCESSOR` / `CONFIG_USE_DEVICE_AEC` / `CONFIG_USE_SERVER_AEC`
  / `CONFIG_USE_AFE_WAKE_WORD` 决定编译进哪套 processor 与 wake word 实现

### MCP 工具
- 通用工具加在 `mcp_server.cc` 的 `AddCommonTools()`；
  **板级工具必须加在该板的 `InitializeTools()` 里**
- 回调签名 `[](const PropertyList& p) -> ReturnValue`，返回 bool/int/string/cJSON*/ImageContent*，
  出错 `throw std::runtime_error(...)`
- 工具按 name 去重，重名只保留第一个

### 协议
- 走 WebSocket 还是 MQTT+UDP 由 OTA 响应下发的配置决定（`Application::InitializeProtocol`）
- 设备↔服务器消息统一带 `type` 字段：`hello`/`tts`/`stt`/`llm`/`mcp`/`system`/`alert`/`custom`
- MCP 复用音频通道的信令通道传输，与音频帧分离

## 本仓库的本地改动

`main/boards/cx-esp32s3/` 是本地新增的自研板（上游没有），当前仍在开发中：
LCD/I2C/XL9555 初始化代码被注释掉，`GetDisplay()` 未实现（走基类默认的 `NoDisplay`），
`config.h` 里仍是 ESP-BOX-3 的引脚宏，`README.md` 是 ESP-BOX-3 文案的副本。
`main/audio/codecs/no_audio_codec.h` 有本地改动，且被以 GBK 编码保存（其余文件为 UTF-8）。
`.github/` 下的 CI 配置在本仓库被删除。
