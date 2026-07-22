# 集成 Agora RTC 协议到小智 AI 聊天机器人

## 概述

将 Agora RTC（Real-Time Audio）协议集成到 xiaozhi-esp32 项目中，作为除 WebSocket 和 MQTT+UDP 之外的第三种通信协议选项。设备通过声网 RTC 进行音频传输（上行/下行），通过 RTM 进行信令交互，通过 Device API 进行设备配对、绑定和对话管理。

```
Device ←→ Agora RTC (音频 PCM 16kHz mono 16-bit)
          + RTM (JSON 信令)
          + HTTP Device API (配对 / 对话管理)
          ←→ 声网 ConvoAI 服务
```

## 前置条件

- xiaozhi-esp32 项目，commit `331d69b` 或更高
- ESP-IDF v5.4+
- Agora RTSA SDK（`components/agora_rtc/`，预编译静态库 `libagora-rtc-sdk.a`）
- Agora AOSL（`components/aosl/`，操作系统抽象层）
- 声网 ConvoAI 服务端

---

## 一、SDK 与 AOSL 集成

### 1.1 AOSL 组件（`components/aosl/`）

AOSL 是 Agora 的 OS 抽象层，为 RTSA SDK 提供跨平台线程、内存、网络、日志能力。

**ESP32-S3 平台适配层**（`components/aosl/platform/src/esp32-s3/`）需实现：

| 文件 | 功能 |
|------|------|
| `aosl_hal_thread.c` | 线程创建、互斥锁、条件变量、信号量 |
| `aosl_hal_socket.c` | socket 操作 |
| `aosl_hal_memory.c` | 内存分配/释放 |
| `aosl_hal_time.c` | 时间获取、延时 |
| `aosl_hal_errno.c` | 错误码映射 |
| `aosl_hal_atomic.c` | 原子操作 |
| `aosl_hal_log.c` | 日志输出 |
| `aosl_hal_iomp.c` | IO 多路复用 |

**线程优先级映射**（`aosl_hal_thread.c`）：
```c
OS_PRIORITY_IDLE        = 0   // FreeRTOS pri 0
OS_PRIORITY_LOW         = 1
OS_PRIORITY_BELOW_NORMAL= 2
OS_PRIORITY_NORMAL      = 3
OS_PRIORITY_ABOVE_NORMAL= 4
OS_PRIORITY_HIGH        = 5   // ← SDK 内部线程（AgoraRtc、AgoraRtcCb）
OS_PRIORITY_REAL_TIME   = 6
```

所有 AOSL 线程创建使用 `sched_priority = OS_PRIORITY_HIGH`（FreeRTOS 优先级 5），调用 `xTaskCreate`。
 
> **注意**：`aosl_hal_thread_create` 中参数 `param->priority` 被忽略，创建时总是 `OS_PRIORITY_HIGH`。如需调整 SDK 内部线程的优先级或绑定核心，修改此文件中的 `xTaskCreate` 调用。

### 1.2 Agora RTC SDK 组件（`components/agora_rtc/`）

预编译 SDK 静态库 + C API 头文件。

**CMakeLists.txt**：
```cmake
idf_component_register(INCLUDE_DIRS ${COMPONENT_PATH}/include
                       REQUIRES aosl lwip)

# 循环依赖：SDK ↔ aosl，使用 link group
target_link_libraries(${COMPONENT_LIB} INTERFACE
    ${AGORA_SDK_LIB} ${AOSL_LIB} ${AGORA_SDK_LIB})
```

---

## 二、协议实现

### 2.1 核心类 `AgoraRtcProtocol`（`main/protocols/agora_rtc_protocol.h/.cc`）

继承自 `Protocol` 基类。

**成员**：
```cpp
// AEC 参考环形缓冲区（无锁 SPSC，PSRAM）
std::unique_ptr<LockFreeRingBuffer> ref_ring_buffer_;
static constexpr size_t kRefBufferMaxSamples = 16000;  // 1秒 @ 16kHz

// Kconfig 功能开关（通过 menuconfig 配置）
#define AGORA_AI_QOS                CONFIG_AGORA_AI_QOS
#define AGORA_CLOUD_AEC             CONFIG_AGORA_CLOUD_AEC
#define AGORA_JITTER_BUFFER         CONFIG_AGORA_JITTER_BUFFER
#define AGORA_JITTER_BUFFER_DURATION_MS CONFIG_AGORA_JITTER_BUFFER_DURATION_MS
```

