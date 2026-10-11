# 小智 ESP32 项目结构与运行流程

## 项目概述

这是一个 **ESP32 智能语音助手** 固件项目（"小智"），基于 ESP-IDF 框架开发。核心功能是：语音唤醒 → 采集音频 → 通过 MQTT/WebSocket 发送到云端 AI 服务器 → 接收 TTS 语音播放 + 表情显示。

---

## 目录结构

### 顶层文件

| 文件 | 功能 |
|------|------|
| CMakeLists.txt | 项目根构建文件，项目名 `xiaozhi`，版本 2.1.0 |
| README.md / README_zh.md | 中英文项目说明 |
| .gitignore | Git 忽略规则 |
| LICENSE | 开源协议 |

### main/ — 核心固件代码

入口及顶层模块：

| 文件 | 功能 |
|------|------|
| main.cc | **程序入口**，初始化 NVS/网络/Application，然后调用 `Run()` 进入主事件循环 |
| application.h / application.cc | **应用主控制器**（单例），是整个系统的调度中心。管理设备状态机、事件循环（FreeRTOS EventGroup）、音频服务、协议层、OTA 升级、告警/通知 |
| device_state.h | 定义设备状态枚举：Unknown → Starting → WifiConfiguring → Idle → Connecting → Listening → Speaking → Upgrading → Activating → AudioTesting → FatalError |
| device_state_machine.h / device_state_machine.cc | **设备状态机**，管理状态转换规则，支持观察者模式通知状态变化 |
| settings.h / settings.cc | NVS 持久化设置读写 |
| ota.h / ota.cc | **OTA 固件升级**，版本检查、固件下载、激活码验证 |
| mcp_server.h / mcp_server.cc | **MCP (Model Context Protocol) 服务端**，管理设备端工具注册。云端可通过 JSON-RPC 调用设备端工具（如拍照、获取传感器数据等） |
| system_info.h / system_info.cc | 系统信息收集（堆内存、芯片型号、固件版本等），生成 User-Agent |
| assets.h / assets.cc | **资源管理**，管理 assets 分区的资源文件下载、解压和应用 |
| Kconfig.projbuild | ESP-IDF 菜单配置定义（板子类型、语言、协议、AEC 模式等） |

### main/audio/ — 音频子系统

| 文件 | 功能 |
|------|------|
| audio_codec.h / audio_codec.cc | **音频编解码器抽象接口**，定义 EnableInput/EnableOutput/Read/Write 等接口 |
| audio_service.h / audio_service.cc | **音频服务核心**，管理 3 个 FreeRTOS 任务：音频输入、音频输出、Opus 编解码。数据流：MIC → 处理器 → Opus 编码 → 发送队列 → 服务器；服务器 → 解码队列 → Opus 解码 → 播放队列 → 扬声器 |
| audio_processor.h | 音频处理器抽象接口（AEC、VAD 等前置处理） |
| wake_word.h | 唤醒词检测抽象接口 |

#### main/audio/codecs/ — 音频编解码器实现

| 文件 | 功能 |
|------|------|
| es8311_audio_codec.cc | ES8311 音频 DAC/ADC 驱动 |
| es8374_audio_codec.cc | ES8374 音频 ADC 驱动 |
| es8388_audio_codec.cc | ES8388 音频编解码器驱动（最常用） |
| es8389_audio_codec.cc | ES8389 音频编解码器驱动 |
| box_audio_codec.cc | ESP-BOX 系列板载音频驱动 |
| dummy_audio_codec.cc | 空实现（无音频硬件） |
| no_audio_codec.cc | 无音频编解码器 |

#### main/audio/processors/ — 音频处理器

| 文件 | 功能 |
|------|------|
| afe_audio_processor.cc | **ESP-SR AFE 音频处理器**（回声消除 AEC、降噪、VAD、唤醒词），基于 Espressif AFE 框架 |
| no_audio_processor.cc | 空处理器 |
| audio_debugger.cc | 音频调试工具 |

#### main/audio/wake_words/ — 唤醒词

