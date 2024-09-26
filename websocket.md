
# AI 语音交互通信协议文档

## 1. 连接建立与鉴权

客户端通过 WebSocket 连接到服务器时，需要在 HTTP 头中包含以下信息：

- `Authorization`: Bearer token，格式为 "Bearer <access_token>"
- `Device-Id`: 设备 MAC 地址
- `Protocol-Version`: 协议版本号，当前为 2

WebSocket URL: `wss://api.tenclass.net/xiaozhi/v1`

## 2. 二进制数据

客户端发送的二进制数据使用固定头格式的协议，如下：

```cpp
struct BinaryProtocol {
    uint16_t version;        // 二进制协议版本，当前为 2
    uint16_t type;           // 消息类型（0：音频流数据，1：JSON）
    uint32_t reserved;       // 保留字段
    uint32_t timestamp;      // 时间戳（保留用作回声消除，也可以用于UDP不可靠传输中的排序）
    uint32_t payload_size;   // 负载大小
    uint8_t payload[];       // 可以是音频数据（Opus 编码或协商的音频格式），也可以封装 JSON
} __attribute__((packed));
```

注意：所有多字节整数字段使用网络字节序（大端序）。

目前二进制数据跟 JSON 都是走同一个 WebSocket 连接，未来实时对话模式下，二进制音频数据可能走 UDP，可以扩展 hello 消息进行协商。

## 3. 音频数据传输

- 客户端到服务器: 使用二进制协议发送 Opus 编码的音频数据
- 服务器到客户端: 使用二进制协议发送 Opus 编码的音频数据，格式与客户端发送的相同

出现 payload_size 为 0 的音频数据包可以用做句子边界标记，可以忽略，但不要报错。

## 4. 握手消息

连接建立后，客户端发送一个 JSON 格式的 "hello" 消息，初始化服务器端的音频解码器。
不需要等待服务器响应，随后即可发送音频数据。

```json
{
  "type": "hello",
  "response_mode": "auto",
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1
  }
}
```

应答模式 `response_mode` 可以为 `auto` 或 `manual`。

`auto`：自动应答模式，服务器实时计算音频 VAD 并自动决定何时开始应答。

`manual`：手动应答模式，客户端状态从 `listening` 变为 `idle` 时，服务器可以应答。

## 5. 状态更新

客户端在状态变化时发送 JSON 消息:

```json
{
  "type": "state",
  "state": "<新状态>"
}
```

可能发送的状态值包括: `idle`, `wake_word_detected`, `listening`, `speaking`。

示例:

1、按住说话（`response_mode` 为 `manual`）

- 当按住说话按钮时，如果未连接服务器，则连接服务器，并编码、缓存当前音频数据，连接成功后，客户端设置状态为 `listening`，并在 hello 消息之后发送缓存的音频数据。
- 当按住说话按钮时，如果已连接服务器，则客户端设置状态为 `listening`，并发送音频数据。
- 当释放说话按钮时，状态变为 `idle`，此时服务器开始识别。
- 服务器开始应答时，推送 `stt` 和 `tts` 消息。
- 客户端开始播放音频时，状态设为 `speaking`。
- 客户端结束播放音频时，状态设为 `idle`。
- 在 `speaking` 状态下，按住说话按钮，会立即停止当前音频播放，状态变为 `listening`。

2、语音唤醒，轮流对话（`response_mode` 为 `auto`）

- 连接服务器，发送 hello 消息，发送唤醒词音频数据，然后发送状态 `wake_word_detected`，服务器开始应答。
- 客户端开始播放音频时，状态设为 `speaking`，此时客户端不会发送音频数据。
- 客户端结束播放音频时，状态设为 `listening`，此时客户端发送音频数据。
- 服务器计算音频 VAD 自动选择时机开始应答时，推送 `stt` 和 `tts` 消息。
- 客户端收到 `tts`.`start` 时，开始播放音频，状态设为 `speaking`。
- 客户端收到 `tts`.`stop` 时，停止播放音频，状态设为 `listening`。

3、语音唤醒，实时对话（`response_mode` 为 `real_time`）

- 连接服务器，发送 hello 消息，发送唤醒词音频数据，然后发送状态 `wake_word_detected`，服务器开始应答。
- 客户端开始播放音频时，状态设为 `speaking`。
- 客户端结束播放音频时，状态设为 `listening`。
- 在 `speaking` 和 `listening` 状态下，客户端都会发送音频数据。
- 服务器计算音频 VAD 自动选择时机开始应答时，推送 `stt` 和 `tts` 消息。
- 客户端收到 `stt` 时，状态设为 `listening`。如果当前有音频正在播放，则在当前 sentence 结束后停止播放音频。
- 客户端收到 `tts`.`start` 时，开始播放音频，状态设为 `speaking`。
- 客户端收到 `tts`.`stop` 时，停止播放音频，状态设为 `listening`。

## 6. 服务器到客户端的消息

### 6.1 语音识别结果 (STT)

```json
{
  "type": "stt",
  "text": "<识别出的文本>"
}
```

### 6.2 文本转语音 (TTS)

TTS开始:
```json
{
  "type": "tts",
  "state": "start",
  "sample_rate": 24000
}
```

句子开始:
```json
{
  "type": "tts",
  "state": "sentence_start",
  "text": "你在干什么呀？"
}
```

句子结束:
```json
{
  "type": "tts",
  "state": "sentence_end"
}
```

TTS结束:
```json
{
  "type": "tts",
  "state": "stop"
}
```

## 7. 连接管理

- 客户端检测到 WebSocket 断开连接时，应该停止音频播放并重置为空闲状态
- 在断开连接后，客户端按需重新发起连接（比如按钮按下或语音唤醒）

这个文档概括了 WebSocket 通信协议的主要方面。
