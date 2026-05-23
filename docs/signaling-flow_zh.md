# 小智信令交互流程图

本文档梳理 xiaozhi-esp32 端与服务器之间 **信令通道** 的完整交互流程。信令在 WebSocket 模式下与音频流复用同一连接（JSON 文本帧 vs 二进制帧），在 MQTT 模式下独占 MQTT 通道，音频走另一条 UDP+AES-CTR。所有消息体格式与传输无关。

代码参考：
- `main/protocols/protocol.cc` / `protocol.h`
- `main/protocols/websocket_protocol.cc`
- `main/protocols/mqtt_protocol.cc`
- `main/application.cc`（下行消息分发）

---

## 1. 握手阶段（OpenAudioChannel）

```mermaid
sequenceDiagram
    autonumber
    participant App as Application
    participant P as Protocol<br/>(WS / MQTT)
    participant S as Server

    Note over App,P: 设备触发开启会话<br/>(唤醒 / 按键 / 来电)
    App->>P: OpenAudioChannel()

    alt MQTT 模式且未连接
        P->>S: MQTT CONNECT (keepalive=240s)
        S-->>P: CONNACK
    end

    P->>S: hello {version, transport,<br/>features:{aec, mcp},<br/>audio_params:{opus,16k,1ch,60ms}}
    Note over S: 分配 session_id<br/>MQTT 模式还分配 UDP 通道 + AES key/nonce

    S-->>P: hello {transport, session_id,<br/>audio_params,<br/>udp:{server,port,key,nonce}(仅 MQTT)}
    P->>P: ParseServerHello<br/>缓存 session_id / 采样率<br/>MQTT 模式初始化 AES + 连 UDP

    alt 10s 内未收到服务器 hello
        P-->>App: SetError(SERVER_TIMEOUT)
        Note over App: 通道打开失败
    else 收到 hello
        P-->>App: on_audio_channel_opened_
        Note over App: 进入 Listening / Speaking 流程
    end
```

---

## 2. 一次完整会话（Listen → STT → LLM → TTS）

```mermaid
sequenceDiagram
    autonumber
    participant U as User
    participant D as Device
    participant P as Protocol
    participant S as Server

    rect rgb(240,248,255)
    Note over D,S: 已完成 hello 握手，通道打开
    end

    alt 唤醒词触发
        U->>D: "你好小智"
        D->>P: SendWakeWordDetected("你好小智")
        P->>S: {type:listen, state:detect, text:"你好小智"}
    end

    D->>P: SendStartListening(mode)
    P->>S: {type:listen, state:start,<br/>mode:auto|manual|realtime}

    par 用户说话（音频流）
        loop 每帧 60ms Opus
            D-->>S: 二进制音频帧<br/>(WS Binary / UDP 加密)
        end
    and 服务端实时反馈
        S-->>P: {type:stt, text:"用户说的话"}
        P-->>D: 显示用户消息到屏幕
    end

    alt 自动停止 (VAD)
        Note over S: 服务端 VAD 判定结束
    else 手动停止
        D->>P: SendStopListening()
        P->>S: {type:listen, state:stop}
    end

    S-->>P: {type:llm, emotion:"happy"}
    P-->>D: 切换表情

    S-->>P: {type:tts, state:start}
    P-->>D: 进入 Speaking 状态

    loop TTS 流式输出
        S-->>P: {type:tts, state:sentence_start, text:"这一句字幕"}
        P-->>D: 屏幕显示 assistant 字幕
        S-->>D: 二进制音频帧 (Opus)
        D->>D: 解码并播放
    end

    S-->>P: {type:tts, state:stop}
    P-->>D: 回到 Idle 或 Listening<br/>(由 listening_mode 决定)
```

---

## 3. 打断流程（Abort）

```mermaid
sequenceDiagram
    autonumber
    participant U as User
    participant D as Device
    participant P as Protocol
    participant S as Server

    Note over S,D: TTS 正在播放

    alt 用户按打断键
        U->>D: 按键 / 触摸
        D->>P: SendAbortSpeaking(kAbortReasonNone)
        P->>S: {type:abort}
    else 检测到唤醒词
        U->>D: "小智"
        D->>P: SendAbortSpeaking(kAbortReasonWakeWordDetected)
        P->>S: {type:abort, reason:"wake_word_detected"}
    end

    S-->>S: 停止 TTS 生成
    S-->>P: {type:tts, state:stop}
    P-->>D: 回到 Idle / Listening
```

---

## 4. 服务端推送类消息（system / alert / mcp / custom）

