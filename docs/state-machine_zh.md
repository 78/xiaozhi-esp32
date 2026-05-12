# xiaozhi 设备状态机与会话消息流转

本文档梳理 xiaozhi 固件的 `DeviceStateMachine`，以及在不同 listening mode（`manual` / `auto` / `realtime`）下，server 与 device 之间的消息如何驱动状态迁移。

> 源码位置
> - 状态机定义：`main/device_state.h`，`main/device_state_machine.{h,cc}`
> - 状态迁移规则：`DeviceStateMachine::IsValidTransition()`
> - 协议消息构造：`main/protocols/protocol.cc`
> - server 消息分发：`Application::OnIncomingJson()`（`main/application.cc:526`）
> - 状态变化副作用：`Application::HandleStateChangedEvent()`（`main/application.cc:860`）

---

## 1. 状态枚举

`main/device_state.h`：

| 状态 | 含义 |
|---|---|
| `kDeviceStateUnknown` | 未初始化 |
| `kDeviceStateStarting` | 启动中 |
| `kDeviceStateWifiConfiguring` | Wi-Fi 配网中 |
| `kDeviceStateIdle` | 空闲，等待唤醒 / 按键 |
| `kDeviceStateConnecting` | 正在与 server 建立音频通道 |
| `kDeviceStateListening` | 录音 / 上传用户语音 |
| `kDeviceStateSpeaking` | 接收并播放 TTS |
| `kDeviceStateUpgrading` | OTA 升级中 |
| `kDeviceStateActivating` | 设备激活流程 |
| `kDeviceStateAudioTesting` | 配网期间的回采测试 |
| `kDeviceStateFatalError` | 致命错误（不可恢复） |

会话相关的核心三态是 **Idle ↔ Listening ↔ Speaking**，其他状态多为运维流程。

---

## 2. Listening Mode

`main/protocols/protocol.h`：

```cpp
enum ListeningMode {
    kListeningModeAutoStop,    // server VAD 自动断句
    kListeningModeManualStop,  // 用户按键控制起止
    kListeningModeRealtime     // 全双工，需 AEC
};
```

默认选择（`Application::GetDefaultListeningMode()`）：

| AEC 配置 | 默认模式 |
|---|---|
| `kAecOff` | `auto` |
| `kAecOn` | `realtime` |

`manual` 仅在用户主动调用 `StartListening()`（按键长按场景）时使用。

---

## 3. 协议消息一览

### 3.1 device → server

| 消息 | 触发点 | 用途 |
|---|---|---|
| `hello` | `OpenAudioChannel` | 握手，协商协议版本和采样率 |
| `listen {state: start, mode}` | 进入 Listening 状态 | 开始一次拾音 |
| `listen {state: stop}` | `StopListening` / Manual 模式按键松开 | 显式结束拾音 |
| `listen {state: detect, text}` | 唤醒词命中 | 上报唤醒词 |
| `abort {reason}` | 用户打断 / 二次唤醒 | 终止当前 TTS |
| `mcp {payload}` | 工具调用 | MCP over WS/MQTT |
| 二进制 audio | Listening 期间 | 上行语音流 |

### 3.2 server → device

| 消息 | device 响应 |
|---|---|
| `hello` 应答 | 保存 `session_id`，标记通道已打开 |
| `tts {state: start}` | `Speaking` 状态 |
| `tts {state: stop}` | 根据 mode 回到 `Idle` 或 `Listening` |
| `tts {state: sentence_start, text}` | 更新 UI 字幕，不改状态机 |
| `stt {text}` | 显示用户识别结果，不改状态机 |
| `llm {emotion}` | 更新表情，不改状态机 |
| `mcp {payload}` | 分发给 `McpServer`，不改状态机 |
| `system {command: reboot}` | 重启设备 |
| `alert {status, message, emotion}` | 播放提示音并显示 |
| 二进制 audio | 仅 `Speaking` 时入解码队列，否则丢弃 |

---

## 4. 合法状态迁移图

源自 `DeviceStateMachine::IsValidTransition()`（`device_state_machine.cc:34`）。会话三态部分：

