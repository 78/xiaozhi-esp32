# xiaozhi-esp32 代码库概览

> 项目版本：v2.2.6（`CMakeLists.txt`）；分支策略：v1 维护至 2026-02，v2 不向下兼容（分区表变更）。

## 1. 这是什么

XiaoZhi（小智）是一款基于 **ESP32 系列 MCU** 的 **开源语音 AI 聊天机器人** 固件。语音作为主入口，背后接 Qwen / DeepSeek 等大模型，并通过 **MCP（Model Context Protocol）** 协议把"模型能力 ↔ 物理设备"双向打通：

- **Device-side MCP**：模型通过 MCP 调用设备本地能力（喇叭、LED、舵机、GPIO…）
- **Cloud-side MCP**：设备通过 MCP 调用云端能力（智能家居、PC 桌面操作、知识检索、邮件…）

默认连接官方服务器 `xiaozhi.me`（OTA URL：`https://api.tenclass.net/xiaozhi/ota/`）。

## 2. 技术栈

| 层 | 选型 |
|---|---|
| 框架 | ESP-IDF ≥ 5.4 |
| 语言 | C++（Google C++ Style） |
| RTOS | FreeRTOS |
| GUI | LVGL（lcd_display）+ 原生 OLED 驱动 + emote 动画显示 |
| 网络 | Wi-Fi、ML307 / NT26 Cat.1 4G、RNDIS |
| 音频编码 | OPUS（60ms 帧，16kHz 编 / 服务端动态采样率解） |
| 语音前端 | ESP-SR（Wakenet 唤醒 + AFE 降噪 + 可选 AEC） |
| 通信协议 | WebSocket 或 MQTT+UDP（二选一，二进制 OPUS + JSON 控制面） |
| 配网 | Hotspot / Acoustic（声波）/ ESP BluFi |
| 存储 | NVS（配置）、SPIFFS（assets：字体 / 表情 / 唤醒词模型） |

## 3. 目录结构

```
xiaozhi-esp32/
├── CMakeLists.txt                 # 顶层 ESP-IDF 工程（PROJECT_VER=2.2.6）
├── sdkconfig.defaults[.<chip>]    # 各芯片默认配置（esp32/c3/c5/c6/p4/s3）
├── partitions/{v1,v2}/            # 分区表（v2 与 v1 不兼容）
├── scripts/                       # 构建辅助：assets 打包、release、p3 音频工具、声波配网 HTML…
├── docs/                          # 协议与开发文档（websocket、mqtt-udp、mcp-protocol、custom-board…）
└── main/                          # ★ 全部固件源码
    ├── main.cc                    # app_main 入口：NVS init → Application::Initialize/Run
    ├── application.{cc,h}         # 单例事件循环 + 核心编排
    ├── device_state{,_machine}.*  # 设备状态机（idle / connecting / listening / speaking …）
    ├── protocol.h / protocols/    # WebSocket、MQTT 协议实现（统一抽象 Protocol 基类）
    ├── ota.*                      # OTA 升级 + 激活码逻辑
    ├── mcp_server.*               # 设备侧 MCP server，注册可被模型调用的本地工具
    ├── audio/                     # ★ 音频管道（详见下文）
    ├── display/                   # 显示抽象 + LCD/OLED/emote 三种实现
    ├── led/                       # single / circular / gpio LED 抽象
    ├── boards/                    # ★ 100+ 开发板适配（详见下文）
    ├── assets/                    # 字体/表情/唤醒词模型（按 locale 组织，38 种语言）
    └── settings.* / system_info.* # NVS 配置封装 / 系统信息查询
```

## 4. 运行时架构

### 4.1 启动链路

```
app_main (main.cc)
  └─ nvs_flash_init
  └─ Application::GetInstance().Initialize()   # 初始化 board / display / audio / ota / 协议回调
  └─ Application::Run()                         # 主事件循环，永不返回
```

### 4.2 主事件位（`application.h`）

`Application` 通过 FreeRTOS `EventGroup` 驱动一个单线程事件循环。其它任务（音频、网络、按键、唤醒）通过事件位 + `Schedule(callback)` 派发到主任务执行：

```
SCHEDULE / SEND_AUDIO / WAKE_WORD_DETECTED / VAD_CHANGE / ERROR
ACTIVATION_DONE / CLOCK_TICK / NETWORK_(CONNECTED|DISCONNECTED)
TOGGLE_CHAT / START_LISTENING / STOP_LISTENING / STATE_CHANGED
```

### 4.3 音频管道（`audio/audio_service.h`）

```
[MIC] → Processors(AFE/降噪) → encode_queue → [Opus Enc] → send_queue → [Server]
[Server] → decode_queue → [Opus Dec] → playback_queue → [Speaker]
```