### 2.2 音频通道建立（`OpenAudioChannel`）

```
1. POST /conversations/start → 获取 RTC 参数（app_id, channel, token, uid, agent_uid）
2. agora_rtc_init(app_id, &handler, &option)
3. agora_rtc_login_rtm(uid, token, &rtm_handler) → 等待 AGORA_RTM_LOGIN_EVENT
4. agora_rtc_create_connection(&conn_id)
5. 配置选项:
   auto_subscribe_audio = true
   enable_audio_decode   = true
   enable_audio_jitter_buffer = CONFIG_AGORA_JITTER_BUFFER
   enable_audio_ai_qos        = CONFIG_AGORA_AI_QOS
   enable_audio_downlink_aec  = CONFIG_AGORA_CLOUD_AEC
   audio_codec = G722 @ 16kHz mono, 60ms pcm_duration
6. agora_rtc_join_channel_with_user_account() → 等待 AGORA_JOINED_EVENT
```

### 2.3 音频上行（`SendAudio`）

**Cloud AEC 模式**（`AGORA_CLOUD_AEC=true`，默认）：
```
mic PCM 960 samples + ref PCM 960 samples → 交错 [mic1,ref1,mic2,ref2,...]
→ agora_rtc_send_audio_data(bytes=3840)
```

**非 Cloud AEC 模式**（`AGORA_CLOUD_AEC=false`）：
```
mic PCM 960 samples → agora_rtc_send_audio_data(bytes=1920)
```

`ref_ring_buffer_` 的数据由 `OnAudioData` 写入、`SendAudio` 读取。

### 2.4 音频下行（`OnAudioData`）

下行音频路径为**直接回调**（无中间 ringbuf 异步任务）：

```
SDK 回调线程 → OnAudioData
  ├─ Write(ref_ring_buffer_)       ← AEC 参考
  └─ on_incoming_audio_(packet)     ← 直接推给 AudioService
```

注意：`OnAudioData` 运行在 SDK 内部 `AgoraRtcCb` 线程（FreeRTOS pri 5），直接推送 PCM 到 `AudioService::PushPacketToDecodeQueue`。

### 2.5 Device API 客户端（`device_api_client.h/.cc`）

HTTP 配对联话管理：

| 阶段 | 端点 | Token |
|------|------|-------|
| 配对中 | POST `/devices/pair-codes` | 无 |
| 轮询认领 | GET `/devices/{id}/binding-status` | `Pair <pair_token>` / `Device <device_token>` |
| 启动对话 | POST `/devices/{id}/conversations/start` | `Device <device_token>` |
| 停止对话 | POST `/devices/{id}/conversations/stop` | `Device <device_token>` |

---

## 三、Kconfig 配置体系

### 3.1 菜单结构

```
Xiaozhi Assistant
  └─ Board Type
  └─ Connection Protocol              ← choice
       ├─ Auto (OTA determined)
       └─○ Agora RTC
       └─ Agora RTC Settings          ← menu (仅 Agora RTC 选中时可见)
            ├─ AI QoS (non-clocked streaming)
            ├─ Cloud AEC (mic-ref interleaving)
            ├─ RTC SDK jitter buffer
            │   └─ Jitter buffer duration  → 20 / 40 / 60 ms
            └─ Device API Server URL
```

### 3.2 配置项

| Kconfig | 默认 | 说明 |
|---------|------|------|
| `CONNECTION_TYPE_AGORA_RTC` | y | 使用 Agora RTC 协议 |
| `AGORA_AI_QOS` | y | AI QoS，服务端非时钟推流 + CHORUS 场景 |
| `AGORA_CLOUD_AEC` | y | 云端 AEC，上行交织 mic+ref |
| `AGORA_JITTER_BUFFER` | y | SDK 内部自适应 jitter buffer |
| `AGORA_JITTER_BUFFER_DURATION_MS` | 60 | Jitter buffer 输出帧长，设为 60 时 OnAudioData 每次回调 60ms PCM |

### 3.3 配置使用流程

```bash
idf.py menuconfig
# → Xiaozhi Assistant → Connection Protocol → Agora RTC
# → 选择需要的子选项
idf.py reconfigure  # 重新生成 sdkconfig
idf.py build
```

---

## 四、无锁环形缓冲区 LockFreeRingBuffer

### 4.1 设计

SPSC（Single Producer Single Consumer），PSRAM 分配。使用 `std::atomic<size_t>` acquire/release 语义。