```
                ┌─────────────────────────────┐
                │                             │
                ▼                             │
   ┌──────────────────────┐                   │
   │     kDeviceStateIdle │◀──────────────┐   │
   └──────────┬───────────┘               │   │
              │                           │   │
              │ 唤醒/按键                  │   │
              ▼                           │   │
   ┌──────────────────────┐               │   │
   │ kDeviceStateConnect- │               │   │
   │        ing           │──失败─────────┘   │
   └──────────┬───────────┘                   │
              │ OpenAudioChannel OK           │
              ▼                               │
   ┌──────────────────────┐                   │
   │ kDeviceStateListen-  │                   │
   │        ing           │◀──────┐           │
   └──────────┬───────────┘       │           │
              │ tts:start         │ tts:stop  │ tts:stop
              ▼                   │ (auto/    │ (manual)
   ┌──────────────────────┐       │  realtime)│
   │ kDeviceStateSpeaking │───────┘           │
   └──────────┬───────────┘                   │
              │  abort / 服务端断开            │
              └───────────────────────────────┘
```

非法跳转（例如 Idle 直接进 Speaking 是允许的，但 Connecting 直接进 Speaking 不允许）会被 `TransitionTo()` 拒绝并打 WARN 日志。

---

## 5. 三种模式下的完整时序

下列时序假定设备已激活、Wi-Fi 已就绪、状态停留在 `Idle`。

### 5.1 Manual 模式（按键长按）

按键场景：长按"对讲键"开始说话，松开按键结束说话，TTS 播放完成后回 Idle。

```
device                              server
  │                                    │
  │ [按键按下] StartListening()         │
  │                                    │
  │ state: Idle → Connecting           │
  │ ────── OpenAudioChannel ─────────▶ │
  │ ◀───── hello {session_id} ──────── │
  │                                    │
  │ state: Connecting → Listening      │
  │ ────── listen{start, "manual"} ──▶ │
  │ ────── audio frames ─────────────▶ │
  │ ────── audio frames ─────────────▶ │
  │ ◀───── stt{text} ───────────────── │  (UI 更新)
  │                                    │
  │ [按键松开] StopListening()           │
  │ ────── listen{stop} ─────────────▶ │
  │ state: Listening → Idle            │
  │                                    │
  │ ◀───── tts{start} ──────────────── │
  │ state: Idle → Speaking             │  ✱ 直接跳过 Listening
  │ ◀───── audio frames ───────────── │
  │ ◀───── tts{sentence_start,text} ── │
  │ ◀───── tts{stop} ────────────────── │
  │ state: Speaking → Idle             │  ✱ manual 模式回 Idle
  │                                    │
```

要点：
- 拾音起止由 device 完全控制，server 不做 VAD
- `tts:stop` 在 manual 模式回 `Idle`（`application.cc:539`）
- 下一轮对话需要再次按键

### 5.2 Auto 模式（无 AEC，服务端 VAD）

唤醒词触发，server 通过 VAD 自动断句，TTS 播完后自动回到 Listening 等待下一句。

```
device                              server
  │                                    │
  │ [唤醒词命中] HandleWakeWordDetected │
  │ state: Idle → Connecting           │
  │ ────── OpenAudioChannel ─────────▶ │
  │ ◀───── hello {session_id} ──────── │
  │                                    │
  │ state: Connecting → Listening      │
  │ ────── listen{start, "auto"} ───▶ │
  │ ────── listen{detect,wake_word} ─▶ │  (可选)
  │ ────── audio frames ─────────────▶ │
  │ ────── audio frames ─────────────▶ │  (server 侧 VAD 检测到静音)
  │                                    │
  │ ◀───── stt{text} ───────────────── │
  │ ◀───── llm{emotion} ────────────── │
  │ ◀───── tts{start} ──────────────── │
  │ state: Listening → Speaking        │
  │ ◀───── audio frames ───────────── │
  │ ◀───── tts{sentence_start,text} ── │
  │ ◀───── tts{stop} ────────────────── │
  │                                    │
  │ ✱ WaitForPlaybackQueueEmpty()       │  (避免播放截断)
  │ state: Speaking → Listening        │  ✱ auto 模式自动回 Listening
  │ ────── listen{start, "auto"} ───▶ │
  │ ────── audio frames ─────────────▶ │  (继续下一轮)
  │                                    │
```

要点：
- 同一次 session 内可以反复 `Listening ↔ Speaking` 切换
- `tts:stop` 在 auto 模式回 `Listening`（`application.cc:542`）
- 切回 Listening 时会先等播放队列放空，避免最后一帧 TTS 被截
- 若用户中途打断（再次唤醒或按键），会发 `abort` 给 server

