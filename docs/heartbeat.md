## 概览

xiaozhi-esp32 客户端没有应用层心跳协议（不存在 {"type":"ping"} 之类的 JSON 消息），保活完全依赖底层传输的协议层 ping：

协议: MQTT
心跳机制: MQTT 协议层 PINGREQ / PINGRESP（esp-mqtt 自动）
间隔: OTA 配的 keepalive，默认 240s（mqtt_protocol.cc:70 settings.GetInt("keepalive", 240)）
谁发: esp-mqtt 客户端库自动发
────────────────────────────────────────
协议: WebSocket
心跳机制: WS 控制帧 ping (opcode 0x9) / pong (0xA)
间隔: 由 server 主动 ping，客户端不主动 ping，仅自动回 pong
谁发: server

## MQTT

mqtt_protocol.cc:70-83：
int keepalive_interval = settings.GetInt("keepalive", 240);
...
mqtt_->SetKeepAlive(keepalive_interval);
最终落到 esp_mqtt.cc:36 mqtt_config.session.keepalive = keep_alive_seconds_，由 esp-mqtt 标准实现。客户端代码本身不做任何主动 ping 调度。

▎ ⚠️ mqtt_protocol.h:21 定义了 MQTT_PING_INTERVAL_SECONDS 90，但全代码无人引用 —— 是 dead code，别误导。

OTA 下发的 keepalive 字段（docs/ota-protocol_zh.md:158：「心跳秒数，未设置时使用代码默认 240」）可覆盖间隔。

## WebSocket

```c
web_socket.cc:253-255：
void WebSocket::Ping() {
    SendControlFrame(0x9, nullptr, 0);
}
```

这个 Ping() 方法在整个 main/ 目录下零调用（grep "Ping()" main/ 无结果）—— 也就是说客户端从不主动发 WS ping，只是被动响应：

```c
// web_socket.cc:389-391
case 0x9: // Ping
    SendControlFrame(0xA, payload.data(), payload_length);
    break;
```
收到 server ping 就回 pong，仅此而已。

客户端的「活性」判断（与心跳并行的另一个机制）

```c
protocol.cc:81-90：
bool Protocol::IsTimeout() const {
    const int kTimeoutSeconds = 120;
    auto duration = now - last_incoming_time_;
    return duration.count() > kTimeoutSeconds;
}
```

- 阈值硬编码 120 秒
- last_incoming_time_ 在 应用层数据到达时刷新（websocket_protocol.cc:165、mqtt_protocol.cc:131, 286）
- 走超时则 IsAudioChannelOpened() 返回 false → 通道被认定挂了

⚠️ WebSocket 场景下一个隐藏坑

WS 的 Ping/Pong 帧在 library 内部处理，不会触发上层 on_data_ 回调（web_socket.cc:376-378 只对 text/binary frame 调用 on_data_），所以Server 只发 WS-level
ping 是不能阻止客户端 120s 超时的。

实际维持 WS 通道存活的方式：
- Server 周期下发任何应用层数据（JSON 状态、音频包、自定义消息都行）
- 或者依赖会话本身有持续交互（TTS 下行、tts state 消息）

如果你的 WS server 设计成「长时间空闲只发 WS ping」，xiaozhi 客户端 120s 后会自动把通道判定为超时并关闭。Server 想做空闲心跳的话，发 JSON 文本帧而不是 WS
ping才有效。

## 文档参考

- docs/mqtt-udp_zh.md:69, 268 —— MQTT keep-alive 说明
- docs/ota-protocol_zh.md:126, 158 —— OTA 下发 keepalive 字段
- docs/websocket.md —— 没有任何心跳条款（这是文档缺口，server 侧实现建议时容易忽略上面那个坑）

## 一句话总结

▎ 没有应用层心跳消息。MQTT 靠 library 自动发 PINGREQ（默认 240s，OTA 可配），WS 客户端只回 pong 不主动 ping —— WS 维持长连的责任在 server，且必须发应用层
▎ JSON 而非 WS ping 帧才能避开 120s 空闲超时。