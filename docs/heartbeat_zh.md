# 心跳与长连接（Heartbeat & Keep-Alive）

> 在原协议基础上引入的小幅扩展，目标是**支持服务器主动下发通知**并**让 server 更快感知设备离线**。
>
> 本文档与服务器端 `firmware-heartbeat-spec.md` 对齐。老固件可继续接入（行为退化但功能不缺失）。

适用代码：
- `main/protocols/websocket_protocol.{h,cc}` — WS 长连接 + 应用层 ping/pong + 重连退避
- `main/protocols/mqtt_protocol.cc` — MQTT keepalive 调整 + Last Will (LWT)
- `managed_components/78__esp-ml307/include/mqtt.h` + 各实现 — 抽象类新增 `SetWill` / `Publish(... retain)`

---

## 1. 设计目标

| 需求 | 实现手段 |
|---|---|
| 设备在线时随时可接收服务器下发 | WS 改为「激活后即连」长期保持；MQTT 保持 broker 长连 |
| 服务器能及时感知设备离线 | WS: 应用层 ping/pong 超时；MQTT: 协议层 keepalive 60s + Last Will |
| 设备能从异常断开恢复 | 指数退避自动重连 |

---

## 2. WebSocket 长连接

### 2.1 生命周期变化

**旧行为**：唤醒/按键 → `OpenAudioChannel` 才建连 → 对话结束 `CloseAudioChannel` 直接断 socket。空闲时 WS 不存在，服务器无法 push。

**新行为**：

```
激活完成 (ActivationDone)
  └─► WebsocketProtocol::Start()
        └─► ConnectAndHello()          // 建连 + hello + 启动 ping timer
              └─► 长连接持续 (Idle 状态)

唤醒 / 按键
  └─► OpenAudioChannel()
        └─► 复用既有长连接, 仅置 audio_session_active_=true
              └─► on_audio_channel_opened_  // 上层切到 Listening

对话结束
  └─► CloseAudioChannel(send_goodbye=false)
        └─► audio_session_active_=false, 触发 on_audio_channel_closed_
              └─► 长连接保留, ping 继续

服务器 push (例如 {"type":"tts","state":"start",...})
  └─► OnIncomingJson 回调 → Application 走原有 dispatcher
```

### 2.2 应用层心跳

**协议**（与 server `firmware-heartbeat-spec.md §2` 对齐）：

```jsonc
// Client → Server, 每 60 秒
{ "type": "ping", "timestamp": <ms_since_boot> }

// Server → Client
{ "type": "pong", "timestamp": <echo> }
```

- **字段名必须是 `timestamp`**（不是 `ts`）—— server 按这个字段对账
- pong **不进任何业务回调**，仅用于刷新 `last_pong_time_us_`（pong 超时检测）和 `last_incoming_time_`（`IsTimeout()` 兜底）

**参数**：

| 项 | 值 | 来源 |
|---|---|---|
| ping 默认间隔 | 60s | `WEBSOCKET_PING_INTERVAL_DEFAULT_S` |
| OTA 覆盖字段 | `ws_ping_interval`（秒，0 = 关闭主动心跳） | `Settings("websocket").GetInt(...)` |
| Pong 超时阈值 | 90s | `WEBSOCKET_PONG_TIMEOUT_MS` |

**Pong 超时判定**：每次 ping 触发时检查 `now - last_pong_time_us > 90s` → 主动 `websocket_.reset()` → 走 `HandleDisconnected` → 安排重连。比 server 端 `ConnectionTimeoutSec=120s` 提早 30s 自查，作为额外保险。

### 2.3 重连退避

| 阶段 | 间隔 |
|---|---|
| 第 1 次 | 1s |
| 第 N 次 | `2^(N-1)` s |
| 封顶 | 60s |

重连成功后 `reconnect_backoff_s_` 重置为 1s。

### 2.4 状态语义变化（重要）

`Protocol::IsAudioChannelOpened()` 的含义变化：

| | 旧含义 | 新含义 |
|---|---|---|
| WS | `websocket_ != nullptr && connected` | `audio_session_active_ == true` |
| MQTT | `udp_ != nullptr && ...` | 不变（与新 WS 语义已经一致） |

**WS 长连接是否存活**与 **audio session 是否活动**现在是两个独立概念，前者无外部 query 接口（内部使用）。

`Protocol::IsTimeout()` 阈值 **保持 120s 不变**，与 server `ConnectionTimeoutSec` 对齐。

---