### 5.3 Realtime 模式（带 AEC，全双工）

需要 AEC。device 边播 TTS 边继续拾音，可以语音打断。

```
device                              server
  │                                    │
  │ [唤醒词命中或按键]                   │
  │ state: Idle → Connecting           │
  │ ────── OpenAudioChannel ─────────▶ │
  │ ◀───── hello {session_id} ──────── │
  │                                    │
  │ state: Connecting → Listening      │
  │ ────── listen{start,"realtime"} ─▶ │
  │ ────── audio frames (持续) ──────▶ │  ✱ 不会自动停
  │                                    │
  │ ◀───── tts{start} ──────────────── │
  │ state: Listening → Speaking        │
  │ ────── audio frames (仍在上行) ──▶ │  ✱ AEC 消除回声后继续拾音
  │ ◀───── audio frames (TTS) ─────── │
  │ ◀───── tts{stop} ────────────────── │
  │ state: Speaking → Listening        │
  │                                    │
  │ ⓘ 若用户说话打断 TTS（server 端 VAD）：│
  │  - device 上行不变，仍持续发 audio   │
  │ ◀───── (server 检测到人声，停 TTS) ── │
  │ ◀───── tts{stop} ────────────────── │  ✱ 复用 stop 兼任 abort 语义
  │ state: Speaking → Listening        │
```

要点：
- `kListeningModeRealtime` 期间 `EnableVoiceProcessing` 不会在 Speaking 时关闭（`application.cc:917`）
- 状态机仍然在 Listening 和 Speaking 之间切，但语音上行不间断
- **device 本地没有 VAD**：AEC 和 VAD 在 AFE 内部互斥（`afe_audio_processor.cc:189`），开 AEC 就关 VAD
- **barge-in 决策权完全在 server**：device 不会主动发 `abort`，唯一会触发 device → server `abort` 的是唤醒词（KWS）命中

---

## 6. tts:stop 行为汇总

| listening_mode | `tts:stop` 之后的目标状态 | 备注 |
|---|---|---|
| `manual` | `Idle` | 一句一交互，需用户再次按键 |
| `auto` | `Listening` | 切换前 `WaitForPlaybackQueueEmpty` |
| `realtime` | `Listening` | 同上，但语音处理本来就一直开着 |

判断逻辑：`application.cc:536-545`

```cpp
} else if (strcmp(state->valuestring, "stop") == 0) {
    Schedule([this]() {
        if (GetDeviceState() == kDeviceStateSpeaking) {
            if (listening_mode_ == kListeningModeManualStop) {
                SetDeviceState(kDeviceStateIdle);
            } else {
                SetDeviceState(kDeviceStateListening);
            }
        }
    });
}
```

**注意**：`tts:stop` 只是状态切换信号，并不直接清空播放队列。真正"不再有声音"取决于：

1. server 不再下发音频包
2. 状态切走之后，`OnIncomingAudio` 里的 `state == kDeviceStateSpeaking` 守卫不成立，残余音频包被丢弃（`application.cc:504`）
3. AutoStop 切回 Listening 时主动等播放队列放空

---

## 7. 打断（abort）流程

device → server 的 `abort` 仅在 **manual / auto 模式**、由 **device 端事件**（唤醒词、按键）触发：

```
device                              server
  │ [唤醒词命中 / 按键] state: Speaking │
  │ ────── abort{reason} ───────────▶ │
  │ state: Speaking → Listening (走 SetListeningMode 路径)
  │ ◀───── (server 停止 TTS) ──────── │
  │ ◀───── tts{stop} ──────────────── │
```

`AbortSpeaking` 在 `application.cc:942`：

```cpp
void Application::AbortSpeaking(AbortReason reason) {
    aborted_ = true;
    if (protocol_) {
        protocol_->SendAbortSpeaking(reason);
    }
}
```

注意 `aborted_` 字段**目前是 dead code**——只在 `AbortSpeaking()` 和 `tts:start` 解析时被赋值，**没有任何地方读它**。所以"后续 tts:stop 会被忽略"这种说法不成立。`tts:stop` 处理逻辑里只看 `GetDeviceState() == kDeviceStateSpeaking`，状态对就处理。

