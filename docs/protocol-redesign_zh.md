# 小智协议优化方案（二次开发）

> 适用场景：自建后端 + 局域网/私有部署。基于现有 `main/protocols/` + `main/ota.cc` + `main/application.cc` 的实现做演进。
>
> 配套文档：[signaling-flow_zh.md](signaling-flow_zh.md) / [ota-protocol_zh.md](ota-protocol_zh.md) / [server-integration_zh.md](server-integration_zh.md)

---

## 0. 设计原则

在动刀之前先约束清楚，避免越改越乱：

1. **单一通道优先**：信令 / 音频 / 通知 / OTA / 激活 全部走一条长连接，唯一例外是固件二进制下载（流量大，CDN 友好）
2. **协议层做版本协商**：当前 `version` 字段名存实亡，新协议必须可以平滑升级
3. **能力声明 (features) 取代版本号 routing**：要新功能就加 feature flag，不要靠 version 数字判断
4. **二进制帧自描述**：一个统一的 frame header，承载 opus / pcm / json / control 多种 payload type
5. **保留 fallback 到老协议的开关**：在线设备灰度切换时需要

---

## 1. 现状速览：痛点清单

### 1.1 通道层

| 痛点 | 影响 | 位置 |
|---|---|---|
| WS 模式 idle 时**不保持长连接**，每次唤醒重连 | 服务器无法主动下发；首包延迟高（TLS 握手 + hello RTT） | `websocket_protocol.cc:23-26` |
| WS 模式**无应用层心跳**，120s 超时全靠下行包刷新 | 半开连接难发现 | `protocol.cc:81-90` |
| MQTT + UDP 双通道，UDP 还要 AES 自加密 | 实现复杂度高，NAT/弱网不友好 | `mqtt_protocol.cc:166-295` |
| MQTT 模式硬编码 `version: 3` | 没有真实协商空间 | `mqtt_protocol.cc:301` |
| WS 客户端**不校验** server hello 的 `version` | 协议升级无对账 | `websocket_protocol.cc:228-254` |

### 1.2 引导层

| 痛点 | 影响 | 位置 |
|---|---|---|
| OTA HTTP 同时承担 4 个职责（版本/配置/激活/校时） | 接口语义混乱，扩展靠加字段 | `ota.cc:77-245` |
| 配置下发只在启动时，运行中变更需重启 | 服务端动态切换 backend 不可能 | `application.cc:323-338` |
| 激活轮询用 HTTP 202，3s 一次 | 用户体验差 | `application.cc:456-469` |
| `firmware.url` 单独走 HTTP GET | OK，但 CDN 鉴权要单独做 | `ota.cc:282` |
| 没有 server-initiated 通知通道 | 不能"服务端推消息让设备说话" | — |

### 1.3 数据层

| 痛点 | 影响 | 位置 |
|---|---|---|
| 三种 binary frame 格式 (v1/v2/v3)，且**仅区别在头部** | 维护成本高，编解码分支多 | `protocol.h:17-31` + `websocket_protocol.cc:33-146` |
| 硬编码 `format: "opus"` | 局域网想用 PCM / G.711 / AAC 都得改代码 | `websocket_protocol.cc:216`，`mqtt_protocol.cc:310` |
| 上下行采样率可不同（16k/24k）需 client resample | 失真，CPU 浪费 | `application.cc:511-514` |
| Opus `frame_duration` 客户端写死 60ms | 实时性要求高的场景不够灵活 | hello 里固定 |

### 1.4 状态层

| 痛点 | 影响 | 位置 |
|---|---|---|
| session_id 字符串拼接 JSON，未转义 | 服务端伪造 session_id 含 `"` 可破坏消息 | `protocol.cc:42-78` |
| `abort` 消息没有携带服务端 turn_id | 多 turn 场景容易错误打断上一轮 | `protocol.cc:42-49` |
| `tts state=stop` 后直接切换状态，**无音频播完确认** | 弱网下尾音被吞 | `application.cc:536-545` |

---

## 2. 三个核心需求的方案

### 2.1 长连接 + 服务端推送

**目标**：设备 idle 时也保持一条到 server 的长连接，server 可以主动 push 通知（来电、闹钟、IoT 联动、定向广播）。

#### 方案 A（推荐）：把 WS 通道生命周期改为「设备上线即连」

```
现状: Activating ─► Idle ─► (唤醒) ─► OpenAudioChannel ─► WS Connect ─► Hello
                                                            ↑
                                                       每次唤醒都重做

目标: Activating ─► WS Connect ─► Hello (control) ─► Idle (长连)
                                       ↓
                                  唤醒时仅发 listen start
                                  收到 server push 时直接处理
```