| 文件 | 功能 |
|------|------|
| afe_wake_word.cc | AFE 自带唤醒词引擎（ESP32S3/P4 可用） |
| custom_wake_word.cc | 自定义唤醒词（MultiNet 模型） |
| esp_wake_word.cc | ESP 通用唤醒词 |

### main/boards/ — 硬件板卡支持

这是项目最大的目录，支持 **100+ 种** ESP32 开发板。

#### main/boards/common/ — 板级公共组件

| 文件 | 功能 |
|------|------|
| board.h | **Board 抽象基类**，定义所有板卡必须实现的接口（音频编解码器、显示器、LED、网络、电源管理等） |
| wifi_board.cc / wifi_board.h | **WiFi 联网板卡基类**，WiFi 连接/配网/BLUFI 配网 |
| ml307_board.cc / ml307_board.h | **ML307 4G 模块联网板卡基类**（蜂窝网络） |
| dual_network_board.cc / dual_network_board.h | **双网络板卡基类**（WiFi + 4G 双模切换） |
| board.cc | Board 基类默认实现 |
| button.cc / button.h | 物理按键管理（单击/双击/长按） |
| backlight.cc / backlight.h | LCD 背光控制 |
| knob.cc / knob.h | 旋钮编码器输入 |
| esp32_camera.cc / camera.h | ESP32 摄像头驱动 |
| adc_battery_monitor.cc / adc_battery_monitor.h | ADC 电池电量检测 |
| axp2101.cc / axp2101.h | AXP2101 电源管理芯片驱动 |
| sy6970.cc / sy6970.h | SY6970 电池充电管理芯片驱动 |
| power_save_timer.cc / power_save_timer.h | 低功耗定时器 |
| sleep_timer.cc / sleep_timer.h | 休眠定时器 |
| system_reset.cc / system_reset.h | 系统复位 |
| afsk_demod.cc / afsk_demod.h | AFSK 音频解调（用于声波配网） |
| blufi.cpp / blufi.h | 蓝牙 BLUFI 配网 |
| press_to_talk_mcp_tool.cc / press_to_talk_mcp_tool.h | 按键对讲 MCP 工具 |
| lamp_controller.h | 灯光控制器接口 |
| i2c_device.cc / i2c_device.h | I2C 设备通用操作 |

#### main/boards/<板名>/ — 各板卡适配

每个子目录对应一种硬件板卡，包含：
- `config.h` — 板级引脚定义（I2C 引脚、I2S 引脚、按键引脚、LCD 引脚等）
- `<板名>.cc` — 板级实现（继承自 wifi_board / ml307_board / dual_network_board 等）

常见板卡举例：

| 目录 | 对应硬件 |
|------|----------|
| bread-compact-wifi/ | 面包板 DIY ESP32 WiFi 版 |
| bread-compact-ml307/ | 面包板 DIY ESP32 + 4G 版 |
| esp-box-3/ / esp-box/ / esp-box-lite/ | 乐鑫 ESP-BOX 系列 |
| m5stack-core-s3/ | M5Stack CoreS3 |
| atoms3-echo-base/ | M5Stack AtomS3 + Echo Base |
| lilygo-t-circle-s3/ | LILYGO T-Circle-S3 |
| lilygo-t-cameraplus-s3/ | LILYGO T-CameraPlus S3 |
| waveshare-s3-* / waveshare-c6-* | 微雪系列板卡 |
| esp-hi/ | ESP-HI 桌面机器人 |
| electron-bot/ | Electron 桌面机器人 |
| otto-robot/ | Otto 机器人 |
| kevin-* | Kevin 系列板卡 |
| atk-dnesp32* | 正点原子系列板卡 |
| du-chatx/ | 都 ChatX 板卡 |
| lichuang-dev/ | 立创开发板 |
| xmini-c3/ | 小迷你 C3 |
| movecall-moji*/ | Movecall Moji 系列 |
| esp-sparkbot/ | ESP SparkBot |

### main/protocols/ — 通信协议

