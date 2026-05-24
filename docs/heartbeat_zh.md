# 心跳与长连接（Heartbeat & Keep-Alive）

> 在原协议基础上引入的小幅扩展，目标是**支持服务器主动下发通知**。不破坏现有协议字段，老固件可继续接入（仅丢失"在线感知"与"被动 push"能力）。

适用代码：
- `main/protocols/websocket_protocol.{h,cc}` — WS 长连接 + 应用层 ping/pong + 重连退避
- `main/protocols/mqtt_protocol.cc` — MQTT keepalive 调整 + Last Will + Birth message
- `managed_components/78__esp-ml307/include/mqtt.h` + 各实现 — 抽象类新增 `SetWill` / `Publish(... retain)`

---

## 1. 设计目标

| 需求 | 实现手段 |
|---|---|
| 设备在线时随时可接收服务器下发 | WS 改为「激活后即连」长期保持；MQTT 保持 broker 长连 |
| 服务器能及时感知设备离线 | WS: 应用层 ping/pong 超时；MQTT: 协议层 keepalive + Last Will |
| 设备能从异常断开恢复 | 指数退避自动重连 |
| 老固件接入仍可工作 | hello 里加 `features.heartbeat`；服务端按此 flag 区分新老设备 |

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
        (服务端可直接 push tts/alert/system/mcp 等)
```

### 2.2 应用层心跳

**协议**：

```jsonc
// Client → Server, 每 30 秒
{ "type": "ping", "ts": <millis_since_boot> }

// Server → Client, 应当尽快回
{ "type": "pong", "ts": <client_ts_echo_or_self> }
```

**参数**（`websocket_protocol.h`）：

| 常量 | 默认值 | 含义 |
|---|---|---|
| `WEBSOCKET_PING_INTERVAL_MS` | 30000 | ping 周期 |
| `WEBSOCKET_PONG_TIMEOUT_MS`  | 90000 | 3 个 ping 没收到 pong → 视为掉线 |

**判定逻辑**：每次 ping 触发时，检查 `now - last_pong_time_us > 90s` → 主动 `websocket_.reset()` → 走 `HandleDisconnected` → 安排重连。

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

也就是说：**WS 长连接是否存活**与 **audio session 是否活动**现在是两个独立概念，前者无外部 query 接口（内部使用）。

`Application::CanEnterSleepMode()` 仍依赖 `IsAudioChannelOpened`：现在它返回 false 只代表"没在通话"，**进入 sleep 会切断 WS 长连接**——这是 power-saving 场景的固有取舍，未做修改。

---

## 3. MQTT 长连接

### 3.1 Keepalive

| 项 | 旧值 | 新值 |
|---|---|---|
| `Settings("mqtt").GetInt("keepalive", default)` 默认 | 240 | **60** |

**注意**：OTA 下发的 `mqtt.keepalive` 仍然优先；改默认值只影响"OTA 未指定 keepalive 时"的回退。生产部署建议**让 OTA server 也下发 60s**，避免不同设备行为不一致。

MQTT 协议层 PINGREQ/PINGRESP 由 `esp-mqtt` 自动处理，业务无需关心。

### 3.2 Last Will

**设置时机**：`MqttProtocol::StartMqttClient` 中，在调用 `mqtt_->Connect()` 之前 `SetWill(...)`。

```
Topic:   device/{client_id}/status
Payload: {"status":"offline","reason":"will","ts":<connect_ts>}
QoS:     1
Retain:  true
```

**触发条件**（broker 行为）：

| 场景 | 是否触发 will |
|---|---|
| 设备 keepalive 超时（60s 没 PINGREQ） | ✓ |
| TCP 异常断开（断电/拔网线） | ✓ |
| 设备主动 DISCONNECT packet | ✗ |
| 设备发送 graceful disconnect 后掉线 | ✗ |

**重要**：设备主动 reset / OTA reboot 时不会发 will（走 DISCONNECT 流程）。这种"主动下线"由 server 业务自行处理（建议设备 reboot 前发一条 `{"status":"offline","reason":"reboot"}` 到同 topic，本次改动**未实现**这条）。

### 3.3 Birth message

`MqttProtocol::OnConnected` 回调中，连接成功后立刻 publish：

```
Topic:   device/{client_id}/status
Payload: {"status":"online","ts":<now>,"version":"<fw_version>"}
QoS:     1
Retain:  true
```

`retain=true` 用于覆盖前一次的 will（同 topic 同 retain）。后续订阅 `device/+/status` 的服务端可立即读到设备真实状态。

### 3.4 服务端订阅约定

```
device/+/status   ← 通配订阅, 监听所有设备的上下线
```

设备 client_id 由 OTA 阶段下发的 `mqtt.client_id` 决定（写在 NVS `mqtt` 命名空间）。

---

## 4. 区分新老设备

设备 hello 消息里加了 feature flag：

```jsonc
"features": {
  "mcp": true,
  "heartbeat": true   // ← 新增, 老固件不带
}
```

**服务端建议逻辑**：

```
session.has_heartbeat = hello.features.heartbeat == true

if session.has_heartbeat:
    # 新设备
    WS:   监控 ping 周期, 90s 没 ping 即判离线 (close socket)
    MQTT: 信任 broker 的在线状态; will/birth 消息驱动 status