```mermaid
flowchart TD
    A[Server 下行消息<br/>OnIncomingJson] --> B{type 字段}

    B -->|tts| T[更新 Speaking 状态<br/>显示字幕]
    B -->|stt| ST[显示用户文本]
    B -->|llm| L[更新表情]

    B -->|mcp| M[McpServer::ParseMessage<br/>执行设备工具调用]
    M -->|工具执行结果| MR[SendMcpMessage<br/>type:mcp, payload:...]
    MR --> S2[(Server)]

    B -->|system| SC{command}
    SC -->|reboot| R[Application::Reboot<br/>OTA 后重启]
    SC -->|其他| W1[日志告警<br/>Unknown system command]

    B -->|alert| AL[Alert 弹窗<br/>+ 振动提示音]

    B -->|custom<br/>需 CONFIG_RECEIVE_CUSTOM_MESSAGE| CU[屏幕 system 行<br/>原样显示 payload]

    B -->|未知 type| W2[ESP_LOGW<br/>Unknown message type]
```

---

## 5. 通道关闭（Goodbye）

```mermaid
sequenceDiagram
    autonumber
    participant App as Application
    participant P as Protocol
    participant S as Server

    alt 客户端主动关闭
        App->>P: CloseAudioChannel(send_goodbye=true)
        alt WebSocket 模式
            Note over P: 不发 goodbye<br/>直接 reset socket
            P->>S: TCP FIN
        else MQTT 模式
            P->>S: {type:goodbye, session_id}
            P->>P: udp_.reset()<br/>关闭音频通道
        end
        P-->>App: on_audio_channel_closed_

    else 服务端主动关闭 (仅 MQTT)
        S-->>P: {type:goodbye, session_id}
        Note over P: session 匹配则关通道<br/>但不回 goodbye<br/>避免 ping-pong
        P-->>App: on_audio_channel_closed_

    else 网络异常断开
        Note over P,S: TCP / MQTT 断链
        P-->>App: on_audio_channel_closed_
        opt MQTT 模式
            P->>P: 启动 60s 重连定时器<br/>仅在 Idle 状态重连
        end
    end
```

---

## 6. 设备状态机（与信令耦合的部分）

```mermaid
stateDiagram-v2
    [*] --> Idle

    Idle --> Connecting: OpenAudioChannel()
    Connecting --> Idle: 握手超时 / 失败
    Connecting --> Listening: 收到 server hello

    Listening --> Speaking: tts state=start
    Speaking --> Listening: tts state=stop<br/>(mode != manual)
    Speaking --> Idle: tts state=stop<br/>(mode == manual)

    Speaking --> Listening: abort 上行<br/>(被打断)

    Listening --> Idle: 通道关闭
    Speaking --> Idle: 通道关闭

    Idle --> Idle: alert / system 消息
```

---

## 7. 信令消息一览表

### 上行（Client → Server）

| type | 子字段 | 触发点 | 说明 |
|---|---|---|---|
| `hello` | version, features, audio_params, transport | 握手 | 协议版本与能力协商，10s 内未应答则失败 |
| `listen` | state=detect, text | 本地唤醒 | 上报唤醒词，触发服务端进入会话 |
| `listen` | state=start, mode | 开始拾音 | mode: auto(VAD) / manual / realtime(需 AEC) |
| `listen` | state=stop | 用户结束说话 | 仅 manual / realtime 显式发送 |
| `abort` | 可选 reason | 打断 TTS | reason="wake_word_detected" 表示被唤醒词打断 |
| `mcp` | payload | MCP 工具调用回执 | 设备端 MCP server 的响应/通知 |
| `goodbye` | session_id | MQTT 通道关闭 | WS 模式不发，直接断开 socket |

### 下行（Server → Client）

| type | 子字段 | 处理 | 说明 |
|---|---|---|---|
| `hello` | session_id, audio_params, udp(MQTT) | 协议层 | 握手回包，分配会话 ID |
| `tts` | state=start/stop/sentence_start, text | 状态机 + UI | TTS 生命周期与字幕 |
| `stt` | text | UI | ASR 识别结果 |
| `llm` | emotion | UI | 模型情绪标签 |
| `mcp` | payload | McpServer | 服务端调用设备工具 |
| `system` | command | 系统 | 目前仅 `reboot`（OTA 后用） |
| `alert` | status, message, emotion | UI + 提示音 | 告警弹窗 |
| `custom` | payload | UI（可选编译） | 业务自定义透传 |
| `goodbye` | session_id | 协议层 | 服务端主动关闭会话 |

---

## 8. 关键约束

- **必须先收到服务器 `hello` 才能开始任何业务消息**，否则 10s 超时（`OpenAudioChannel`）。
- **`session_id` 由服务器分配**，所有上行消息都会带上。
- **MQTT 模式下信令与音频分离**：信令走 MQTT topic，音频走独立 UDP，hello 阶段下发 UDP 地址与 AES-CTR key/nonce。
- **服务端发的 `goodbye` 客户端不回**，避免 ping-pong。
- **应用层超时**：`Protocol::IsTimeout()` 判定 `last_incoming_time_` 超过 120s 视为通道失效。
- **WebSocket 模式无应用层心跳**（底层 `WebSocket::Ping()` 未被调用）；**MQTT 模式靠协议 keepalive（默认 240s）**。