> 在 realtime 模式下，device 端**不会**主动发 `abort`——因为本地没有 VAD，没有"用户在说话"的判定来源。barge-in 完全靠 server 端用上行音频做 VAD，然后通过 `tts:stop` 通知 device 收尾。

---

## 8. 通道关闭与 session 失效

- server 主动断开 / 推 `goodbye`：触发 `OnAudioChannelClosed` → `SetDeviceState(kDeviceStateIdle)`（`application.cc:517`）
- device 主动关闭：`CloseAudioChannel`（如用户在 Listening 时再次按键停止）
- 关闭后 `session_id_` 清空（MQTT 显式清，WebSocket 依赖重连覆盖）
- 下一次唤醒/按键必须重新走 `Connecting → hello` 握手

---

## 9. 状态机语义分析：三种模式的本质差异

固件用同一套 `DeviceState` enum 承载了三种模式，但**同名状态在不同模式下物理含义并不一致**，协议的对称性也是缺的。这一节把这些隐含差异列清楚，作为 server 端实现的判断依据。

### 9.1 同名状态、不同语义

| 状态 | manual | auto | realtime |
|---|---|---|---|
| **Listening** | mic 开，等用户松开按键 | mic 开，等 server VAD 端点 | mic 从入口就开，**贯穿 Speaking 不关** |
| **Speaking** | TTS 播放，mic 关 | TTS 播放，mic 关（仅 AFE 唤醒词） | TTS 播放，**mic 仍开，持续上行** |
| Listening ↔ Speaking | **互斥**（时分复用） | **互斥**（时分复用） | **叠加**（Speaking 是在 Listening 之上叠了一层播放） |

也就是说，realtime 下的 `Speaking` 不能再理解为"只在说话"——它实际是"既在听又在说"。`HandleStateChangedEvent` 里靠 `if (listening_mode_ != kListeningModeRealtime)` 守卫绕过几个 transition 副作用（关 mic、切唤醒词），就是因为这个语义不一致——靠"代码补丁"在维护一个抽象层不对的设计。

### 9.2 协议方向的不对称

| 模式 | 打断触发源 | 决策点 | 打断信令方向 | 信令载体 |
|---|---|---|---|---|
| manual | 用户（按键） | **device** | device → server | `listen:stop` / `abort` |
| auto | 唤醒词 / 按键 | **device**（KWS 命中） | device → server | `abort{reason:wake_word_detected}` |
| realtime | 用户语音（barge-in） | **server**（上行 VAD） | **server → device** | **复用 `tts:stop`**（无专门信令） |

**协议里只有 device→server 的 `abort`，没有 server→device 的 `abort`**。realtime 模式下 server 想强制 device 闭嘴，只能让 `tts:stop` 兼任两个角色：

- manual / auto 下：`tts:stop` = "本句讲完了，自然收尾"
- realtime 下：`tts:stop` 既可能是"自然收尾"，也可能是"用户打断你立刻闭嘴"

device 收到 `tts:stop` 时**无法分辨这两种语义**。这是为什么 auto 模式必须 `WaitForPlaybackQueueEmpty()`（让尾音放完），而 realtime 模式同一条消息又希望立刻静音——可固件用同一条路径处理，导致 realtime barge-in 存在残留播放。

### 9.3 推动力转移

| 模式 | 推动一轮对话的角色 | server 定位 |
|---|---|---|
| manual | device 主导 | 被动响应（按键起、按键停） |
| auto | device 起 + server 终 | 半自治（device 发起，server VAD 决定何时停） |
| realtime | 主要靠 server | 全自治（端点、barge-in 都在 server） |

驱动力越往 realtime 方向，**device 越退化成 audio I/O 端**，状态机主动权越往 server 移。但代码里 `SetDeviceState` 都是 device 自己调，realtime 下变成"server 通过 tts 消息**间接**驱动 device 状态机"——这种间接驱动天然有延迟和歧义。

### 9.4 已知边界（不修固件的前提下）

| 现象 | 触发场景 | 影响 | 解释 |
|---|---|---|---|
| realtime barge-in 残留播放 | server 在 TTS 中途发 `tts:stop` 打断 | 设备会继续播 decode/playback 队列里剩余音频（最坏 ~2.4s，实测通常几百 ms） | `tts:stop` 切到 Listening 时**不调** `ResetDecoder()`，队列自然消耗 |
| auto 模式 `tts:stop` 来得太早会截尾 | server 在最后一帧 audio 发完前就发 stop | 末尾被截 | 已用 `WaitForPlaybackQueueEmpty()` 缓解，但只覆盖 playback 队列已入队部分 |
| `aborted_` flag 是 dead code | — | 无副作用，但状态机里没有"打断态"的概念 | 字段只写不读 |