| 文件 | 功能 |
|------|------|
| protocol.h / protocol.cc | **协议抽象基类**，定义音频发送/接收、JSON 消息收发、音频通道管理等接口 |
| mqtt_protocol.h / mqtt_protocol.cc | **MQTT 协议实现**，通过 MQTT 与云端通信，支持 BinaryProtocol v2/v3 二进制协议和 UDP 模式 |
| websocket_protocol.h / websocket_protocol.cc | **WebSocket 协议实现**，通过 WebSocket 与云端通信 |

### main/display/ — 显示子系统

| 文件 | 功能 |
|------|------|
| display.h / display.cc | **Display 抽象基类**，定义 SetStatus/SetEmotion/SetChatMessage/UpdateStatusBar 等接口 |
| lcd_display.h / lcd_display.cc | **LCD 通用显示**实现（含 LVGL 支持），管理状态栏、聊天消息、表情、通知等 UI 元素 |
| oled_display.h / oled_display.cc | OLED 显示屏实现（SSD1306 等） |
| emote_display.h / emote_display.cc | 表情动画显示（用于 ESP-HI 等带动态表情的设备） |
| lvgl_display/ | LVGL 图形库相关：emoji 表情集合、主题、字体、GIF/JPG 解码 |

### main/led/ — LED 控制

| 文件 | 功能 |
|------|------|
| led.h | LED 抽象接口 |
| single_led.cc | 单颗 LED 控制（WS2812 / GPIO LED） |
| gpio_led.cc | GPIO 直驱 LED |
| circular_strip.cc | 环形 LED 灯带控制 |

### main/assets/ — 内置资源文件

| 路径 | 内容 |
|------|------|
| common/ | 通用音效（exclamation.ogg, popup.ogg, success.ogg 等） |
| locales/<语言>/ | 各语言语音包（数字 0-9、welcome、activation、upgrade 等 OGG 音频 + language.json） |

### scripts/ — 工具脚本

| 文件/目录 | 功能 |
|------|------|
| build_default_assets.py | 构建默认资源包（字体、表情、ESP-SR 模型） |
| gen_lang.py | 从 language.json 生成 C 头文件（Lang::Strings / Lang::Sounds） |
| release.py | 固件发布打包脚本 |
| versions.py | 版本管理 |
| download_github_runs.py | 下载 GitHub Actions 构建产物 |
| ogg_converter/ | OGG 音频格式转换工具 |
| p3_tools/ | P3 音频格式工具集 |
| acoustic_check/ | 声学检测工具 |
| spiffs_assets/ | SPIFFS 资源打包工具 |
| Image_Converter/ | LVGL 图片格式转换工具 |
| audio_debug_server.py | 音频调试服务器 |
| sonic_wifi_config.html | 声波 WiFi 配网页面 |

### docs/ — 文档

| 文件 | 内容 |
|------|------|
| mcp-protocol.md | MCP 协议说明 |
| mcp-usage.md | MCP 使用指南 |
| mqtt-udp.md | MQTT UDP 模式说明 |
| websocket.md | WebSocket 协议说明 |
| custom-board.md | 自定义板卡开发指南 |
| blufi.md | BLUFI 蓝牙配网说明 |
| v0/ / v1/ | 各版本硬件接线图/照片 |

### .github/ — CI/CD

| 文件 | 功能 |
|------|------|
| workflows/build.yml | GitHub Actions 自动构建固件 |
| ISSUE_TEMPLATE/ | Issue 模板（构建问题/运行时 bug/功能请求） |

---

## 整体数据流

```
[按键/触摸/唤醒词] → Application (事件循环)
                          ↓
[MIC] → AudioCodec → AudioProcessor(AEC/VAD) → Opus Encoder → SendQueue
                                                                    ↓
                                          MQTT/WebSocket Protocol → 云端 AI 服务器
                                                                    ↓
[Speaker] ← Opus Decoder ← DecodeQueue ← MQTT/WebSocket Protocol ←  TTS 音频 + JSON (情绪/文字/MCP)
                          ↓
                    Display (表情/文字/状态栏)
```

核心循环在 `Application::Run()` 中，通过 FreeRTOS 事件组驱动：网络连接 → 激活 → 就绪 → 唤醒词检测 → 连接云端 → 监听 → AI 回复 → 播放 TTS，循环往复。