## 3. MQTT 长连接

### 3.1 Keepalive

| 项 | 旧值 | 新值 |
|---|---|---|
| `Settings("mqtt").GetInt("keepalive", default)` 默认 | 240 | **60** |

**注意**：OTA 下发的 `mqtt.keepalive` 仍然优先；改默认值只影响"OTA 未指定 keepalive 时"的回退。

MQTT 协议层 PINGREQ/PINGRESP 由 `esp-mqtt` 自动处理，业务无需关心。

### 3.2 Last Will (LWT)

**设置时机**：`MqttProtocol::StartMqttClient` 中，在调用 `mqtt_->Connect()` 之前 `SetWill(...)`。

```
Topic:   /p2p/device_public/<mac_underscore>
         例: /p2p/device_public/aa_bb_cc_dd_ee_ff
Payload: {"type":"goodbye","device_id":"aa:bb:cc:dd:ee:ff"}
QoS:     0
Retain:  false
```

**重点设计选择**：

1. **topic 不复用 `publish_topic_`**：直接由 MAC 拼，与 server 端约定的 `/p2p/device_public/<mac_u>` 路径完全一致。即使 OTA 未下发 `publish_topic_`，LWT 仍能正确设置。
2. **payload 不带 `session_id`**：MQTT will 必须在 CONNECT 报文里固化，而 session_id 由 hello-ack 之后才能拿到，所以无法写进 will。server 端 adapter 收到 LWT republish 的 goodbye 时按 topic 上的 deviceID 路由，**不校验 session_id**。
3. **`device_id` 字段值用带冒号 MAC**：沿用 MQTT 业务消息约定。
4. **QoS=0 / retain=false**：一次性事件，不需要 retain。这点与"标准 online/offline 状态主题"模式不同。

**触发条件**（broker 行为）：

| 场景 | 是否触发 will |
|---|---|
| 设备 keepalive 超时（60s 没 PINGREQ） | ✓ |
| TCP 异常断开（断电/拔网线） | ✓ |
| 设备主动 DISCONNECT packet | ✗ |
| 设备发送 graceful disconnect 后掉线 | ✗ |

**重要**：设备主动 reset / OTA reboot 时不会发 will（走 DISCONNECT 流程）。这种"主动下线"由 server 业务自行处理，或固件后续加一条主动 publish goodbye 再 disconnect。本次未实现，详见 §6.2。

### 3.3 双路径离线感知

server 端有两条独立路径感知设备掉线，任一到达即清理 session：

| 路径 | 触发方式 | 含义 |
|---|---|---|
| LWT republish | broker 自动 publish will 到 topic | adapter 收到 `goodbye` 消息，业务层路由清理 |
| broker lifecycle hook | broker 端 `OnDisconnect` 钩子 | 翻动 `deviceLifecycleState`，关闭对应 transport |

设备侧只需正确设置 LWT，**不需要订阅或处理任何 status topic**。

---

## 4. 区分新老设备

server 端无需在设备 hello 里通过特定 feature flag 区分。判定逻辑：

```
新固件:
  WS:   60s 周期发 ping → server 看到周期心跳即认定为新固件, 按 120s 超时
  MQTT: keepalive=60s, 有 LWT → broker 60s 检测离线 + LWT republish 双路径

老固件:
  WS:   从不主动发 ping → server 按 ConnectionTimeoutSec 自然超时空闲连接,
        业务交互期间正常工作
  MQTT: keepalive=240s, 无 LWT → broker 360s 检测离线,
        仅 lifecycle hook 路径生效, 离线感知延迟退化 (功能不缺失)
```

`features.heartbeat` 字段**未添加**，与 server 端对齐。

---

## 5. 测试验证

### 5.1 MQTT 模式

| 验证项 | 操作 | 期望 |
|---|---|---|
| CONNECT 包字段 | 抓包 | `Keep Alive = 60`, `Will Flag = 1`, `Will Topic = /p2p/device_public/aa_bb_cc_dd_ee_ff`, `Will Payload` 是合法 goodbye JSON 含 `type` / `device_id` |
| 异常 will republish | 拔网线 / 断电 | ≤90s broker 在 will topic 上 republish goodbye, server 收到后清理 session |
| 主动断不发 will | 调用 `protocol_.reset()` (e.g. 切 AEC 模式) | broker 不 republish will（待改进：主动发 goodbye） |
| 重连后心跳 | 拔网恢复 | 60s 内 broker 收到 PINGREQ, 标记设备在线 |