**实现要点**（改动量：中）

| 项 | 改法 |
|---|---|
| `WebsocketProtocol::Start()` | 不再返回 `true`，改成实际 `Connect + Hello`（合并 `OpenAudioChannel` 的握手部分） |
| `OpenAudioChannel()` | 退化成"切换到音频上下文"，只发 `listen start` |
| `CloseAudioChannel()` | 不真的断 socket，只发 `listen stop` 或新增 `session end` 信令 |
| 新增 `kHelloStateControl` / `kHelloStateAudio` | 区分"控制长连"和"带音频上下文" |
| 应用层 ping/pong | 每 30s 一次，3 次失败重连（替代 120s 被动超时） |
| 断线重连 | 指数退避，state machine 不应进入 `Connecting` UI |

**Tradeoff**

| | 利 | 弊 |
|---|---|---|
| 长连 | server push 立即可用；首包延迟 -300~500ms（省 TLS+TCP+hello RTT）| 设备常驻一条 TCP 连接，server 端 fd / 内存上升 |
| 心跳 | 半开连接秒级发现 | 流量 +100B/min/device |

千~万级设备，wss 长连成本完全可接受（一条 wss 空闲 ~4KB 内存 server 端）。

#### 方案 B：单独开一条「通知 WS」与音频 WS 并存

不推荐，违反"单一通道"原则，运维成本翻倍。仅在**音频通道必须用 UDP**（公网弱网）时考虑。

#### Server push 消息建议格式

```jsonc
// 通用 push（不需要会话上下文）
{ "type": "push", "category": "notification",
  "payload": { "title": "...", "body": "...", "tts": "可选的播报文本" } }

// 服务端发起一次会话（主动叫醒设备说话）
{ "type": "push", "category": "invoke",
  "payload": { "tts": "提醒你 3 点开会了" } }
// 设备收到后：SetDeviceState(Speaking) + 接受紧随的 tts 流

// 远程控制
{ "type": "push", "category": "control",
  "payload": { "action": "reboot|reload_config|set_volume|..." } }
```

`application.cc:526-612` 的 `OnIncomingJson` 分发器**只需加 push 分支**，状态机不动。

---

### 2.2 协议统一与简化

**目标**：删 MQTT + UDP 路径，OTA HTTP 收敛到长连接，激活也并进来。

#### 2.2.1 收敛方案

```
┌─────────────────────────────────────────────────────┐
│                老架构                                │
│  HTTP OTA  ──┐                                       │
│  HTTPS Activate ─┼──► 启动期 (异步任务)              │
│  WS / MQTT  ──┘                                      │
│  UDP audio (仅 MQTT 模式)                            │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│                新架构                                │
│  Bootstrap HTTP  (一次性, 拿 token + ws_url)         │
│           │                                          │
│           ▼                                          │
│  WSS 长连接                                          │
│   ├─ control: hello / config / activate / push       │
│   ├─ audio:   pcm | opus | g711 (binary frame)       │
│   ├─ ota:     server push 触发, 设备 GET 二进制      │
│   └─ mcp:     工具调用透传                           │
└─────────────────────────────────────────────────────┘
```

#### 2.2.2 Bootstrap HTTP（仅保留这一个 HTTP 接口）

```
POST /bootstrap
Headers:
  Device-Id, Client-Id, Serial-Number?, User-Agent
Body: { "mac": "...", "uuid": "...", "fw_version": "...", "sn": "..." }

Resp 200:
{
  "ws_url":  "wss://...",
  "token":   "...",             // 长连接鉴权用
  "token_exp": 1735689600,      // 过期时间
  "server_time": <ms>
}

Resp 401: 未注册/未激活，返回 activation challenge
Resp 426: 强制升级，返回 firmware url
```

**比现状好在哪**：
- 一个接口一个语义，不再把激活/升级/配置/校时塞一起
- token 化，避免每次请求带 SN/MAC 暴露设备身份
- 强制升级走 HTTP 状态码，不靠业务字段

#### 2.2.3 长连接握手

```jsonc
// Client → Server (替代现有 hello)
{
  "type": "hello",
  "protocol": { "version": 4, "min_compatible": 4 },   // 真正的协商
  "device": { "uuid": "...", "fw": "2.3.0", "board": "..." },
  "auth":   { "token": "<from bootstrap>" },
  "features": {                                         // 能力声明
    "mcp": true,
    "aec": false,
    "pcm": true,                                        // 支持的音频编码
    "opus": true,
    "push": true,                                       // 支持 server push
    "ota_inband": true                                  // 支持长连内触发 OTA
  },
  "audio": {                                            // 默认偏好, 实际编码用每帧 frame header
    "preferred_codec": "pcm",                           // pcm | opus | g711
    "sample_rate": 16000,
    "channels": 1
  }
}

// Server → Client
{
  "type": "hello",
  "protocol": { "version": 4 },
  "session_id": "...",
  "server_time": <ms>,
  "audio": {                                            // 服务器决定的编码
    "codec": "pcm",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 20                                // ms
  },
  "limits": {
    "max_audio_frame_bytes": 4096,
    "idle_timeout": 600
  }
}
```