这些不修固件可以解决吗？——下一节"server 集成规范"给的策略就是按这些边界把责任拆给 server。

---

## 10. Server 端集成规范

**总原则**：固件保持现状，由 server 通过控制消息的发送时序与语义遵守来保证三种 mode 都"正常工作"。下面分通用、按 mode、和已知 trade-off 三块。

### 10.1 通用要求

#### 10.1.1 握手（hello）

收 device hello（示例）：
```json
{
  "type": "hello",
  "version": 3,
  "features": { "aec": true, "mcp": true },
  "transport": "websocket",
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  }
}
```

server 必须回（在 device 的 hello 超时窗口内，参考 `WebsocketProtocol::OpenAudioChannel`）：
```json
{
  "type": "hello",
  "transport": "websocket",
  "session_id": "<server 分配>",
  "audio_params": {
    "sample_rate": 16000,
    "frame_duration": 60
  }
}
```

- `transport` 必须与 device 请求一致，否则 device 报 "Unsupported transport"
- `session_id` 是 server 分配，device 后续上行消息都会回带这个 id
- `audio_params.sample_rate` 是 server 输出 TTS 的采样率，device 会按需 resample；不一致会日志告警但不报错

#### 10.1.2 session_id 一致性

- server 所有下行控制消息（tts、stt、llm、mcp、alert、system、goodbye）**应该**带 `session_id`
- MQTT 协议 device 端**强校验**：`session_id` 不匹配的消息会被丢弃（`mqtt_protocol.cc:118`）
- WebSocket device 端目前不强校验，但建议 server 仍带，方便后续诊断

#### 10.1.3 消息顺序

- WebSocket / MQTT 都在 TCP 上，**server 发送顺序 = device 接收顺序**
- 控制消息和音频帧在 WebSocket 走同一通道，在 MQTT 走 control+UDP 两条通道（UDP 无序，但有 timestamp）
- 关键不变量：**server 发完一回合最后一帧 audio 再发 `tts:stop`**

#### 10.1.4 心跳与超时

- device 端 `Protocol::IsTimeout` 阈值 120 秒（`protocol.cc:81`），**距离最近一条 server 入站消息**
- server 在空闲期至少每 60s 发一次任意消息（推荐心跳/heartbeat 帧），避免 device 误判超时
- 详见 `docs/heartbeat.md`

#### 10.1.5 二进制音频

- 格式 Opus，采样率取 hello 协商值
- WebSocket version 2/3 使用 `BinaryProtocol2/3` 帧头（带 timestamp 等字段）
- 仅在 device 处于 `Speaking` 状态时入解码队列，其它状态丢弃——所以**先发 `tts:start` 切状态、再发 audio**

#### 10.1.6 goodbye

- server 主动断 session：发 `{"type":"goodbye","session_id":"..."}` 后关闭通道
- device 收到 goodbye 后 `OnAudioChannelClosed` 触发，状态机回 Idle，`session_id_` 清空

---

### 10.2 Manual 模式契约

**device 行为**：
- 进入 Listening 后发 `listen{state:"start", mode:"manual"}`
- 持续上行 audio
- 按键松开发 `listen{state:"stop"}` 显式收尾
- 直接进入等待 TTS（state 临时回 Idle，等 `tts:start`）
- 收到 `tts:stop` 后回 Idle，**不会自动进入下一轮 Listening**

**server 必须做**：

| 规则 | 说明 |
|---|---|
| ✅ 把 `listen{state:"stop"}` 当 ASR 端点 | 不要再做 VAD 端点判定 |
| ❌ 不在 `listen{stop}` 之前发 `tts:start` | device 还在录音，TTS 会被丢 |
| ✅ 一轮回答完毕发 `tts:stop` | 一定要发，否则 device 卡在 Speaking |
| ✅ 处理 `abort{reason}` | 用户中途按键/唤醒会发 abort，要立即停 TTS 生成并清空发送队列 |
| ❌ 不要在 `tts:stop` 之后继续主动推送 TTS | manual 是一问一答，下一轮要等 device 再发 `listen{start}` |