```cpp
class LockFreeRingBuffer {
    int16_t* buffer_;              // PSRAM: heap_caps_malloc(MALLOC_CAP_SPIRAM)
    size_t capacity_;              // int16_t sample 数量
    std::atomic<size_t> write_pos_; // 生产者专属
    std::atomic<size_t> read_pos_;  // 消费者专属
};
```

**SPSC 契约**：生产者只写 `write_pos_` 不碰 `read_pos_`，消费者反之。缓冲区满时直接覆盖最旧数据（不碰 `read_pos_`），消费者读到脏数据但不会破坏数据结构。

### 4.2 Write

```cpp
void Write(const int16_t* data, size_t count) {
    size_t w = write_pos_.load(memory_order_relaxed);
    for (size_t i = 0; i < count; i++)
        buffer_[w] = data[i], w = (w + 1) % capacity_;
    write_pos_.store(w, memory_order_release);
}
```

### 4.3 Read

```cpp
size_t Read(int16_t* data, size_t count) {
    size_t r = read_pos_.load(memory_order_relaxed);
    size_t w = write_pos_.load(memory_order_acquire);
    size_t available = circular_distance(w, r, capacity_);

    // 数据不足：全零填充，不推进 read_pos_
    if (available < count) {
        memset(data, 0, count * sizeof(int16_t));
        return 0;
    }

    for (size_t i = 0; i < count; i++)
        data[i] = buffer_[r], r = (r + 1) % capacity_;
    read_pos_.store(r, memory_order_release);
    return count;
}
```

**关键语义**：数据不足时全零返回，不消费数据。避免了部分有效 PCM + 零填充造成的帧边界不连续 → click/pop 音。

### 4.4 Reset

```cpp
void Reset() {
    write_pos_.store(0, memory_order_relaxed);
    read_pos_.store(0, memory_order_relaxed);
}
```

**竞态风险**：`Reset` 在 `CloseAudioChannel` 中调用时可能与 `OnAudioData`（SDK 回调线程）的 Write 并发。极端情况下会产生不一致的 `write_pos_/read_pos_`，但下一帧 Write 即恢复正常。Reset 仅在 mute 事件时触发（罕见），风险可接受。

---

## 五、Application 框架集成

### 5.1 初始化切换

```cpp
void Application::InitializeProtocol() {
#if CONFIG_CONNECTION_TYPE_AGORA_RTC
    protocol_ = std::make_unique<AgoraRtcProtocol>();
#else
    // OTA → MQTT 或 WebSocket
#endif
}
```

### 5.2 激活流程

RTC 模式下 `ActivationTask` 走 `AgoraPairingTask`：

1. SNTP 时间同步
2. 加载资源文件
3. 检查是否已有 `device_token`（NVS）：
   - 有 → 验证绑定状态 → `InitializeProtocol()` → 完成
   - 无 → 请求配对码 → 轮询配对状态 → 完成

### 5.3 全双工音频接收

```cpp
protocol_->OnIncomingAudio([this](auto packet) {
#if CONFIG_CONNECTION_TYPE_AGORA_RTC
    if (state == kDeviceStateSpeaking || state == kDeviceStateListening)
        audio_service_.PushPacketToDecodeQueue(std::move(packet));
#else
    if (state == kDeviceStateSpeaking)
        audio_service_.PushPacketToDecodeQueue(std::move(packet));
#endif
});
```

RTC 模式下 listening 和 speaking 都接收下行音频（全双工）。

---

## 六、关键架构决策

### 6.1 下行音频直接回调（无 ringbuf 中间层）

下行音频不经过额外 ringbuf 处理，`OnAudioData` 直接 `on_incoming_audio_`。简化路径、减少延迟，前提是 FreeRTOS tick 精度足够。

### 6.2 FreeRTOS tick 精度（CONFIG_FREERTOS_HZ）

通过 `AGORA_JITTER_BUFFER_DURATION_MS=60` 让 jitter buffer 输出 60ms 帧（匹配下行消费粒度），`OnAudioData` 每次收到 60ms PCM，与下行读取节奏自然同步。

HZ=100（10ms tick）下搭配此设置即可稳定工作，不需要改为 HZ=1000。

### 6.3 Cloud AEC vs Device AEC

| 模式 | 上行数据 | `enable_audio_downlink_aec` | 适用场景 |
|------|---------|---------------------------|----------|
| Cloud AEC | mic+ref 交织（1920→3840 字节） | true | 云端处理回声消除 |
| 无 Cloud AEC | 纯 mic（1920 字节） | false | 设备端自带 AEC 或无需 AEC |

