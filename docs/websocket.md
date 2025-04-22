以下是一份基于代码实现整理的 WebSocket 通信协议文档，概述客户端（设备）与服务器之间如何通过 WebSocket 进行交互。该文档仅基于所提供的代码推断，实际部署时可能需要结合服务器端实现进行进一步确认或补充。

---

## 1. 总体流程概览

1. **设备端初始化**  
   - 设备上电、初始化 `Application`：  
     - 初始化音频编解码器、显示屏、LED 等  
     - 连接网络  
     - 创建并初始化实现 `Protocol` 接口的 WebSocket 协议实例（`WebsocketProtocol`）  
   - 进入主循环等待事件（音频输入、音频输出、调度任务等）。

2. **建立 WebSocket 连接**  
   - 当设备需要开始语音会话时（例如用户唤醒、手动按键触发等），调用 `OpenAudioChannel()`：  
     - 根据配置获取 WebSocket URL
     - 设置若干请求头（`Authorization`, `Protocol-Version`, `Device-Id`, `Client-Id`）  
     - 调用 `Connect()` 与服务器建立 WebSocket 连接  

3. **发送客户端 “hello” 消息**  
   - 连接成功后，设备会发送一条 JSON 消息，示例结构如下：  
   ```json
   {
     "type": "hello",
     "version": 1,
     "transport": "websocket",
     "audio_params": {
       "format": "opus",
       "sample_rate": 16000,
       "channels": 1,
       "frame_duration": 60
     }
   }
   ```
   - 其中 `"frame_duration"` 的值对应 `OPUS_FRAME_DURATION_MS`（例如 60ms）。

4. **服务器回复 “hello”**  
   - 设备等待服务器返回一条包含 `"type": "hello"` 的 JSON 消息，并检查 `"transport": "websocket"` 是否匹配。  
   - 如果匹配，则认为服务器已就绪，标记音频通道打开成功。  
   - 如果在超时时间（默认 10 秒）内未收到正确回复，认为连接失败并触发网络错误回调。

5. **后续消息交互**  
   - 设备端和服务器端之间可发送两种主要类型的数据：  
     1. **二进制音频数据**（Opus 编码）  
     2. **文本 JSON 消息**（用于传输聊天状态、TTS/STT 事件、IoT 命令等）  

   - 在代码里，接收回调主要分为：  
     - `OnData(...)`:  
       - 当 `binary` 为 `true` 时，认为是音频帧；设备会将其当作 Opus 数据进行解码。  
       - 当 `binary` 为 `false` 时，认为是 JSON 文本，需要在设备端用 cJSON 进行解析并做相应业务逻辑处理（见下文消息结构）。  

   - 当服务器或网络出现断连，回调 `OnDisconnected()` 被触发：  
     - 设备会调用 `on_audio_channel_closed_()`，并最终回到空闲状态。

6. **关闭 WebSocket 连接**  
   - 设备在需要结束语音会话时，会调用 `CloseAudioChannel()` 主动断开连接，并回到空闲状态。  
   - 或者如果服务器端主动断开，也会引发同样的回调流程。

---

## 2. 通用请求头

在建立 WebSocket 连接时，代码示例中设置了以下请求头：

- `Authorization`: 用于存放访问令牌，形如 `"Bearer <token>"`  
- `Protocol-Version`: 固定示例中为 `"1"`  
- `Device-Id`: 设备物理网卡 MAC 地址  
- `Client-Id`: 设备 UUID（可在应用中唯一标识设备）

这些头会随着 WebSocket 握手一起发送到服务器，服务器可根据需求进行校验、认证等。

---

## 3. JSON 消息结构

WebSocket 文本帧以 JSON 方式传输，以下为常见的 `"type"` 字段及其对应业务逻辑。若消息里包含未列出的字段，可能为可选或特定实现细节。

### 3.1 客户端→服务器

1. **Hello**  
   - 连接成功后，由客户端发送，告知服务器基本参数。  
   - 例：
     ```json
     {
       "type": "hello",
       "version": 1,
       "transport": "websocket",
       "audio_params": {
         "format": "opus",
         "sample_rate": 16000,
         "channels": 1,
         "frame_duration": 60
       }
     }
     ```