else:
    # 老设备
    WS:   audio channel 关闭即视为离线 (老固件不维持长连接)
    MQTT: keepalive 240s, will 不会有 (老固件没设置)
          broker 60s 超时不准, 只能监控 audio session
```

---

## 5. 测试验证

### 5.1 MQTT 模式

| 验证项 | 操作 | 期望 |
|---|---|---|
| Birth 覆盖 will | 设备上电连接 | broker 上 `device/{id}/status` 是 `online` |
| 异常 will | 拔网线 / 断电 | 60s 内 broker 推 `offline,reason=will` 到 `device/{id}/status` |
| 主动断不发 will | 调用 `protocol_.reset()` (e.g. 切 AEC 模式) | broker 不推任何 status 消息（待改进：主动发 `offline,reason=client`） |
| Status retain 生效 | 后启动的订阅者订阅 `device/+/status` | 立刻收到对应设备最近一次的 retained 消息 |

### 5.2 WebSocket 模式

| 验证项 | 操作 | 期望 |
|---|---|---|
| 长连接保持 | 激活完成后 idle 1 分钟 | 日志看 ping 每 30s 发送 |
| Pong 超时重连 | server 停止回 pong | 90s 后日志报 `Pong timeout`, 然后 1s/2s/... 退避重连 |
| Server push 可达 | server idle 状态推 `{"type":"alert",...}` | 设备屏幕显示 alert（无需先唤醒） |
| 断网恢复 | 拔 WiFi 30s 再插回 | 重连退避中, 网络恢复后下一次 retry 成功 |
| Audio session 复用 | 第二次唤醒 | UI 仅闪烁极短 Connecting (因为 OpenAudioChannel 不真的 connect)，TTS 首包延迟显著下降 |

### 5.3 老设备验证

| 验证项 | 期望 |
|---|---|
| 旧固件接入 server | hello 里无 `features.heartbeat`, server 按老逻辑处理 |
| 旧固件 audio channel 关闭 | server 收到 WS close, 视为离线 |

---

## 6. 已知问题与待办

### 6.1 esp-ml307 依赖修改未持久化

`managed_components/78__esp-ml307/` 是 IDF Component Manager 从 registry 拉取的依赖，本次改动直接修改了：
- `include/mqtt.h`（加 `SetWill` + `Publish(..., retain)`）
- `src/esp/esp_mqtt.cc`（实现 will 配置）
- `src/{ml307,ec801e}/{*.h,*.cc}`（同步扩 `Publish` 签名）

**问题**：执行 `idf.py reconfigure` 或在新机器克隆后，managed_components 会被重新拉取，本地改动丢失。

**推荐的持久化方案**（任选一）：

1. **fork 到 `components/esp-ml307/`**：把整个 `managed_components/78__esp-ml307` 复制到仓库根目录 `components/` 下，并从 `main/idf_component.yml` 移除 `78/esp-ml307: ~3.6.5` 依赖。components/ 目录会优先于 managed_components 被 IDF 使用。
2. **向上游 esp-ml307 提 PR**：让 78 合并 SetWill / Publish retain 这两个改动。
3. **维护 patch 文件**：脚本化在每次 reconfigure 后自动应用。

当前未做选择，建议 **量产前必须解决**。

### 6.2 设备主动 reboot/reset 时未发 offline

如 §3.2 所述，设备 graceful disconnect 时 broker 不会发 will，但当前代码也没在 reboot/destructor 中主动 publish `offline` 消息。结果：服务端在设备 reboot 期间会以为它还在线（retain 仍是 `online`），直到下次 birth 消息覆盖。

如需精确状态，可在 `Application::Reboot` 之前调 MQTT publish offline，或在 `MqttProtocol::~MqttProtocol` 中加 publish + 短暂 sleep 让消息送达后再 disconnect。

### 6.3 WiFi 断开未立即触发 WS 重连

WS 模式下，WiFi 断开后并不会立刻关闭 WS socket（TCP keep-alive 兜底，可能数十秒）。本次实现依赖 90s pong 超时来感知。若需更快感知，可在 `Application::HandleNetworkDisconnectedEvent` 里主动 `websocket_.reset()`。当前未做，避免过度耦合。

### 6.4 UI 闪烁 "Connecting"

新 WS 长连下，每次唤醒仍走 Idle → Connecting → Listening 状态机，UI 短暂显示 "Connecting"。如需取消，可改 `Application::HandleToggleChatEvent` 在 WS 长连存活时直接进 Listening。这是 polish 项，不影响功能。

---

## 7. 协议字段一览（增量部分）

### 上行新增

| type | 子字段 | 触发点 | 说明 |
|---|---|---|---|
| `ping` | `ts` (ms since boot) | WS 模式每 30s | 应用层心跳 |

`hello.features.heartbeat: true` 新增 boolean 字段。

### 下行新增

| type | 子字段 | 处理 | 说明 |
|---|---|---|---|
| `pong` | `ts` (任意) | 协议层 | 重置 pong 超时计数 |

### MQTT broker 侧新增

| topic | payload | retain | 说明 |
|---|---|---|---|
| `device/{client_id}/status` | `{"status":"online", ...}` | true | 设备上线 (birth) |
| `device/{client_id}/status` | `{"status":"offline","reason":"will",...}` | true | 设备异常下线 (will) |