### 6.4 AI QoS

`AGORA_AI_QOS=true` 时：
- 服务端使用 **Non-Clocked Streaming**（非时钟驱动推流）
- 信令协商设置为 **CHORUS** 音频场景
- Jitter buffer 变为纯 FIFO 模式（跳过延迟统计、SPEEDUP、启动等待）

关闭 AI QoS 时 jitter buffer 按自适应模式运行（EWMA 统计 + SPEEDUP）。

### 6.5 Jitter Buffer 行为对比

| 特性 | 标准模式 | AI QoS 模式 |
|------|---------|------------|
| 启动等待 | ✅ ~60ms（帧累积阈值） | ❌ 无（有数据即出） |
| EWMA 抖动统计 | ✅ | ❌ |
| SPEEDUP 跳帧 | ✅ | ❌ |
| 空缓冲 | 返回静音 | 返回静音 |
| 线程安全 | SDK 内部单线程 | 同左 |

---

## 七、线程调度与优先级

### 7.1 默认优先级（git baseline）

| 任务 | 优先级 | 说明 |
|------|--------|------|
| `main`（Application::Run） | 10 | 主事件循环 |
| `audio_input` | 8 | MIC 采集 |
| `audio_output` | 4 | 扬声器输出 |
| `opus_codec` | 2 | 音频编解码 |
| `AgoraRtc`（SDK） | 5 | SDK 内部线程（AOSL） |
| `AgoraRtcCb`（SDK） | 5 | SDK 回调队列 |
| lwIP TCP/IP | 18 | 网络协议栈 |

### 7.2 线程优先级常见问题

- `AgoraRtc`（pri 5）低于音频任务（pri 2/4/8），但高于 `opus_codec`（pri 2）
- SDK 20ms 定时器运行在 `AgoraRtc` 线程（pri 5），精度受更高优先级的 `audio_input`(8)、`main`(10) 影响
- `CONFIG_FREERTOS_HZ=1000` 使各任务调度更精细，但增加上下文切换开销

---

## 八、调试指南

### 编译
```bash
idf.py set-target esp32s3
idf.py menuconfig
# Xiaozhi Assistant → Connection Protocol → Agora RTC
# Agora RTC Settings → 选择子选项
idf.py reconfigure
idf.py build
```

### 日志
```bash
idf.py monitor
```

### 关键日志 TAG

| TAG | 内容 |
|-----|------|
| `AgoraRTC` | RTC 协议事件（初始化、加入频道、收发音频） |
| `DeviceAPI` | Device API 调用（配对码、轮询、对话管理） |
| `RingBuf` | 环形缓冲区状态（数据不足告警） |
| `Application` | 应用层事件（激活、配对、状态切换） |

### 常见问题

| 问题 | 解决方案 |
|------|---------|
| `newlib nano format` 编译错误 | `CONFIG_NEWLIB_NANO_FORMAT=n` |
| RTM 登录超时 | 检查网络、app_id |
| 加入频道失败 | 检查 token、app_id/channel/uid 匹配 |
| 下行无声音 | 检查 `enable_audio_decode=true` |
| 泡泡音/click 音 | ① 开启 `CONFIG_FREERTOS_HZ=1000` ② 关闭 jitter buffer |
| LVGL 崩溃 | 检查 `CONFIG_FREERTOS_HZ` 是否导致显示任务调度异常 |

---


## 九、编译完整固件包

### 9.1 总规则

> **⚠️ 重要**：每次编译前必须先清理旧的 sdkconfig 再重新配置，否则之前 Board 的配置（如 SPI RAM QUAD/OCT mode、flash 大小、Board Type 等）会残留导致编译错误。

> **⚠️ 必须设置 Board Type**：清理 sdkconfig 后，如果不通过 menuconfig 选择 Board Type，默认值（ZHENGCHEN_1_54TFT_ML307）会被使用，编译出的固件将不匹配目标硬件，烧录后设备无法启动！

**编译步骤（必须按顺序执行）：**

```bash
# 第1步：清理旧配置
rm -f sdkconfig sdkconfig.old

# 第2步：选择 Board Type、语言和协议（必须先选 Board，否则默认值错误）
idf.py menuconfig
# → Xiaozhi Assistant → Board Type → 选择对应的开发板
# → Xiaozhi Assistant → Default Language → Chinese / English
# → Connection Protocol → Agora RTC
# → Agora RTC Settings → Device API Server URL → 填入对应区域地址

# 第3步：生成配置并编译
idf.py reconfigure
idf.py build
```

