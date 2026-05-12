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

## 7. Server-initiated UDP push（协议未约定）

> 场景：通知音、定时播报、被动消息（"快递到了"）、广播——server 想**主动**向 device 推一段音频播放，而不是回应一次正常对话。

### 7.1 结论

**xiaozhi 协议层没有官方约定**支持 server 主动推送音频。当前协议的 mental model 是 client-initiated 对话（用户说话 → server 回复 TTS），没有给「server push」留显式入口。但代码留了三个可利用的"洞"，可以**间接**实现，前提条件严格。

### 7.2 协议层为什么不支持

#### 障碍 1：UDP 通道严格 client-initiated

`mqtt_protocol.cc:215-294`（`OpenAudioChannel`）：必须 **client 先 publish** `{"type":"hello","transport":"udp"}` → server 回 hello 带 `udp.{server, port, key, nonce}` → client 才 `network->CreateUdp(2)` 创建 socket。

Server 在此之前**不可能**对 UDP 推音频：
- Client 在 NAT 后，server 不知道其出口 IP/Port
- AES-CTR nonce 是 session 协商的，没建立 session 无法加密
- Client 端根本没有监听 UDP 端口

#### 障碍 2：Client 端 UDP 音频包按 device state 过滤

`application.cc:498-502`：
```cpp
protocol_->OnIncomingAudio([this](std::unique_ptr<AudioStreamPacket> packet) {
    ...
    if (GetDeviceState() == kDeviceStateSpeaking) {
        audio_service_.PushPacketToDecodeQueue(std::move(packet));
    }
});
```
**仅 `kDeviceStateSpeaking` 才播放**，idle / listening 状态收到 UDP 音频**直接丢弃**。

#### 障碍 3：120s 空闲超时 + NAT 映射超时

`mqtt_protocol.cc:387-389`：
```cpp
bool MqttProtocol::IsAudioChannelOpened() const {
    return udp_ != nullptr && !error_occurred_ && !IsTimeout();
}
```

- `IsTimeout()` 阈值硬编码 120s（`protocol.cc:81-90`），无应用层数据就判超时
- 路由器 NAT 映射典型 30–120s 不上行流量就回收 —— server 拿着旧端口推音频包**到不了 client**
- MQTT keepalive 不能维持 UDP 映射（不同 socket）
- 协议层**没有 UDP 保活机制**

### 7.3 代码留下的三个洞

| 洞 | 位置 | 描述 |
|---|---|---|
| `tts state=start` 无前置检查 | `application.cc:531-535` | 不要求当前在 Listening、不验证 session、不看 listening_mode——MQTT 收到就 `SetDeviceState(Speaking)` |
| `WakeWordInvoke` 是 public API | `application.h:108`、`application.cc:1024-1054` | 全代码无人调用，预留给外部触发对话（MCP tool、button、外设事件）|
| `tts stop` 已经在 idle 也能切换 | `application.cc:536-545` | 用 `if (GetDeviceState() == kDeviceStateSpeaking)` 守卫，幂等安全 |

### 7.4 可行的实现路径

#### 路径 A：channel 已 open 时（最简单）

**前提**：device 刚结束一次对话，UDP channel 还 open，state=idle，距上次有数据 < 120s。

Server 端：
1. MQTT 发 `{"type":"tts","state":"start","session_id":"..."}` → client 切 Speaking
2. UDP 推 Opus 音频包 → client 解密 + 解码 + 播放
3. MQTT 发 `{"type":"tts","state":"stop","session_id":"..."}` → client 切 idle / listening

**风险**：
- 120s 内 server 不发任何东西 → channel 视为超时
- NAT 映射回收 → server 的 UDP 包到不了 client
- 用 `tts` type 跟正常对话回复在语义上混淆，日志不好区分

#### 路径 B：channel 没 open 时（device 长时间 idle）

**协议没解决的核心场景**。三个 workaround：

| 方案 | 改动 | 评估 |
|---|---|---|
| **B1：扩展 JSON 协议** | client `OnIncomingJson` 加新分支，如 `{"type":"play"}` → 主动 `OpenAudioChannel` + `SetDeviceState(Speaking)` | **推荐**。最干净，固件改动 < 20 行 |
| **B2：MCP tool 路径** | server 通过 MCP 调 device-side 工具，工具调 `WakeWordInvoke` 或自定义 | 利用现有 MCP 双向通道，但要 device 端注册对应 tool |
| **B3：扩展 system command** | `application.cc:575-587` 的 `{"type":"system","command":"..."}` 目前只有 `reboot`，加新 command | 跟 system 语义混淆，不推荐 |

#### 路径 C：周期"心跳唤醒"

让 client idle 时也每 N 分钟主动 OpenAudioChannel + 立刻 CloseAudioChannel，给 server 留推送窗口。开销大，违背"按需建立"原则，**不推荐**。

### 7.5 推荐方案 B1 的最小实现

**协议设计**：新增 `play` type（区别于 `tts`，明确表达"server 主动推送"语义）。

```jsonc
// Server → Client（MQTT 控制通道）
{
  "type": "play",
  "session_id": "...",
  "source": "notification"   // 可选：notification / broadcast / scheduled / ...
}
```

收尾仍走 `{"type":"tts","state":"stop"}`，复用现有逻辑。

**client 端固件改动**（`application.cc::OnIncomingJson` 加分支）：
```cpp
} else if (strcmp(type->valuestring, "play") == 0) {
    Schedule([this]() {
        if (!protocol_->IsAudioChannelOpened()) {
            protocol_->OpenAudioChannel();   // 主动建 UDP 通道
        }
        aborted_ = false;
        SetDeviceState(kDeviceStateSpeaking);
    });
}
```

**好处**：
- 不污染 `tts` 语义（埋点、日志能清晰区分主动推送 vs 对话回复）
- 完全复用 UDP 通道、加密、Opus 解码、播放队列
- 收尾走标准 `tts stop`，state 机不用动
- 客户端代码改动量 < 20 行

**注意事项**：
- channel 是否 open 由 client 检查，server 可以"无脑发"，client 自己决定要不要建通道
- 仍然需要解决 **NAT 映射超时**：长时间 idle 后 client 主动重建 channel（hello → 新 UDP 4 元组），server 必须用**新** session 的 nonce 加密
- 若 device 处于其他状态（Listening 中），新加分支应处理冲突——典型做法是先 abort 当前对话再 play

### 7.6 一句话

> xiaozhi 协议没有 server-push 约定，但 `tts state=start` 无前置检查这个"洞"让 channel-open 状态下的推送变得可行。要支持「device 长时间 idle 时主动推送」，必须在 client 端加新 JSON type 让 client 主动 `OpenAudioChannel`，协议层没办法绕过去。

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