**关键变化**
- `protocol.version` 真正用于路由 + `min_compatible` 用于协商失败时设备 fallback / 升级
- `features` 取代隐含约定
- `audio` 双向，client 声明偏好、server 决定实际编码

#### 2.2.4 统一二进制帧

替换现有三种 `BinaryProtocol1/2/3`，定义一个：

```c
struct UnifiedFrame {
    uint8_t  magic;          // 0xAA, 用于快速错位检测
    uint8_t  type;           // 0=audio_opus, 1=audio_pcm, 2=audio_g711,
                             // 16=json_control, 32=ota_chunk, 64=mcp_binary
    uint8_t  flags;          // bit0: last_in_turn, bit1: encrypted, bit2: compressed
    uint8_t  reserved;
    uint32_t seq;            // 单连接单调递增, 便于丢包检测
    uint32_t timestamp_ms;   // 仅 audio 使用, 用于 server-side AEC
    uint16_t payload_len;
    uint8_t  payload[];
} __attribute__((packed));   // 14 字节头
```

好处：
- 三种 audio 编码同一套封装代码
- 控制消息和音频帧可在同一连接里乱序而不冲突（type 区分）
- `seq` 让 server 能算丢包率
- `flags` 留扩展位

#### 2.2.5 激活并入长连接

```
设备 Bootstrap 拿到 token (即使是 "activation_token")
     ↓
长连接 hello, server 检查发现未激活
     ↓
Server → { "type":"activation", "state":"required",
           "code":"123456", "challenge":"...", "message":"..." }
     ↓
Client 显示激活码 + 计算 HMAC
     ↓
Client → { "type":"activation", "state":"submit",
           "hmac":"...", "serial_number":"..." }
     ↓
Server → { "type":"activation", "state":"done" }
              或 { "state":"pending" }  ← 不再轮询，server push 通知
```

**省掉的复杂度**：HTTP 202 + 3s 轮询循环（`application.cc:456-469`）整段砍掉。

#### 2.2.6 OTA 触发并入长连接

```
Server 决定要升级某设备:
    Server → { "type":"ota", "action":"available",
               "version":"2.4.0", "url":"https://...", "force": false }
Client 接受 → 仍然走 HTTPS GET 下载二进制 (CDN 友好)
            → 写完后回报 { "type":"ota", "action":"finished" }
            → 自己 reboot
```

二进制下载留 HTTP 是合理的：
- 长连接传 2MB 固件会阻塞信令
- CDN 缓存机制完整
- HTTP Range 续传成熟

---

### 2.3 PCM 格式支持（局域网零编解码）

#### 2.3.1 现状

`audio_service.cc` 编码路径硬编码 opus encoder，`hello.audio_params.format = "opus"`。设备端解码也是 opus。

#### 2.3.2 改造点

| 文件 | 改动 |
|---|---|
| `protocol.h` | `enum AudioCodec { kCodecOpus, kCodecPcm, kCodecG711 };` |
| hello | `audio.codec` 协商完写入 `protocol_->set_codec()` |
| `audio_service.cc` | encoder/decoder 工厂化，PCM 模式跳过编解码直接 memcpy |
| `UnifiedFrame.type` | 0/1/2 区分音频编码，receive 路径按 type 解析 |
| `audio_codec.h` | 现有 codec 抽象保持，新增 `RawCodec` 实现（passthrough） |

#### 2.3.3 带宽与性能预估

| 编码 | 16kHz mono | 24kHz mono | CPU (S3) |
|---|---|---|---|
| Opus 24kbps | 3 KB/s | 3 KB/s | enc ~12% / dec ~5% |
| PCM 16-bit | **32 KB/s** | **48 KB/s** | ~0% |
| G.711 µ-law | 8 KB/s | — | <1% |

**结论**：
- 局域网 WiFi（>50Mbps）跑 PCM 完全没问题，且省 ~15% CPU、降低首包延迟（不用等 opus encoder 攒帧）
- 跨公网建议保持 opus
- G.711 是中间档（窄带电话品质），CPU/带宽折中

#### 2.3.4 推荐配置策略