- **完整固件输出目录**：`releases/`（工程根目录下），每次编译生成的 `.bin` 文件放入此目录便于归档。
- **必须编译两个版本**：每个 Board 都必须同时编译 **中国大陆版本（sh2）** 和 **海外版本（sg3）**，缺少任一版本视为不完整。
- **中国大陆版本**：`Default Language` → `LANGUAGE_ZH_CN`，`Device API Server URL` → `https://mybot.sh2.agoralab.co/api`
- **海外版本**：`Default Language` → `LANGUAGE_EN_US`，`Device API Server URL` → `https://mybot.sg3.agoralab.co/api`

完整固件包命名规则：

```
{board_name}_{region}.bin

board_name = BOARD_TYPE 去掉前缀 "BOARD_TYPE_"，全小写
region     = Device API URL 中的区域标识（sh2 或 sg3）

示例：
  BOARD_TYPE_ZHENGCHEN_1_54TFT_ML307 + sh2
  → zhengchen_1_54tft_ml307_sh2.bin

  BOARD_TYPE_ZHENGCHEN_1_54TFT_ML307 + sg3
  → zhengchen_1_54tft_ml307_sg3.bin
```

### 9.2 生成完整固件包

编译完成后，在 `build/` 目录下执行：

```bash
# 从 build 输出获取正确的 flash args
idf.py build

# 打包为单个完整固件（以 zhengchen sh2 为例）
cd build
python -m esptool --chip esp32s3 merge_bin \
  --output zhengchen_1_54tft_ml307_sh2.bin \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 bootloader/bootloader.bin \
  0x8000 partition_table/partition-table.bin \
  0xd000 ota_data_initial.bin \
  0x20000 xiaozhi.bin \
  0x800000 generated_assets.bin
```

> `--flash_size`、偏移地址以 `idf.py build` 输出的烧录命令为准，不同分区表可能不同。

### 9.3 已支持的 Board 编译配置

#### 9.3.1 BOARD_TYPE_ZHENGCHEN_1_54TFT_ML307

除总规则外，其余均保持默认。

#### 9.3.2 BOARD_TYPE_M5STACK_CORE_S3

> **⚠️ QUAD/OCT mode 残留问题**：M5Stack Core S3 使用 **Quad Mode PSRAM**。如果在此 Board 之后编译其他 Board，必须清理 sdkconfig（`rm -f sdkconfig`），否则残留的 `CONFIG_SPIRAM_MODE_QUAD=y` 会导致其他使用 OCT mode 或不同 SPI RAM 配置的 Board 编译失败。

除总规则外，还需配置：
```
Component config → ESP PSRAM → Support for external, SPI-connected RAM
  → SPI RAM config → Mode (QUAD/OCT) of SPI RAM chip in use
    → Quad Mode PSRAM
```

#### 9.3.3 BOARD_TYPE_SEEED_STUDIO_SENSECAP_WATCHER

除总规则外，还需配置：
```
Partition Table → Custom partition table file → partitions/v2/32m.csv
Serial flasher config → Flash size → 32 MB
```

#### 9.3.4 BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_75C

除总规则外，其余保持默认。

#### 9.3.5 BOARD_TYPE_ZHENGCHEN_1_54TFT_WIFI

除总规则外，其余保持默认。

#### 9.3.6 BOARD_TYPE_ESP_VOCAT

除总规则外，其余保持默认。

### 9.4 完整编译及打包示例（中国大陆版 zhengchen）

```bash
# 第一步：清理旧配置（重点！防止不同 Board 配置残留）
rm -f sdkconfig sdkconfig.old

# 第二步：配置 Board、语言、协议
idf.py menuconfig
# Xiaozhi Assistant → Board Type → 征辰科技1.54(ML307)
# Xiaozhi Assistant → Default Language → Chinese
# Connection Protocol → Agora RTC → Device API Server URL
#   → https://mybot.sh2.agoralab.co/api

# 第三步：生成新配置并编译
idf.py reconfigure
idf.py build
cd build
python -m esptool --chip esp32s3 merge_bin \
  --output zhengchen_1_54tft_ml307_sh2.bin \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 bootloader/bootloader.bin \
  0x8000 partition_table/partition-table.bin \
  0xd000 ota_data_initial.bin \
  0x20000 xiaozhi.bin \
  0x800000 generated_assets.bin
```