2. **Listen**  
   - 表示客户端开始或停止录音监听。  
   - 常见字段：  
     - `"session_id"`：会话标识  
     - `"type": "listen"`  
     - `"state"`：`"start"`, `"stop"`, `"detect"`（唤醒检测已触发）  
     - `"mode"`：`"auto"`, `"manual"` 或 `"realtime"`，表示识别模式。  
   - 例：开始监听  
     ```json
     {
       "session_id": "xxx",
       "type": "listen",
       "state": "start",
       "mode": "manual"
     }
     ```

3. **Abort**  
   - 终止当前说话（TTS 播放）或语音通道。  
   - 例：
     ```json
     {
       "session_id": "xxx",
       "type": "abort",
       "reason": "wake_word_detected"
     }
     ```
   - `reason` 值可为 `"wake_word_detected"` 或其他。

4. **Wake Word Detected**  
   - 用于客户端向服务器告知检测到唤醒词。  
   - 例：
     ```json
     {
       "session_id": "xxx",
       "type": "listen",
       "state": "detect",
       "text": "你好小明"
     }
     ```

5. **IoT**  
   - 发送当前设备的物联网相关信息：  
     - **Descriptors**（描述设备功能、属性等）  
     - **States**（设备状态的实时更新）  
   - 例：  
     ```json
     {
       "session_id": "xxx",
       "type": "iot",
       "descriptors": { ... }
     }
     ```
     或
     ```json
     {
       "session_id": "xxx",
       "type": "iot",
       "states": { ... }
     }
     ```

---

### 3.2 服务器→客户端

1. **Hello**  
   - 服务器端返回的握手确认消息。  
   - 必须包含 `"type": "hello"` 和 `"transport": "websocket"`。  
   - 可能会带有 `audio_params`，表示服务器期望的音频参数，或与客户端对齐的配置。  
   - 成功接收后客户端会设置事件标志，表示 WebSocket 通道就绪。

2. **STT**  
   - `{"type": "stt", "text": "..."}`
   - 表示服务器端识别到了用户语音。（例如语音转文本结果）  
   - 设备可能将此文本显示到屏幕上，后续再进入回答等流程。

3. **LLM**  
   - `{"type": "llm", "emotion": "happy", "text": "😀"}`
   - 服务器指示设备调整表情动画 / UI 表达。  

4. **TTS**  
   - `{"type": "tts", "state": "start"}`：服务器准备下发 TTS 音频，客户端进入 “speaking” 播放状态。  
   - `{"type": "tts", "state": "stop"}`：表示本次 TTS 结束。  
   - `{"type": "tts", "state": "sentence_start", "text": "..."}`
     - 让设备在界面上显示当前要播放或朗读的文本片段（例如用于显示给用户）。  

5. **IoT**  
   - `{"type": "iot", "commands": [ ... ]}`
   - 服务器向设备发送物联网的动作指令，设备解析并执行（如打开灯、设置温度等）。

6. **音频数据：二进制帧**  
   - 当服务器发送音频二进制帧（Opus 编码）时，客户端解码并播放。  
   - 若客户端正在处于 “listening” （录音）状态，收到的音频帧会被忽略或清空以防冲突。

---

## 4. 音频编解码

1. **客户端发送录音数据**  
   - 音频输入经过可能的回声消除、降噪或音量增益后，通过 Opus 编码打包为二进制帧发送给服务器。  
   - 如果客户端每次编码生成的二进制帧大小为 N 字节，则会通过 WebSocket 的 **binary** 消息发送这块数据。

2. **客户端播放收到的音频**  
   - 收到服务器的二进制帧时，同样认定是 Opus 数据。  
   - 设备端会进行解码，然后交由音频输出接口播放。  
   - 如果服务器的音频采样率与设备不一致，会在解码后再进行重采样。

---

## 5. 常见状态流转

以下简述设备端关键状态流转，与 WebSocket 消息对应：

1. **Idle** → **Connecting**  
   - 用户触发或唤醒后，设备调用 `OpenAudioChannel()` → 建立 WebSocket 连接 → 发送 `"type":"hello"`。  

2. **Connecting** → **Listening**  
   - 成功建立连接后，若继续执行 `SendStartListening(...)`，则进入录音状态。此时设备会持续编码麦克风数据并发送到服务器。  

3. **Listening** → **Speaking**  
   - 收到服务器 TTS Start 消息 (`{"type":"tts","state":"start"}`) → 停止录音并播放接收到的音频。  

