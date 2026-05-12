# Server 端集成行为分析

> 本文从 **客户端实际代码行为** 出发，给 server 实现者整理：协议版本号、abort、goodbye、心跳、按键交互、监听模式等关键点的真实语义与坑点。
>
> 配套文档（不重复其内容，仅交叉引用）：
> - [abort.md](abort.md) — abort 触发路径与新 turn 行为
> - [heartbeat.md](heartbeat.md) — 心跳机制与 120s 空闲超时
> - [listening-modes-and-power-vad.md](listening-modes-and-power-vad.md) — 监听模式 + Power VAD 设计
> - [websocket.md](websocket.md) / [mqtt-udp.md](mqtt-udp.md) — 传输协议规范
> - [ota-protocol_zh.md](ota-protocol_zh.md) — OTA 配置下发

---

## 1. 协议版本号 `version`

xiaozhi 没有真正的版本协商机制。`version` 在 WebSocket 里**决定二进制音频帧的封装格式**；MQTT 里是占位常量。

### 1.1 客户端如何使用 `version`

**WebSocket**（`websocket_protocol.cc`）：

| 位置 | 行为 |
|---|---|
| `line 84-90` | 从 NVS `websocket.version` 读取，未配置时默认 `1`（`websocket_protocol.h:27`）|
| `line 108` | WS 握手 HTTP 头 `Protocol-Version: <v>` |
| `line 207` | client → server hello JSON 里 `"version": v` |
| `line 33-57` | **上行音频** binary frame 按 v 编码 |
| `line 115-146` | **下行音频** binary frame 按 v 解析 |

三种 binary frame 格式（`protocol.h:17-31`）：

| version | 帧格式 | timestamp | 备注 |
|---|---|---|---|
| 1 | 裸 Opus | 无 | 默认 |
| 2 | `BinaryProtocol2`（16 字节头）| **有**（毫秒，用于 server-side AEC）| 大端字节序 |
| 3 | `BinaryProtocol3`（4 字节头）| 无 | 头部更小，省字节 |

**MQTT**（`mqtt_protocol.cc:301`）：

```cpp
cJSON_AddNumberToObject(root, "version", 3);
```
硬编码常量 `3`，不可配置。UDP 音频包是自定义 AES-CTR + nonce 加密格式，跟 `version` 数字**无任何绑定关系**。

### 1.2 关键事实：客户端不校验 server hello 的 `version`

`ParseServerHello`（`websocket_protocol.cc:228-254`、`mqtt_protocol.cc:322+`）只读 `transport`、`session_id`、`audio_params`，**完全不读 server 回的 `version` 字段**。也就是：
- 没有最低版本要求
- 没有协议协商/降级
- 没有 capability negotiation

### 1.3 对 server 的要求

**WebSocket server 必须**：
1. 接受 HTTP 头 `Protocol-Version: 1|2|3`
2. **按 client hello 中的 `version` 字段切换二进制 codec**：v=2 时收发都包 `BinaryProtocol2`（大端，含 timestamp）；v=3 时包 `BinaryProtocol3`；v=1 时收发裸 Opus
3. server hello 必须包含 `transport: "websocket"`（客户端会校验，不匹配直接 return — `websocket_protocol.cc:229-233`）、`session_id`、`audio_params.{sample_rate, frame_duration}`
4. server hello **不需要**返回 `version`（返回了也被忽略）

**MQTT server 必须**：
1. 接受 hello 中的 `version: 3`（硬编码，没有别的值）
2. server hello 必须包含 `transport: "udp"`（校验：`mqtt_protocol.cc:323-327`）、`session_id`、`audio_params`、`udp.{server, port, key, nonce}`
3. UDP 音频按 AES-CTR + nonce 协议解（详见 [mqtt-udp_zh.md](mqtt-udp_zh.md)）

### 1.4 坑点