---

## 运行流程图

### 程序入口 app_main() (main.cc)

```
app_main()
  ├─ 1. 初始化 NVS Flash (WiFi 配置存储)
  │     - 若 NVS 损坏则自动擦除重建
  │
  ├─ 2. Application::Initialize()
  │     ├─ 创建 Board 单例 (根据 CONFIG_BOARD_TYPE 选择具体板卡)
  │     ├─ 初始化 Display (LCD/OLED)
  │     ├─ 初始化 AudioService (音频编解码器 + 创建 3 个 FreeRTOS 任务)
  │     │    ├─ audio_input_task      (MIC → 处理器 → 编码队列)
  │     │    ├─ audio_output_task     (解码队列 → Opus → 扬声器)
  │     │    └─ opus_codec_task       (PCM → Opus 编码 / Opus → PCM 解码)
  │     ├─ 注册音频回调 (on_wake_word / on_vad / on_send_queue)
  │     ├─ 注册状态机回调 (on_state_change)
  │     ├─ 启动 1 秒周期时钟 (更新状态栏)
  │     ├─ 初始化 MCP Server (注册设备端工具)
  │     └─ Board.StartNetwork() ← 异步启动网络
  │
  └─ 3. Application::Run()  ← 主事件循环 (永不返回)
        │
        xEventGroupWaitBits(ALL_EVENTS) ← 阻塞等待任意事件
        │
        事件包括:
        ├─ NETWORK_CONNECTED      → 网络连上了
        ├─ NETWORK_DISCONNECTED   → 网络断开了
        ├─ ACTIVATION_DONE        → 激活完成
        ├─ WAKE_WORD_DETECTED     → 检测到唤醒词
        ├─ VAD_CHANGE             → 语音活动检测状态变化
        ├─ TOGGLE_CHAT            → 按键短按 (切换对话状态)
        ├─ START_LISTENING        → 按键长按 (开始监听)
        ├─ STOP_LISTENING         → 按键松开 (停止监听)
        ├─ STATE_CHANGED          → 设备状态发生变化
        ├─ SEND_AUDIO             → 有编码后的音频待发送
        ├─ SCHEDULE               → 有回调任务需要主线程执行
        ├─ CLOCK_TICK             → 每秒定时器 (更新状态栏, 打印内存)
        └─ ERROR                  → 发生错误
```

---

### 阶段 A：网络连接流程

```
Board.StartNetwork()
      │
      ▼
┌──────────────────────────┐
│  WiFi 配网检查            │
│  - 有已保存 WiFi → 直接连  │
│  - 无保存 → 进入配网模式    │
│    (BLE BLUFI / 声波配网)  │
└──────────────────────────┘
      │
      ├── Scanning ──→ UI 显示 "正在扫描WiFi"
      ├── Connecting ──→ UI 显示 "连接中..."
      │
      ▼
┌──────────────────────────┐
│  NETWORK_CONNECTED 事件    │
│  HandleNetworkConnected() │
└──────────────────────────┘
      │
      ▼  (当前状态 == Starting 或 WifiConfiguring)
┌──────────────────────────┐
│  状态 → kDeviceStateActivating │
│  创建 activation 任务      │
└──────────────────────────┘
```

---

### 阶段 B：激活流程 (ActivationTask)

```
ActivationTask()  ← 在独立 FreeRTOS 任务中运行
      │
      ├── 1. CheckAssetsVersion()
      │      - 检查是否需要下载新资源包 (字体/表情/语音)
      │      - 有更新 → 下载 → 应用 → 可能进入 Upgrading 状态
      │
      ├── 2. CheckNewVersion()
      │      - 向服务器检查固件版本
      │      - 有新版本 → 下载固件 → 自动升级 → 重启
      │      - 无新版本 → 标记当前版本有效
      │      - 有激活码 → 显示激活码 → 语音播报 → 轮询等待激活
      │      - (最多重试 10 次, 退避延迟加倍)
      │
      └── 3. InitializeProtocol()
             - 根据服务器返回选择 MQTT 或 WebSocket
             - 注册协议层回调:
               ├─ OnIncomingAudio  → PushPacketToDecodeQueue
               ├─ OnIncomingJson   → 解析 TTS/STT/LLM/MCP/System 消息
               ├─ OnAudioChannelOpened  → 开高性能模式
               ├─ OnAudioChannelClosed  → 转低功耗模式 → 回 Idle
               ├─ OnConnected      → 关闭告警
               └─ OnNetworkError   → 触发错误事件
             - protocol_->Start() → 连接云端服务器
      │
      ▼
发送 MAIN_EVENT_ACTIVATION_DONE 事件
```