```jsonc
// 设备 hello 里声明能力 + 偏好
"features": { "opus": true, "pcm": true, "g711": true },
"audio": { "preferred_codec": "pcm", "sample_rate": 16000 }

// Server 端策略（伪代码）
if (client_ip_is_lan(device) && device.features.pcm)
    use pcm 16k
else if (device.features.opus)
    use opus 16k
else
    fallback
```

#### 2.3.5 注意点

1. **采样率必须双向一致**：现在 `application.cc:511-514` 会 resample，PCM 模式下应直接拒绝不一致的握手（log 已经在告警了，干脆做硬性校验）
2. **AEC 是否还需要**：PCM 模式下，server-side AEC 更容易做（直接对原始 PCM 做参考信号对齐），可以在 features 里去掉 device-side AEC 编译开关的耦合
3. **帧大小**：opus 一般 60ms/帧，PCM 建议 20ms/帧（16k mono = 640 字节/帧），更低延迟、更平滑

---

## 3. 其他可优化点（按 ROI 排序）

### 3.1 高 ROI（建议做）

#### A. session_id / 字符串字段做 JSON 转义
`protocol.cc:42-78` 用字符串拼接，session_id 含 `"` 会破坏 JSON。改用 cJSON 构建（参考 `mqtt_protocol.cc:297-320` 的写法）。**安全 + 健壮性**，改动 30 行。

#### B. 应用层 ping/pong
现 `IsTimeout()` 120s 太长，且只有下行有包才更新 `last_incoming_time_`。加 30s 主动 ping，3 次没 pong 主动重连。

```jsonc
// Client → Server
{ "type":"ping", "ts": 1735689600000 }
// Server → Client
{ "type":"pong", "ts": 1735689600000 }
```

#### C. turn_id / req_id 贯穿
新增字段 `turn_id` (server 在 listen start 响应里下发)，abort / stt / llm / tts / mcp 都带上。**解决多 turn 乱序**。

```jsonc
// Server (listen start 应答)
{ "type":"listen", "state":"started", "turn_id":"t-001" }
// Client (后续都带 turn_id)
{ "type":"abort", "turn_id":"t-001" }
```

#### D. tts 播放完成回报
弱网下 `tts state=stop` 客户端立即切状态导致尾音丢失。增加：

```jsonc
// Client → Server (本地音频队列播完)
{ "type":"tts", "state":"played", "turn_id":"t-001" }
```

server 端可以基于此做"读完了再问下一个问题"的同步。

#### E. 错误码体系
现在错误统一用 i18n 字符串（`Lang::Strings::SERVER_ERROR`），server 端没法精确处理。建议：

```jsonc
// Server → Client
{ "type":"error", "code":4001, "message":"...", "fatal":false }

// Client → Server (上报本地错误)
{ "type":"error", "code":5001, "message":"opus decode failed",
  "context":{ "turn_id":"...", "seq":42 } }
```

约定 4xxx = server-side、5xxx = client-side、code 段位区分模块。

---

### 3.2 中 ROI（看场景）

#### F. mTLS 设备证书（替代 token）
当前 ws Authorization Bearer token 可以被刷固件后泄露。生产部署建议：
- 烧录期写入 device certificate（efuse 或加密分区）
- WSS 握手用 mTLS
- 服务端通过 cert CN 识别设备

工作量大，但去除了 token 轮换 / 撤销的所有问题。

#### G. 流控 / 拥塞反馈
现状：设备无脑发音频，发不出去就 `return false`（`websocket_protocol.cc:28-31`）。改进：
- 服务端定期下发 `{ "type":"flow", "audio_credit": 100 }` 类的窗口
- 设备根据 credit 控制发送速率
- 弱网下不至于把队列堆满 OOM

#### H. Push 优先级与去重
server push 通知可能在设备 Speaking 中到达，需要：
- `priority`: low | normal | urgent
- `dedup_key`: 同一 key 仅展示一次
- `ttl`: 过期就不展示

#### I. 配置热更新
长连接打开后，server 可以下发新配置：

```jsonc
{ "type":"config", "patch": { "wake_word_threshold": 0.6, "tts_volume": 80 } }
```

设备本地存 NVS、热应用。比现状（必须 reboot 才能改）灵活。

#### J. 多设备绑定 / 用户上下文
现协议没有 user 概念。如果做家庭场景：
- bootstrap 阶段返回 `user_id` / `group_id`
- 每条消息可带 `user_id`（多个家庭成员一台设备）

---

### 3.3 低 ROI（除非有强需求）

#### K. WebRTC 替代 WS+UDP
公网弱网场景才有意义；自建局域网 WSS 完全够。