1. **WS 默认 v=1，无 timestamp**：要做 server-side AEC 必须让设备配 `websocket.version = 2`。`CONFIG_USE_SERVER_AEC` 编译开关只影响 hello 里 `features.aec` 和本地 timestamp 记录（`audio_service.cc:315-321`），**不会**自动把 version 提到 2。
2. **客户端不做版本兼容 fallback**：client 配 v=2 而 server 当 v=1 处理 → server 把 16 字节 `BinaryProtocol2` 头当 Opus 帧丢给 decoder → 噪音/解码失败，无错误反馈。
3. **MQTT 的 `version: 3` 是占位常量**：不要拿它做 routing。MQTT 兼容性边界在 `features` 字段（`mcp`、`aec` boolean），不在 version。
4. **JSON 控制消息无版本约束**：新增 type 老 client 会进 `else` 打 `WARN "Unknown message type"`（`application.cc:610`），不会崩；老 server 不识别新 type 通常静默丢弃。新功能建议先扩 `features` 字段做能力声明。

---

## 2. abort 消息

> 主分析见 [abort.md](abort.md)。本节仅给 server 实现要点。

### 2.1 客户端发送两种 JSON

```jsonc
// 物理交互（按键 / Toggle / StartListening）
{"session_id":"...","type":"abort"}

// 唤醒词命中
{"session_id":"...","type":"abort","reason":"wake_word_detected"}
```

实现位置：`protocol.cc:42-49` 的 `Protocol::SendAbortSpeaking()`。

### 2.2 三条触发路径对客户端状态的影响

| 路径 | 入口 | 客户端是否清本地 TTS 队列 | 是否自动开新 turn |
|---|---|---|---|
| 唤醒词打断 | `HandleWakeWordDetectedEvent` (`application.cc:784`) | **是**（经 `EnableVoiceProcessing(true)` → `ResetDecoder`）| **是**，自发 `listen start` |
| 按键 `StartListening`（路径 B）| `HandleStartListeningEvent` Speaking 分支（`application.cc:760-762`）| **是** | **是** |
| 按键 `ToggleChatState`（路径 A）| Speaking 分支（`application.cc:711-712`）| **否**（仅发 abort，状态不变）| **否**，等用户再操作 |

### 2.3 server 必须做的

```text
on_abort(msg):
    1. cancel_inflight_llm()
    2. cancel_inflight_tts()
    3. stop_sending_audio_packets()
    4. send({"type":"tts","state":"stop"})   # 路径 A 必须；其它路径无害（被 state 守卫忽略）
    5. log("abort_reason", msg.reason or "none")
    # 不需要主动开新 turn — 等客户端 listen start
```

**幂等关键点**：客户端 `tts stop` 处理用 `if (GetDeviceState() == kDeviceStateSpeaking)` 守卫（`application.cc:531-545`），多发或迟到都安全。Server 端**统一发 stop 兜底**是 KISS 解法。

### 2.4 `reason` 字段建议

server 不强制区分，但建议记录用于：
- 「唤醒词打断率」高 → 用户喜欢中途插话（UX 信号）
- 「按键打断率」高 → TTS 太长/太啰嗦（产品信号）
- 上下文衔接：唤醒词打断是否复用 session/对话历史

---

## 3. goodbye 消息

### 3.1 MQTT —— 真实有效

`mqtt_protocol.cc:115-126` 收到 goodbye 的逻辑：
```cpp
if (session_id == nullptr || session_id_ == session_id->valuestring) {
    Application::GetInstance().Schedule([this, alive]() {
        if (*alive) {
            CloseAudioChannel(false);   // 不回 goodbye 给 server，避免 ping-pong
        }
    });
}
```

`CloseAudioChannel(false)`（`mqtt_protocol.cc:192-213`）会：
1. `udp_.reset()` 销毁 UDP socket
2. 触发 `on_audio_channel_closed_()` → app 切到 `kDeviceStateIdle`
3. **不**清 `session_id_`（只有 `OpenAudioChannel` 起始处 line 224 才清）

### 3.2 多次 goodbye 副作用

| 项 | 行为 | 影响 |
|---|---|---|
| `udp_.reset()` 对已 null 的 unique_ptr | no-op | 无 |
| 重复触发 `on_audio_channel_closed_()` | 多次 `Schedule(SetDeviceState(idle))` | 状态机 `TransitionTo` 对相同状态 no-op（`device_state_machine.cc:111-114`）— 无害 |
| 重复 log `Closing audio channel...` | 日志噪音 | 可忽略 |

总体：**无功能性损坏**。

### 3.3 ⚠️ 隐藏坑：`session_id` 为 null + 客户端已重开 channel