---

### 10.3 AutoStop 模式契约

**device 行为**：
- 进入 Listening 发 `listen{state:"start", mode:"auto"}`
- 持续上行 audio，**不会发 `listen{stop}`**
- 进入 Speaking 时关闭 mic，仅留 AFE 唤醒词
- 收 `tts:stop` 后切回 Listening，**会再次发 `listen{state:"start", mode:"auto"}`** 开始下一轮

**server 必须做**：

| 规则 | 说明 |
|---|---|
| ✅ 在上行音频流上做 VAD | device 不会主动断句，必须 server VAD 决定一回合何时结束 |
| ❌ 不要等 `listen{stop}` | device 永远不发 |
| ✅ 每次新的 `listen{start, mode:"auto"}` 视为新一回合 | 重置 ASR 上下文（partial 等） |
| ✅ 全部 audio 发完后**才**发 `tts:stop` | device 在收 stop 之前不会做 `WaitForPlaybackQueueEmpty`（注：实际是收 stop 切到 Listening 时 wait），过早 stop 会截尾 |
| ✅ 处理 `abort{reason:wake_word_detected}` | 唤醒词二次触发时 device 会发 abort，server 要立刻停 TTS 并准备新一轮（device 也会接着发新的 `listen{start}`） |
| ✅ 兼容 `listen{state:"detect", text:wake_word}` | `CONFIG_SEND_WAKE_WORD_DATA` 开启时 device 会在 listen{start} 后立刻补发唤醒词文本和音频，server 可以用来矫正 ASR 上下文 |

**典型节奏**：

```
device: listen{start, auto}
device: audio frames... (server VAD 判定结束)
server: stt → llm → tts{start} → audio frames → tts{stop}
device: listen{start, auto}    ← 自动开始下一轮
device: audio frames...
...
```

---

### 10.4 Realtime 模式契约

**device 行为**：
- 进入 Listening 发 `listen{state:"start", mode:"realtime"}`
- **持续不间断上行 audio**，包括 TTS 播放期间
- 进入 Speaking 时**不**关闭 mic，AEC 在 AFE 内做（device-side AEC）或交给 server（server-side AEC）
- 收 `tts:stop` 后切回 Listening，**不会重新发 listen{start}**（mic 本来就一直开着）
- **device 本地不做 VAD，不会主动发 abort**

**server 必须做**：

| 规则 | 说明 |
|---|---|
| ✅ 在上行音频流上做 VAD | 既用于端点判定，也用于 barge-in 检测 |
| ✅ 接受不间断 Opus 上行 | 静默段也会传，server 必须能处理常时流 |
| ✅ 全部 audio 发完后才发 `tts:stop`（自然收尾时）| 同 auto |
| ✅ **barge-in 时主动停 TTS 并发 `tts:stop`** | server 检测到用户语音 → 立刻中止 LLM 生成 → 停止发 TTS audio → 发 `tts:stop` |
| ✅ device-side AEC：上行已是干净人声 | 不需要 server 端再做 AEC |
| ⚠️ server-side AEC：device 上行带回声，但每包带 timestamp | server 需要按 timestamp 对齐自己最近发出的 TTS 帧做远端 AEC |
| ❌ 不要发 server → device 的 abort | 协议里没有这条消息，device 也不解析 |

#### 10.4.1 barge-in 残留播放的缓解策略（不修固件）

如 §9.4 所述，realtime 下 server 发 `tts:stop` 时，device 的 decode/playback 队列还有未消耗的音频。要把残留延迟压到最低，**server 端**可以做：

1. **降低 server 端 audio 发送 buffer**
   - 边生成边推流，不预聚合大段
   - TCP socket 设 `TCP_NODELAY`，减少 Nagle 延迟
   - 目标：从"决定打断"到"最后一帧 audio 离开 server 网卡"不超过一个 frame_duration（60ms）

2. **缩短 VAD 触发延迟**
   - barge-in 用更敏感的 VAD 阈值或更短的 trigger window（如 60-100ms 持续语音即触发，不要等 500ms）
   - 触发后**立刻**停止下发 audio，再发 `tts:stop`

3. **stop 之前主动 flush 发送侧 audio 队列**
   - 如果 server 内部有 audio buffer，barge-in 时丢弃尚未发送的部分（不只是停止生成新的）