---

### 阶段 C：就绪状态 (Idle)

```
HandleActivationDone()
      │
      ▼
┌───────────────────────────────────────────┐
│  状态 → kDeviceStateIdle                   │
│  - 显示版本号通知                           │
│  - 播放成功提示音                           │
│  - 释放 OTA 对象                           │
│  - 进入低功耗模式                           │
│  - 启动唤醒词检测 (EnableWakeWordDetection)  │
│  - UI: 状态栏="待机", 表情="neutral"         │
└───────────────────────────────────────────┘
      │
      ▼  (等待用户交互)
┌───────────────────────────────────────────┐
│  可触发的操作:                              │
│  - 唤醒词 → WAKE_WORD_DETECTED             │
│  - 按键短按 → TOGGLE_CHAT                  │
│  - 按键长按 → START_LISTENING              │
│  - 按键松开 → STOP_LISTENING               │
└───────────────────────────────────────────┘
```

---

### 阶段 D：语音对话流程（核心）

```
┌──────────────────────────────────────────────────────────┐
│                    用户说唤醒词 / 按键                       │
└──────────────────────────────────────────────────────────┘
      │
      ▼
HandleWakeWordDetected() / HandleToggleChat()
      │
      ├── 检查状态 == kDeviceStateIdle
      │
      ├── 1. audio_service_.EncodeWakeWord()
      │      (编码唤醒词的 PCM 音频数据)
      │
      ├── 2. protocol_->OpenAudioChannel()
      │      (如果音频通道未打开)
      │      状态 → kDeviceStateConnecting
      │      UI: "连接中..."
      │
      ├── 3. 通道打开成功后:
      │      - 发送唤醒词音频数据给服务器 (如果启用)
      │      - protocol_->SendWakeWordDetected(wake_word)
      │
      └── 4. SetListeningMode()
              │
              ├── AEC 关闭 → kListeningModeAutoStop
              │               (检测到静音自动停止)
              └── AEC 开启 → kListeningModeRealtime
                              (持续实时对话)
      │
      ▼
┌───────────────────────────────────────────┐
│  状态 → kDeviceStateListening              │
│  - protocol_->SendStartListening(mode)    │
│  - audio_service_.EnableVoiceProcessing   │
│    (启动 AEC/VAD/降噪)                     │
│  - audio_service_.EnableWakeWordDetection  │
│    (关闭唤醒词，避免误触发)                  │
│  - 播放提示音 (popup.ogg)                  │
│  - UI: "聆听中...", LED 变化               │
└───────────────────────────────────────────┘
      │
      │  MIC 采集音频 → 音频处理器 → Opus 编码 → MQTT/WS → 云端
      │
      ▼
┌───────────────────────────────────────────┐
│  云端处理并返回:                             │
│                                            │
│  ┌─ STT (语音识别)                         │
│  │    type:"stt", text:"用户说的话"          │
│  │    → UI 显示用户消息                      │
│  │                                         │
│  ├─ LLM (情绪)                              │
│  │    type:"llm", emotion:"happy"          │
│  │    → UI 显示对应表情                      │
│  │                                         │
│  ├─ TTS (语音合成)                          │
│  │    type:"tts", state:"start"            │
│  │    → 状态 → kDeviceStateSpeaking        │
│  │    → 后端持续推送 Opus 音频包             │
│  │    → AudioService 解码 → 扬声器播放       │
│  │    type:"tts", state:"stop"             │
│  │    → AutoStop 模式 → 回到 Listening      │
│  │    → ManualStop 模式 → 回到 Idle         │
│  │                                         │
│  ├─ MCP (设备控制)                           │
│  │    type:"mcp", payload:{...}            │
│  │    → McpServer.ParseMessage()           │
│  │    → 执行工具调用 (拍照/获取传感器等)       │
│  │                                         │
│  └─ System (系统命令)                        │
│       type:"system", command:"reboot"      │
│       → 重启设备                             │
└───────────────────────────────────────────┘
      │
      ▼
┌───────────────────────────────────────────┐
│  对话结束 → AudioChannel 关闭               │
│  - 回到低功耗模式                           │
│  - 状态 → kDeviceStateIdle                 │
│  - 重新启用唤醒词检测                        │
│  - 等待下一轮对话                            │
└───────────────────────────────────────────┘
```