```text
t0  client 开 channel #1，session_id_ = "A"
t1  server 决定结束 #1，发 goodbye
t2  消息在网络上飞着
t3  用户按按键 → client 开 channel #2，session_id_ = "B"
t4  goodbye 到达 client：
    - 若 goodbye 带 session_id="A" → "A" ≠ "B" → 被忽略 ✅
    - 若 goodbye 不带 session_id   → 命中 nullptr 分支 → 误关 channel #2 ❌
```

**给 server 的硬性要求**：
- **每条 goodbye 都必须带 `session_id`**，让 client 能做 mismatch-reject
- 不要重复发；若必须重发，所有重发条目都带**结束时**的那个 session_id（不要换成新的）

### 3.4 WebSocket —— JSON goodbye 完全无效

- `websocket_protocol.cc:78-80`：`CloseAudioChannel` 的 `send_goodbye` 参数被 `(void)send_goodbye;` 显式忽略 — **WS 协议层根本不发 goodbye**
- application 的 `OnIncomingJson` 没有 `"goodbye"` 分支（`application.cc:529, 555, 563, 570, 575, 588, 598`），收到只命中 `else` 打印 `WARN "Unknown message type: goodbye"`

**WS server 关连接应该发 WebSocket close frame（opcode 0x8）**，client library `web_socket.cc:383-387` 处理该 frame 触发 `on_disconnected_` → application 切 idle。**不要**发 JSON goodbye。

### 3.5 goodbye 的语义本质

在 MQTT 路径下 goodbye 实际**做了**和**没做**的事情：

```
server goodbye → mqtt_protocol.cc:115-126
                 → CloseAudioChannel(false)   // 不回包
                   ├─ udp_.reset()              // 关 UDP socket
                   ├─ on_audio_channel_closed_()→ app → kDeviceStateIdle
                   └─ 保留 session_id_、MQTT 连接、AES ctx 等待下次 hello
```

所以 goodbye 的真实语义是 **"强制结束当前 audio turn，把设备拉回 idle"**，不是 kick connection、不是探活。MQTT 连接、session_id_ 都不会被它清除——下次 hello 才会重置。

### 3.6 Server 该在何时下发 goodbye（MQTT）

| # | 场景 | 为什么必须 server 主动发 |
|---|---|---|
| 1 | **server 单边判定 turn 结束**（LLM 出完、TTS 播完，且不紧跟 listen） | client 不会自己关 audio channel；不发设备就一直挂着 UDP，等 120s 超时才回 idle，体验上像"卡住" |
| 2 | **server 检测到 UDP 上行静默**（如 30~60s 没收到设备 opus 包，但 MQTT 还活） | 大概率 NAT 老化 / 路由变了。goodbye 让设备回 idle，下次按键自动重新协商新 UDP 4 元组 |
| 3 | **server 侧 pipeline 故障**（LLM 挂、TTS 服务挂、加密 ctx 异常） | 别让设备傻等；释放设备状态，下次它会重新走 hello |
| 4 | **server 端 session 资源回收**（超过最大会话时长 / idle 阈值 / 内存压力驱逐） | UDP 加密上下文 / SSRC / 端口绑定都有成本；server 单边释放但不通知，设备继续用旧 key/nonce 加密的包到了 server 也解不开 |
| 5 | **服务热重启 / 滚动升级 / session 迁移** | 重启前批量发 goodbye，比让一万台设备走 120s 超时友好得多 |
| 6 | **协议版本/能力变更**（极少见） | goodbye 后下次 hello 自然带新版本号 |

### 3.7 不该发 goodbye 的反例

| # | 反例 | 为什么 |
|---|---|---|
| A | 设备 MQTT 已断（broker LWT/超时上报）后再补 goodbye | 管道已死，发不出去；浪费 broker 资源，徒增日志噪音 |
| B | 收到 client goodbye 后回一个 goodbye | client 代码 `CloseAudioChannel(false)` 已经避免了 ping-pong（`mqtt_protocol.cc:122` 注释明确），server 也不该自己点火 |
| C | 正常 turn 间隔（一轮对话刚结束，要立刻进下一轮 listen） | 让 audio channel 留着，发 `tts stop` 就够了。每个 turn 都 close/open 一次会重新建 UDP + 跑 AES setkey，得不偿失 |
| D | 当 heartbeat 用 | goodbye 是终结信号不是探活信号。保活靠 MQTT keepalive |
| E | 重发 goodbye 时换新的 session_id | 参考 §3.3 —— 必须带**结束时**那个 session_id，否则可能误关设备已经重开的 channel #2 |