4. **Speaking** → **Idle**  
   - 服务器 TTS Stop (`{"type":"tts","state":"stop"}`) → 音频播放结束。若未继续进入自动监听，则返回 Idle；如果配置了自动循环，则再度进入 Listening。  

5. **Listening** / **Speaking** → **Idle**（遇到异常或主动中断）  
   - 调用 `SendAbortSpeaking(...)` 或 `CloseAudioChannel()` → 中断会话 → 关闭 WebSocket → 状态回到 Idle。  

---

## 6. 错误处理

1. **连接失败**  
   - 如果 `Connect(url)` 返回失败或在等待服务器 “hello” 消息时超时，触发 `on_network_error_()` 回调。设备会提示“无法连接到服务”或类似错误信息。

2. **服务器断开**  
   - 如果 WebSocket 异常断开，回调 `OnDisconnected()`：  
     - 设备回调 `on_audio_channel_closed_()`  
     - 切换到 Idle 或其他重试逻辑。

---

## 7. 其它注意事项

1. **鉴权**  
   - 设备通过设置 `Authorization: Bearer <token>` 提供鉴权，服务器端需验证是否有效。  
   - 如果令牌过期或无效，服务器可拒绝握手或在后续断开。

2. **会话控制**  
   - 代码中部分消息包含 `session_id`，用于区分独立的对话或操作。服务端可根据需要对不同会话做分离处理，WebSocket 协议为空。

3. **音频负载**  
   - 代码里默认使用 Opus 格式，并设置 `sample_rate = 16000`，单声道。帧时长由 `OPUS_FRAME_DURATION_MS` 控制，一般为 60ms。可根据带宽或性能做适当调整。

4. **IoT 指令**  
   - `"type":"iot"` 的消息用户端代码对接 `thing_manager` 执行具体命令，因设备定制而不同。服务器端需确保下发格式与客户端保持一致。

5. **错误或异常 JSON**  
   - 当 JSON 中缺少必要字段，例如 `{"type": ...}`，客户端会记录错误日志（`ESP_LOGE(TAG, "Missing message type, data: %s", data);`），不会执行任何业务。

---

## 8. 消息示例

下面给出一个典型的双向消息示例（流程简化示意）：

1. **客户端 → 服务器**（握手）
   ```json
   {
     "type": "hello",
     "version": 1,
     "transport": "websocket",
     "audio_params": {
       "format": "opus",
       "sample_rate": 16000,
       "channels": 1,
       "frame_duration": 60
     }
   }
   ```

2. **服务器 → 客户端**（握手应答）
   ```json
   {
     "type": "hello",
     "transport": "websocket",
     "audio_params": {
       "sample_rate": 16000
     }
   }
   ```

3. **客户端 → 服务器**（开始监听）
   ```json
   {
     "session_id": "",
     "type": "listen",
     "state": "start",
     "mode": "auto"
   }
   ```
   同时客户端开始发送二进制帧（Opus 数据）。

4. **服务器 → 客户端**（ASR 结果）
   ```json
   {
     "type": "stt",
     "text": "用户说的话"
   }
   ```

5. **服务器 → 客户端**（TTS开始）
   ```json
   {
     "type": "tts",
     "state": "start"
   }
   ```
   接着服务器发送二进制音频帧给客户端播放。

6. **服务器 → 客户端**（TTS结束）
   ```json
   {
     "type": "tts",
     "state": "stop"
   }
   ```
   客户端停止播放音频，若无更多指令，则回到空闲状态。

---

## 9. 总结

本协议通过在 WebSocket 上层传输 JSON 文本与二进制音频帧，完成功能包括音频流上传、TTS 音频播放、语音识别与状态管理、IoT 指令下发等。其核心特征：

- **握手阶段**：发送 `"type":"hello"`，等待服务器返回。  
- **音频通道**：采用 Opus 编码的二进制帧双向传输语音流。  
- **JSON 消息**：使用 `"type"` 为核心字段标识不同业务逻辑，包括 TTS、STT、IoT、WakeWord 等。  
- **扩展性**：可根据实际需求在 JSON 消息中添加字段，或在 headers 里进行额外鉴权。

服务器与客户端需提前约定各类消息的字段含义、时序逻辑以及错误处理规则，方能保证通信顺畅。上述信息可作为基础文档，便于后续对接、开发或扩展。