4. **可选——分句生成而不是整段生成**
   - 每收到一句 LLM partial 就生成对应 TTS 并推流
   - 这样"未发送的剩余部分"少，barge-in 时丢弃的代价低

实测 device 端残留 = (server-side 未发送 audio 总长) + (TCP 在途 audio) + (device decode 队列 ≤ 2400ms) + (device playback 队列 ≤ 120ms)。前两项 server 可控，后两项要等队列自然消耗。

#### 10.4.2 device-side / server-side AEC 二选一

|  | device AEC（`CONFIG_USE_DEVICE_AEC`）| server AEC（`CONFIG_USE_SERVER_AEC`）|
|---|---|---|
| hello 中 `features.aec` | 不带 | `true` |
| 上行音频内容 | AFE 已消回声的纯人声 | 麦克风原始混音（含 TTS 回声） |
| 上行帧 timestamp | 0 | 最近一次播放包的 timestamp |
| server 需做的额外处理 | 直接 ASR/VAD | 先按 timestamp 对齐做远端 AEC，再 ASR/VAD |
| 对硬件要求 | 需 codec 有回采参考通道 | 任何 codec 都行 |

server 实现时 hello 应答最好读 `features.aec` 来分支：标了 `aec:true` 走 server-side AEC 路径，否则走纯 ASR 路径。

---

### 10.5 跨模式通用控制消息

server 可以在**任意模式、任意状态**下发以下消息，device 只更新 UI 不影响状态机：

| 消息 | 用途 |
|---|---|
| `stt {text}` | 显示用户语音识别结果 |
| `llm {emotion}` | 切换表情（如 happy / thinking / neutral） |
| `tts {state:"sentence_start", text}` | 显示 TTS 字幕（不切状态） |
| `mcp {payload}` | MCP 工具调用（device 端有 MCP server） |
| `alert {status, message, emotion}` | 弹出提示并播放提示音 |
| `system {command:"reboot"}` | 远程重启设备 |
| `custom {payload}` | （需 `CONFIG_RECEIVE_CUSTOM_MESSAGE`）传任意 JSON 给 UI 展示 |

device 对未识别 `type` 字段会打 WARN 日志但不报错。

---

### 10.6 错误处理

- ASR 失败 / LLM 报错：建议发 `alert` 让 device 提示用户，或直接发 `tts:start/audio/stop` 播报错误语音
- 不要在 device 当前 `state != Speaking` 时发 audio——会被丢弃
- 不要发 `tts:stop` 但 device 不在 Speaking——会被静默忽略（`application.cc:538`）
- 不要在没有 `tts:start` 的情况下单独发 audio——device 仍是 Listening 状态，audio 不会进解码队列

---

### 10.7 最小固件改动建议（仅在必要时）

按 §10.4.1 的策略，server 已经能把 realtime barge-in 残留压到一个 frame_duration 量级。如果业务要求"barge-in 立即静默（< 50ms）"，**只此一个场景**值得改固件，最小改动是：

```cpp
// application.cc OnIncomingJson 的 tts:stop 分支
} else if (strcmp(state->valuestring, "stop") == 0) {
    if (listening_mode_ == kListeningModeRealtime) {
        aborted_ = true;                            // 同步置位，下面 OnIncomingAudio 立刻看见
    }
    Schedule([this]() {
        if (GetDeviceState() == kDeviceStateSpeaking) {
            if (listening_mode_ == kListeningModeRealtime) {
                audio_service_.ResetDecoder();      // 清队列
            }
            // ... 原有状态切换逻辑
        }
    });
}

// application.cc tts:start 分支
if (strcmp(state->valuestring, "start") == 0) {
    aborted_ = false;                               // 同步清零（已有，挪到 Schedule 外）
    Schedule([this]() { SetDeviceState(kDeviceStateSpeaking); });
}

// application.cc OnIncomingAudio
if (aborted_) return;                               // gate
if (GetDeviceState() == kDeviceStateSpeaking) {
    audio_service_.PushPacketToDecodeQueue(std::move(packet));
}
```

外加 `aborted_` 字段改 `std::atomic<bool>`。

此改动**完全向后兼容**——server 不发任何新消息，行为与现状一致；只是 realtime 模式 barge-in 残留消除。其他两个模式不受影响。