### 3.8 发送前的三问

```
1. MQTT 通道还通吗？               否 → 别发，走超时清理
2. 设备此刻可能在用这个 channel 吗？ 否 → 没必要发（无害但浪费）
3. 我有 session_id 吗？             否 → 别发（§3.3 的坑会误伤）
```

三个都是 yes → 发，并且带上 session_id。

---

## 4. 心跳与连接保活

> 主分析见 [heartbeat.md](heartbeat.md)。本节是给 server 实现者的精简要点。

### 4.1 关键事实

| 协议 | 心跳机制 | 谁发 | 默认间隔 |
|---|---|---|---|
| MQTT | 协议层 PINGREQ/PINGRESP（esp-mqtt 库自动）| 客户端 | `keepalive`，OTA 可配，默认 **240s** |
| WebSocket | WS 控制帧 ping (0x9) / pong (0xA)| **server 主动 ping**，客户端只回 pong | 无 |

客户端**没有应用层心跳 JSON**（不存在 `{"type":"ping"}`）。

### 4.2 ⚠️ WS 隐藏坑：`Protocol::IsTimeout()` 120s 空闲超时

```cpp
// protocol.cc:81-90
bool Protocol::IsTimeout() const {
    const int kTimeoutSeconds = 120;
    auto duration = now - last_incoming_time_;
    return duration.count() > kTimeoutSeconds;
}
```

`last_incoming_time_` 仅在 **应用层数据**到达时刷新（`websocket_protocol.cc:165`、`mqtt_protocol.cc:131, 286`）。

**WS Ping/Pong 帧在 library 内部处理，不会触发上层 `on_data_` 回调**（`web_socket.cc:376-378` 只对 text/binary frame 调用 `on_data_`），所以 **server 只发 WS-level ping 是不能阻止客户端 120s 超时的**。

**WS server 维持空闲连接的正确方式**：周期下发 **JSON 文本帧**（任何 type 都行），不要靠 WS ping。

---

## 5. 按键交互路径分析

### 5.1 触发路径取决于 board 代码绑了哪种回调

参考 `compact_wifi_board.cc:26-39`：
```cpp
boot_button_.OnClick(...)        → ToggleChatState();   // 路径 A
touch_button_.OnPressDown(...)   → StartListening();    // 路径 B（按下）
touch_button_.OnPressUp(...)     → StopListening();     // 路径 B（松开）
```

| 回调 | 触发时机 | 调用 | abort 后是否开新 turn |
|---|---|---|---|
| `OnClick` | 「按下+松开」完整一次结束才回调 | `ToggleChatState` 路径 A | **否** |
| `OnPressDown` | 按下瞬间 | `StartListening` 路径 B | **是**（按下瞬间开新 turn）|
| `OnPressUp` | 松开瞬间 | `StopListening` | —— |
| `OnLongPress` | 持续按住超阈值 | board 自定义 | —— |

**关键**：是「绑了哪种回调」，**不是**「短按 vs 长按」的差别。

### 5.2 「按住说话」UX 的正确接法

如果产品 UX 严格要求"按住说话"，board 代码应当**只绑** `OnPressDown / OnPressUp`，**不绑** `OnClick`：

```cpp
boot_button_.OnPressDown([] { Application::GetInstance().StartListening(); });
boot_button_.OnPressUp([]   { Application::GetInstance().StopListening();  });
// 不绑 OnClick
```

这样 server 端 abort 处理可简化为「**一定会跟一条 listen start**」，少一个分支。

### 5.3 唤醒词打断的特殊行为

`application.cc:808-823`：
```cpp
} else if (state == kDeviceStateSpeaking || state == kDeviceStateListening) {
    AbortSpeaking(kAbortReasonWakeWordDetected);
    while (audio_service_.PopPacketFromSendQueue());   // 清空发送队列
    if (state == kDeviceStateListening) {
        protocol_->SendStartListening(GetDefaultListeningMode());
        audio_service_.ResetDecoder();
        ...
    } else {  // Speaking
        play_popup_on_listening_ = true;
        SetListeningMode(GetDefaultListeningMode());   // → Listening → SendStartListening
    }
}
```