---

### 阶段 E：特殊状态转换

```
┌──────────────────────────────────────────────────────┐
│  按键切换状态 (TOGGLE_CHAT)                            │
│                                                      │
│  Idle       ──→ 打开音频通道 → Connecting → Listening │
│  Speaking   ──→ AbortSpeaking → 中断播放               │
│  Listening  ──→ CloseAudioChannel → Idle              │
│  Activating ──→ 取消激活 → Idle                        │
│  WifiConfig  ──→ 进入音频测试模式                       │
│  AudioTest   ──→ 退出音频测试 → WifiConfig              │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│  正在播放时检测到唤醒词:                                │
│  Speaking ──→ AbortSpeaking(WakeWord) ──→ Listening   │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│  网络断开:                                            │
│  Connecting/Listening/Speaking ──→ CloseAudioChannel  │
│  → 等待重连 → 重新激活                                 │
└──────────────────────────────────────────────────────┘
```

---

### 音频数据流（双工）

```
上行 (MIC → 云端):
┌──────┐    ┌─────────────┐    ┌───────────┐    ┌──────────┐    ┌──────────┐
│ MIC  │───→│ AudioCodec  │───→│ Processor  │───→│ Opus     │───→│ Protocol │
│      │    │ (ES8388等)  │    │ (AEC/VAD) │    │ Encoder  │    │ (MQTT/WS)│
└──────┘    └─────────────┘    └───────────┘    └──────────┘    └────┬─────┘
                                                                     │
                                                                     ▼
                                                                 ☁️ 云端 AI

下行 (云端 → 扬声器):
☁️ 云端 AI
     │
     ▼
┌──────────┐    ┌───────────┐    ┌─────────────┐    ┌──────────┐
│ Protocol │───→│ Opus      │───→│ AudioCodec  │───→│ Speaker  │
│ (MQTT/WS)│    │ Decoder   │    │ (ES8388等)  │    │          │
└──────────┘    └───────────┘    └─────────────┘    └──────────┘
```

---

### 状态机完整图

```
                    ┌──────────┐
                    │ Unknown  │
                    └────┬─────┘
                         │
                    ┌────▼─────┐
                    │ Starting │
                    └────┬─────┘
                         │
              ┌──────────┼──────────┐
              │          │          │
         ┌────▼────┐ ┌──▼───┐ ┌───▼──────┐
         │Activating│ │Wifi  │ │FatalError│
         └────┬────┘  │Config│ └──────────┘
              │        └──┬───┘
              │           │
         ┌────▼────┐ ┌───▼──────┐
         │Upgrading│ │AudioTest │
         └─────────┘ └──────────┘
              │
         ┌────▼────┐
         │  Idle   │◄──────────────────────┐
         └───┬──┬──┘                       │
             │  │                          │
    ┌────────┘  └────────┐                 │
    │                    │                 │
┌───▼──────────┐  ┌──────▼──────┐          │
│ Connecting   │  │ AutoStop → Listening ──┤
└──────┬───────┘  │ ManualStop→ Listening──┘
       │          └──────┬──────┘
       │                 │
       │          ┌──────▼──────┐
       └─────────→│  Speaking   │
                  └─────────────┘
```