### 5.2 WebSocket 模式

| 验证项 | 操作 | 期望 |
|---|---|---|
| 心跳报文 | 设备激活后 idle 90s | 抓包能看到每 60s 一条 `{"type":"ping","timestamp":<ms>}` |
| Pong 响应 | server 收到 ping 应在 < 100ms 内回 | `{"type":"pong","timestamp":<echo>}` |
| Pong 超时重连 | server 停止回 pong | 90s 后日志报 `Pong timeout`, 然后 1s/2s/... 退避重连 |
| Server push 可达 | server idle 状态推 `{"type":"alert",...}` | 设备屏幕显示 alert（无需先唤醒） |
| 断网恢复 | 拔 WiFi 30s 再插回 | 重连退避中, 网络恢复后下一次 retry 成功 |
| OTA 关闭心跳 | server 下发 `websocket.ws_ping_interval = 0` 后重启 | 日志看 `ping disabled`, 设备不再主动发 ping |
| Audio session 复用 | 第二次唤醒 | UI 仅闪烁极短 Connecting, TTS 首包延迟显著下降 |

---

## 6. 已知问题与待办

### 6.1 esp-ml307 依赖修改未持久化

`managed_components/78__esp-ml307/` 是 IDF Component Manager 从 registry 拉取的依赖，本次改动直接修改了：
- `include/mqtt.h`（加 `SetWill` + `Publish(..., retain)`）
- `src/esp/esp_mqtt.cc`（实现 will 配置）
- `src/{ml307,ec801e}/{*.h,*.cc}`（同步扩 `Publish` 签名）

**问题**：执行 `idf.py reconfigure` 或在新机器克隆后，managed_components 会被重新拉取，本地改动丢失。

**推荐的持久化方案**（任选一）：

1. **fork 到 `components/esp-ml307/`**：把整个 `managed_components/78__esp-ml307` 复制到仓库根目录 `components/` 下，并从 `main/idf_component.yml` 移除 `78/esp-ml307: ~3.6.5` 依赖。components/ 优先级高于 managed_components。
2. **向上游 esp-ml307 提 PR**：让 78 合并 SetWill / Publish retain 这两个改动。
3. **维护 patch 文件**：脚本化在每次 reconfigure 后自动应用。

当前未做选择，**量产前必须解决**。

### 6.2 设备主动 reboot/reset 时未发 goodbye

graceful disconnect 时 broker 不会发 will，且当前代码也没在 reboot/destructor 中主动 publish goodbye。结果：服务端在设备 reboot 期间会以为它还在线，直到下次 birth/hello 出现。

如需精确状态，可在 `Application::Reboot` 之前调 MQTT publish goodbye，或在 `MqttProtocol::~MqttProtocol` 中加 publish + 短暂 sleep 让消息送达后再 disconnect。

### 6.3 WiFi 断开未立即触发 WS 重连

WS 模式下，WiFi 断开后并不会立刻关闭 WS socket（TCP keep-alive 兜底，可能数十秒）。本次实现依赖 90s pong 超时来感知。若需更快感知，可在 `Application::HandleNetworkDisconnectedEvent` 里主动 `websocket_.reset()`。

### 6.4 UI 闪烁 "Connecting"

新 WS 长连下，每次唤醒仍走 Idle → Connecting → Listening 状态机，UI 短暂显示 "Connecting"。如需取消，可改 `Application::HandleToggleChatEvent` 在 WS 长连存活时直接进 Listening。Polish 项，不影响功能。

---

## 7. 协议字段一览（增量部分）

### 上行新增

| type | 子字段 | 触发点 | 说明 |
|---|---|---|---|
| `ping` | `timestamp` (ms since boot) | WS 模式每 60s | 应用层心跳 |

### 下行新增

| type | 子字段 | 处理 | 说明 |
|---|---|---|---|
| `pong` | `timestamp` (echo) | 协议层 | 重置 pong 超时计数 |

### MQTT broker 侧 LWT

| topic | payload | retain | 触发 |
|---|---|---|---|
| `/p2p/device_public/<mac_underscore>` | `{"type":"goodbye","device_id":"<mac:colon>"}` | false | 设备异常下线（keepalive 超时 / TCP 异常） |

### OTA 新增可配字段

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `websocket.ws_ping_interval` | int | 60 | WS 应用层 ping 间隔（秒）；0 关闭主动心跳 |
| `mqtt.keepalive` | int | 60 | 已有字段，默认值由 240 改 60 |