**细节**：唤醒词那段音频帧默认**不重传上行**（`PopPacketFromSendQueue` 已经清空），server 听到的是唤醒词**之后**的语音。如果用户喊「小智，几点了」，server 只收到「几点了」。

---

## 6. 监听模式与 AEC/VAD 概要

> 完整设计见 [listening-modes-and-power-vad.md](listening-modes-and-power-vad.md)。

### 6.1 三种 ListeningMode

| Mode | JSON `mode` | 触发条件 | 上行/下行 |
|---|---|---|---|
| `kListeningModeAutoStop` | `auto` | `aec_mode_ == kAecOff` 的默认 | 半双工，依赖服务端 VAD 决定收尾 |
| `kListeningModeManualStop` | `manual` | 按键交互（押住-说话）| 半双工，本地 manual stop 触发收尾 |
| `kListeningModeRealtime` | `realtime` | `aec_mode_ != kAecOff` 的默认 | **全双工**，TTS 播放期间持续上行 |

`application.cc:955-957`：
```cpp
ListeningMode GetDefaultListeningMode() const {
    return aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime;
}
```

### 6.2 realtime 全双工的关键证据

`application.cc:917-921`：
```cpp
case kDeviceStateSpeaking:
    if (listening_mode_ != kListeningModeRealtime) {
        audio_service_.EnableVoiceProcessing(false);   // 仅非 realtime 才关麦
        audio_service_.EnableWakeWordDetection(audio_service_.IsAfeWakeWord());
    }
```
**realtime 模式 Speaking 状态不关麦**，所以上行（mic → AEC → Opus）和下行（TTS → 喇叭）同时跑。

### 6.3 AEC / VAD 位置

| 模式 | AEC 位置 | VAD 位置 |
|---|---|---|
| `kAecOnDeviceSide` | 设备端 AFE（`afe_audio_processor.cc:191-193`）| **设备 VAD 被显式 disable**，靠服务端 VAD |
| `kAecOnServerSide` | server | server（设备发裸 PCM/Opus + timestamp）|
| `kAecOff` | 无 | 设备 AFE VAD（半双工，TTS 播放时关麦）|

**realtime 必有 AEC**，否则 TTS 会被麦克风环回成上行送回 server。

### 6.4 realtime 模式的打断完全由 server VAD 决定

- 客户端 realtime 模式下**没有本地 VAD**
- 何时收尾、何时打断完全靠 server VAD
- server 通过下发 `{"type":"tts","state":"stop"}` 让设备退出 Speaking 状态（`application.cc:531-545`）

⚠️ 注意：server 下发 `tts stop` 后，客户端**不会**主动清本地 decode/playback 队列。**本地缓冲的 TTS 帧会继续播完**，听感上有「老师我懂了……噢」的小尾巴。

---

## 7. Server 主动播报（MQTT+UDP 主动下行）

> 场景：通知音、定时播报、被动消息（"快递到了"）、广播——server 想**主动**向 device 推一段音频播放，而不是回应一次正常对话。
>
> 服务端 (`xiaozhi-esp32-server-golang`) 的完整时序、参数、排查 checklist 见 [mqtt_udp_active_downlink.md](../mqtt_udp_active_downlink.md)。本节聚焦**协议约定** + **client 端固件需要做的改动**。

### 7.1 必须先解决的三个客观障碍

xiaozhi 协议本身是 client-initiated 对话模型，没有 server-push 入口。要做主动下行，必须先解决：

| 障碍 | 根因 | 解法 |
|---|---|---|
| UDP 通道严格 client-initiated | server 不知道 client 的 NAT 出口 (`mqtt_protocol.cc:215-294`)；AES-CTR nonce 来自 session 协商 | 让设备**先发一帧 UDP 上行**给 server 打洞 |
| 仅 `kDeviceStateSpeaking` 才播放下行 | `application.cc:498-502` 在其它状态直接丢弃 UDP 包 | 收到推送通知后 client 自己切到 Speaking |
| 120s 空闲 + NAT 映射超时 | `protocol.cc:81-90` 硬编码 120s；路由器 NAT 典型 30–120s 回收 | server 维护"热链路窗口"，过窗即触发完整握手 |

### 7.2 推荐方案：`speak_request` / `speak_ready` 握手

服务端通过两条 MQTT 控制消息和一帧 UDP 打洞包，把上面三个障碍一次性解决：