#### L. gRPC / Protobuf 替代 JSON
ESP32-S3 解 JSON 不算瓶颈（cJSON 几 KB/ms），protobuf 库引入会增加 30~50KB flash。除非节点数千万级、JSON 解析成 CPU 瓶颈，不建议换。

#### M. 加密音频帧
WSS 本身已加密。除非用 plain WS + 自加密（不推荐），否则不需要再加一层。MQTT+UDP 模式那套 AES-CTR 直接砍掉。

---

## 4. 推荐演进路线

按"风险递增 + 价值递减"排序，分四期：

### Phase 1（1-2 周）：稳定性 + 易用性
1. WS 模式应用层 ping/pong（替代 120s 被动超时）
2. session_id / 字符串字段 JSON 转义
3. `turn_id` 贯穿
4. tts played 回报
5. error code 标准化

**收益**：现网 bug 收敛，问题定位时间 -50%。

### Phase 2（2-3 周）：长连接 + Server Push
1. WS 改为「上线即连」+ idle 保活
2. 新增 `push` 消息类型 + handler
3. 断线重连状态机
4. 应用层流控基础（credit）

**收益**：解锁服务端主动通知能力。

### Phase 3（3-4 周）：协议统一
1. 设计 `UnifiedFrame` 替代三种 binary protocol
2. Bootstrap HTTP 替代 OTA HTTP（保留老接口做兼容）
3. 激活并入长连接
4. OTA available push + HTTP 下载分离

**收益**：协议层代码量 -40%，server 实现复杂度大幅下降。

### Phase 4（2 周）：PCM + 编码协商
1. AudioCodec 工厂
2. hello 双向音频协商
3. PCM passthrough 实现
4. server 端按网络环境选 codec

**收益**：局域网首包延迟 -100ms，CPU -15%，便于做 server-side AEC。

---

## 5. 兼容性方案

如果存量设备需要保留，建议：

1. **协议版本号真正使用**：`protocol.version=4` 触发新协议，老固件停在 `version=1/2/3`
2. **Bootstrap 接口同时支持新旧**：URL 不变 `OTA_URL`，根据请求头里的 `Protocol-Version` 返回不同 JSON schema
3. **WS 端点分两个**：`wss://.../v3` 老协议、`wss://.../v4` 新协议；hello 时引导切换
4. **灰度策略**：bootstrap 响应里加 `feature_flags`，按 device_id 灰度开新协议

---

## 6. 关键决策点（需要你拍板）

| 决策 | 选项 | 倾向 |
|---|---|---|
| MQTT/UDP 模式 | 保留 / 删除 | **删除**（自建后端用不到，简化 30% 协议代码） |
| Bootstrap 是否拆出 | 拆 / 维持 OTA HTTP | **拆**（语义清晰，老接口可保留兼容） |
| 鉴权方案 | token / mTLS / 混合 | **token 先行**，mTLS 二期（视生产规模） |
| 音频默认编码 | opus / pcm / 协商 | **协商**，server 按 client_ip 自动选 |
| 帧大小 | 60ms / 20ms / 协商 | **协商**，PCM 默认 20ms，opus 默认 60ms |
| 老协议兼容 | 砍 / 共存 | **共存 1 年**（量产设备需要） |

---

## 7. 风险与回退

| 风险 | 缓解 |
|---|---|
| 长连接对 server 资源压力 | 用 nginx/envoy 做接入层，业务 server 无状态；千~万级设备一台 8C16G 接入足够 |
| PCM 占带宽，WiFi 抖动导致丢帧 | Phase 4 实施时同步实现 server 端 jitter buffer；提供 codec 一键回退 opus |
| 协议大改引入新 bug | Phase 1 先稳定老协议；新协议单独 endpoint 灰度；保留老 endpoint 至少 1 年 |
| 设备端 OTA 失败回滚 | 沿用现 `MarkCurrentVersionValid` 机制，pending_verify 状态下没确认就自动回滚 |

---

## 附：相关代码位置速查

| 主题 | 文件 |
|---|---|
| 应用主循环 + 协议挂回调 | `main/application.cc:473-615` |
| WebSocket 协议 | `main/protocols/websocket_protocol.cc` |
| MQTT + UDP 协议 | `main/protocols/mqtt_protocol.cc` |
| 协议基类 + 上行消息 | `main/protocols/protocol.cc` |
| OTA / 激活 HTTP | `main/ota.cc` |
| Binary frame 定义 | `main/protocols/protocol.h:17-31` |
| 设备状态机 | `main/device_state_machine.cc` |
| 音频流水线 | `main/audio/audio_service.cc` |