任务划分：
- **AudioInputTask** + **AudioOutputTask**：负责 codec 与 processor
- **OpusCodecTask**：单独跑 Opus 编/解（CPU 密集，独立任务）

可插拔模块：
- `audio/codecs/`：ES8311 / ES8374 / ES8388 / ES8389 / box / dummy / no_audio
- `audio/processors/`：AFE 降噪 + audio_debugger（UDP 透传 PCM 调试）
- `audio/wake_words/`：`afe_wake_word`（Wakenet+AFE，S3/P4）/ `esp_wake_word`（无 AFE，C3/C5/C6/PSRAM ESP32）/ `custom_wake_word`（Multinet 自定义词）

### 4.4 协议层（`protocols/`）

`Protocol` 抽象基类，子类 `WebsocketProtocol` / `MqttProtocol`，统一回调接口：
- `OnIncomingAudio` / `OnIncomingJson` / `OnAudioChannelOpened/Closed` / `OnNetworkError`
- 二进制帧定义：`BinaryProtocol2`（带 timestamp 用于服务端 AEC）/ `BinaryProtocol3`
- 监听模式：`AutoStop` / `ManualStop` / `Realtime`（Realtime 需 AEC）

### 4.5 状态与交互

- `DeviceStateMachine` 维护设备状态，状态切换触发 `OnStateChanged` 回调更新 UI/LED
- `ListeningMode` × `AecMode(Off/DeviceSide/ServerSide)` 决定语音交互行为
- 唤醒后可选发送 wake-word 数据作为对话首包（`SEND_WAKE_WORD_DATA`）

## 5. 多板适配策略

`main/boards/` 下 **100 个子目录**，每个对应一种开发板，覆盖：

- **芯片家族**：ESP32 / S3 / C3 / C5 / C6 / P4
- **网络方案**：Wi-Fi / ML307 4G / NT26 4G / RNDIS
- **品牌生态**：Espressif（BOX/Korvo/SparkBot/Spot/HI/VoCat）、M5Stack、立创、LILYGO、Waveshare、正点原子、DFRobot、Movecall、SenseCAP…

公共能力在 `boards/common/` 提供：
- `board.{cc,h}` / `wifi_board` / `ml307_board` / `nt26_board` / `rndis_board` / `dual_network_board`
- 外设抽象：`button` / `knob` / `backlight` / `camera` (esp32_camera + esp_video) / `axp2101`(PMIC) / `sy6970`(充电) / `adc_battery_monitor`
- 系统能力：`power_save_timer` / `sleep_timer` / `system_reset` / `blufi`(BLE 配网) / `afsk_demod`(声波配网)

板型在 `Kconfig.projbuild` 通过 `BOARD_TYPE_xxx` choice 选择，并由 `IDF_TARGET_xxx` 约束可用项。

## 6. 资源与本地化

- **38 种语言** locale 在 `main/assets/locales/`（zh-CN/zh-TW/en-US/ja-JP/ko-KR/欧洲多语种/中东/东南亚…）
- **三种 Assets 烧录模式**：默认 / 自定义（本地或远程 URL）/ 表情专用（emote 风格）
- 字体、表情、唤醒词模型作为独立 SPIFFS 分区烧录

## 7. 关键开发者入口

| 想做的事 | 看哪里 |
|---|---|
| 加新开发板 | `docs/custom-board.md` + 模仿 `boards/<某个相近板>/` 复制改 |
| 改主流程/状态机 | `main/application.{cc,h}`、`device_state_machine.*` |
| 改音频链路 | `main/audio/audio_service.{cc,h}` 及 `codecs/` `processors/` `wake_words/` |
| 接新协议 | `main/protocols/protocol.h` 抽象 + 新增子类 |
| 加设备能力（让 LLM 能调用） | `main/mcp_server.{cc,h}` 注册 tool |
| 配置项 | `main/Kconfig.projbuild`（Board / Wake Word / AEC / Camera / 配网…） |
| 协议规范 | `docs/websocket.md`、`docs/mqtt-udp.md`、`docs/mcp-protocol.md` |

## 8. 构建与烧录概要

- 推荐：Linux + Cursor/VSCode + ESP-IDF 插件（SDK ≥ 5.4）
- `idf.py set-target esp32s3 && idf.py menuconfig`（选 Board / 唤醒词 / 显示风格）
- `idf.py build flash monitor`
- 新手可直接用预编译固件，无需搭环境（详见 `README.md` 的飞书 Wiki 链接）

---

> 一句话总结：**一份固件 × 100+ 开发板 × MCP 协议**，用单例 `Application` 事件循环串起音频管道、协议层、状态机和板级抽象，让 LLM 既能听说，也能动手。