```
[Server]                                       [Device]
   │
   │ 1. MQTT speak_request ───────────────────▶
   │    {type, session_id, text, auto_listen}
   │
   │                                         2. 检查 UDP 通道
   │                                            ├─ 可用 → 直接打洞
   │                                            └─ 失效 → 发 hello 重协商
   │                                               (server 识别为 duplicate_hello)
   │
   │ 3. UDP 上行打洞包 ◀───────────────────────
   │    (server SetRemoteAddr 绑 NAT 出口)
   │
   │ 4. MQTT speak_ready ◀─────────────────────
   │    {state:"ready", udp_config.ready:true}
   │
   │ 5. MQTT tts sentence_start ──────────────▶  client → Speaking
   │ 6. UDP 加密音频帧（多个）──────────────────▶  播放
   │ 7. MQTT tts sentence_end ────────────────▶
   │
   │ 8a. auto_listen=true  → MQTT listen start  (进入下一 turn)
   │ 8b. auto_listen=false → MQTT goodbye       (按 §3.6 场景 #1 收尾)
```

server 端用 5s pending timer 等 `speak_ready`，超时即放弃；并维护 60s 热链路窗口 (`chat.speak_request_reuse_window_ms`)，窗内重复 inject 跳过整套握手直接 TTS 下行。

### 7.3 消息约定

**server → device：`speak_request`**

```jsonc
{
  "type": "speak_request",
  "session_id": "xxx-xxx-xxx",
  "text": "提醒您 10 点开会",   // 文本预览，便于设备 UI / 日志
  "auto_listen": false          // true: 播完进 listen；false: 播完发 goodbye 收尾
}
```

**device → server：`speak_ready`**

```jsonc
{
  "type": "speak_ready",
  "session_id": "xxx-xxx-xxx",
  "state": "ready",
  "udp_config": {
    "ready": true,
    "reuse_existing": true      // true=复用已有 UDP 通道；false=刚走 hello 重建
  }
}
```

session_id 全程一致（speak_request → speak_ready → tts → goodbye）；若 client 走了 hello 重协商，使用 server hello 返回的 session_id。

### 7.4 Client 端固件改动

> 现状：`application.cc:526-610` 的 `OnIncomingJson` **未实现** `speak_request` 分支；`mqtt_protocol.cc` 也未提供 `SendSpeakReady`。下面是最小改动方案。

**Step 1**：在 `Protocol` 基类加 `SendSpeakReady`（参考 `Protocol::SendStartListening` 的写法，`protocol.cc:57-69`）：

```cpp
void Protocol::SendSpeakReady(bool reuse_existing) {
    std::string message = "{\"session_id\":\"" + session_id_ +
        "\",\"type\":\"speak_ready\",\"state\":\"ready\"," +
        "\"udp_config\":{\"ready\":true,\"reuse_existing\":" +
        (reuse_existing ? "true" : "false") + "}}";
    SendText(message);
}
```

**Step 2**：在 `application.cc::OnIncomingJson` 增加 `speak_request` 分支：

```cpp
} else if (strcmp(type->valuestring, "speak_request") == 0) {
    Schedule([this]() {
        // a) 状态冲突保护：仅 idle 接受主动播报，其它状态忽略
        if (GetDeviceState() != kDeviceStateIdle) {
            ESP_LOGW(TAG, "speak_request ignored, state=%d", GetDeviceState());
            return;
        }

        // b) 保证 UDP 通道可用；不可用则走 hello 重协商
        //    (server 端会识别为 duplicate_hello，保留 pendingSpeakRequest)
        bool reuse_existing = protocol_->IsAudioChannelOpened();
        if (!reuse_existing) {
            if (!protocol_->OpenAudioChannel()) {
                ESP_LOGE(TAG, "speak_request: OpenAudioChannel failed");
                return;
            }
        }

        // c) 上行一帧静音 Opus 给 server 打洞，绑 NAT RemoteAddr
        protocol_->SendAudio(MakeSilentOpusPacket());

        // d) 切到 Speaking，准备接收 TTS UDP 帧
        aborted_ = false;
        SetDeviceState(kDeviceStateSpeaking);

        // e) 回 speak_ready，解 server 端 pending 阻塞
        protocol_->SendSpeakReady(reuse_existing);
    });
}
```

**关键点**：
- **状态冲突**：当前若在 Listening / Speaking，应该 reject 或先 abort 当前 turn 再播报。最安全的做法是 reject，由 server 端 5s 超时自然失败，避免设备状态机错乱。
- **打洞包**：一帧静音 Opus 即可（payload 全 0 / 极短帧）。目的是让 server `udp_server.go:151` 的 `SetRemoteAddr` 拿到 NAT 出口，**不参与 TTS 播放**。
- **`reuse_existing` 的取值**：跟 server 的 `markSpeakPathWarm` 是否要更新缓存有关，必须如实上报。
- **收尾**：完全复用现有 `tts stop` / `goodbye` 处理（`application.cc:531-545`、`mqtt_protocol.cc:115-126`），state 机不动。

### 7.5 失败与边界

| 情况 | 客户端行为 | 服务端兜底 |
|---|---|---|
| MQTT 在线但设备处于 Speaking/Listening | reject（不回 speak_ready） | 5s pending timer 超时，HTTP 调用方收到错误 |
| `OpenAudioChannel` 失败（hello 10s 超时） | 静默放弃，**不重试**（避免风暴）| 同上，超时失败 |
| UDP 打洞包发出但被防火墙拦 | 仍会回 speak_ready；server 端 `WaitRemoteAddr(2s)` 后下行包整包丢 | 表现为"speak_ready OK 但没声音"——查防火墙 |
| session_id 不匹配（设备并发收到多条） | 应按收到的最新 speak_request 处理，旧的 speak_ready 不发 | `HandleSpeakReadyMessage` 校验 session_id，不匹配只打 warning 不解阻塞 |
| pendingSpeakRequest 期间设备意外上行音频 | （无需特殊处理）| `chat.go:402` 自动丢弃，避免被当用户输入 |

### 7.6 一句话

> 主动播报靠 `speak_request → 设备 UDP 打洞 → speak_ready` 三步握手补齐协议层缺的 server-push 入口。client 端需要新增 `speak_request` 分支 + `SendSpeakReady`，整体改动 < 30 行；server 端则用 60s 热链路窗口避免每次重协商。

---

## 8. Server 实现最小 checklist

### WebSocket
- [ ] 握手时读 `Protocol-Version` 头（v1/v2/v3）
- [ ] 等待 client hello → 解析 `version`、`features`、`audio_params`、`transport`
- [ ] 回 server hello：必带 `transport=websocket`、`session_id`、`audio_params.{sample_rate, frame_duration}`
- [ ] 按 client `version` 编/解音频 binary frame（裸 Opus / BP2 / BP3）
- [ ] v2 模式下处理 `timestamp` 字段用于 AEC 时间对齐
- [ ] 空闲期周期下发 **JSON 文本帧**维持连接（不要发 WS ping，不计入 120s 超时刷新）
- [ ] **关连接发 WS close frame**（opcode 0x8），不要发 JSON goodbye
- [ ] 收到 abort 时统一回 `tts stop` 兜底
- [ ] realtime 模式靠 server VAD 决定何时下发 `tts stop`

### MQTT
- [ ] 订阅 client hello topic，识别 `version: 3`
- [ ] 回 server hello：必带 `transport=udp`、`session_id`、`audio_params`、`udp.{server, port, key, nonce}`
- [ ] UDP 通道用 client 协商的 AES-CTR + nonce 加密上下行
- [ ] 主动断开发 `{"type":"goodbye", "session_id": "..."}` —— **必须带 session_id**，防止误关已重开的新 channel
- [ ] keepalive 默认 240s，可通过 OTA 下发 `keepalive` 字段覆盖
- [ ] 收到 abort 时统一回 `tts stop` 兜底

### 通用
- [ ] JSON 文本帧的语义在所有 version 下相同
- [ ] 新增 type 时建议先扩 hello 的 `features` 字段做能力声明（前向兼容）
- [ ] 不要假设客户端会做版本/能力 fallback —— 它不会

---

## 9. 一句话总结

> xiaozhi 客户端**不做协议协商、不做版本 fallback、不做应用层心跳**。所有兼容性、保活、流控的责任都在 server 端，client 是「按预设规则跑」的纯执行端。Server 实现的核心原则是 **幂等 + KISS**：状态相关的下行（`tts stop`、`goodbye`）多发都安全，但漏发或带错 `session_id` 会直接导致客户端卡死或误关新 channel。
