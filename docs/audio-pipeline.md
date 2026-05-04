# 音频处理链路分析（xiaozhi-esp32）

本文档基于当前仓库代码（`v226` 分支，配置 `bread-compact-wifi` + `NoAudioCodecSimplex`）梳理音频从硬件 I2S → 网络 → 扬声器的完整链路。涉及的源文件：

- `main/application.cc`、`main/application.h`
- `main/audio/audio_service.{h,cc}`
- `main/audio/audio_codec.{h,cc}`
- `main/audio/codecs/no_audio_codec.{h,cc}`
- `main/audio/processors/afe_audio_processor.{h,cc}` / `no_audio_processor.*`
- `main/audio/wake_words/{afe,custom,esp}_wake_word.*`
- `main/audio/demuxer/ogg_demuxer.*`
- `main/protocols/protocol.{h,cc}`、`websocket_protocol.cc`、`mqtt_protocol.cc`
- `main/boards/bread-compact-wifi/{compact_wifi_board.cc,config.h}`

---

## 1. 总览

整体分为**三条独立的 FreeRTOS 任务**，通过 5 条带锁队列串联：

| 任务 | 优先级/Core | 职责 |
|---|---|---|
| `audio_input` | prio 8（pin Core 0 当 AFE 启用） | 从 I2S 读 PCM，喂给唤醒词引擎和 AFE 处理器 |
| `audio_output` | prio 4 | 从 `audio_playback_queue_` 取 PCM，写 I2S |
| `opus_codec` | prio 2 | 兼任 Opus 编/解码，连接 PCM ↔ 网络包 |
| `audio_communication` | prio 3 | （AFE 内部）跑 AEC/NS/VAD，输出干净 PCM |

队列：

| 队列 | 元素 | 容量 | 生产者 → 消费者 |
|---|---|---|---|
| `audio_encode_queue_` | `AudioTask`(PCM) | 2 | AFE/直通 → opus_codec |
| `audio_send_queue_` | `AudioStreamPacket`(Opus) | 40 | opus_codec → Application（→ 网络） |
| `audio_decode_queue_` | `AudioStreamPacket`(Opus) | 40 | 网络 → opus_codec |
| `audio_playback_queue_` | `AudioTask`(PCM) | 2 | opus_codec → audio_output |
| `audio_testing_queue_` | `AudioStreamPacket` | 167 | AFE → 回环测试 |

帧参数（`audio_service.h`）：
- `OPUS_FRAME_DURATION_MS = 60`
- 编码端固定 16 kHz / mono / int16
- 解码端跟随 `packet->sample_rate`（首包按 codec 输出采样率开 decoder）

---

## 2. 硬件抽象层（HAL）

### 2.1 `AudioCodec`（基类，`audio/audio_codec.h`）

负责 I2S 通道、采样率、音量、输入/输出使能状态。子类必须实现：

```cpp
virtual int Read(int16_t* dest, int samples) = 0;
virtual int Write(const int16_t* data, int samples) = 0;
```

字段 `tx_handle_` / `rx_handle_` 是 `i2s_chan_handle_t`，由子类构造时分配。`Start()` 从 NVS `Settings("audio")` 读 `output_volume`（默认 70，最低钳到 10）。

### 2.2 `NoAudioCodecSimplex`（当前 board 使用）

`bread-compact-wifi` 在 `compact_wifi_board.cc:174` 创建：

```cpp
static NoAudioCodecSimplex audio_codec(
    AUDIO_INPUT_SAMPLE_RATE,   // 16000
    AUDIO_OUTPUT_SAMPLE_RATE,  // 24000
    AUDIO_I2S_SPK_GPIO_BCLK,   // GPIO15
    AUDIO_I2S_SPK_GPIO_LRCK,   // GPIO16
    AUDIO_I2S_SPK_GPIO_DOUT,   // GPIO7  → MAX98357 等放大器
    AUDIO_I2S_MIC_GPIO_SCK,    // GPIO5
    AUDIO_I2S_MIC_GPIO_WS,     // GPIO4
    AUDIO_I2S_MIC_GPIO_DIN);   // GPIO6  ← INMP441 等数字 MIC
```

含义：**simplex（双口）** —— TX/RX 各占一个 I2S 控制器，互不干扰。`duplex_=false`，`input_reference_=false`（无回采参考通道）。

`Read()`：以 32-bit 读入 → `value >> 12` 截到 16-bit（适配 24-bit MEMS 麦克风左对齐数据）→ 200 ms 超时。
`Write()`：把 16-bit 输入按 `output_volume_` 的二次方（gamma 平方）做软件音量映射到 32-bit，再 `i2s_channel_write`。
`EnableInput/Output()`：调用 `i2s_channel_enable/disable`，受 `data_if_mutex_` 保护。

### 2.3 其他 codec（仅作对比，未在当前 board 启用）

`box_*`、`es8311_*`、`es8374_*`、`es8388_*`、`es8389_*` 走 I2C 配置 codec 芯片寄存器；`dummy_*` 仅占位。

---

## 3. AudioService 初始化

`Application::Initialize()`（`application.cc:73`）：

```cpp
auto codec = board.GetAudioCodec();  // 获取 NoAudioCodecSimplex
audio_service_.Initialize(codec);
audio_service_.Start();
```

`AudioService::Initialize()`（`audio_service.cc:62`）：

1. `codec_->Start()` 加载音量。
2. **创建 Opus 解码器**：`esp_opus_dec_open(sample_rate=codec->output_sample_rate()=24000, dur=60ms)` → `decoder_frame_size_=1440`。
3. **创建 Opus 编码器**：固定 16 kHz / mono / VBR / DTX / complexity=0 / 60 ms / `APPLICATION_AUDIO`，比特率自动。
4. 如果 `codec->input_sample_rate() != 16000`，开 `input_resampler_`（`esp_ae_rate_cvt`）。当前 board 输入就是 16000，所以不创建。
5. **选择处理器**：
   - `CONFIG_USE_AUDIO_PROCESSOR=y` → `AfeAudioProcessor`（依赖 ESP-ADF 的 `esp_afe_sr_iface_t`）
   - 否则 → `NoAudioProcessor`（直通）
6. 注册回调：处理器输出 → `PushTaskToEncodeQueue(EncodeToSendQueue, ...)`；VAD 状态 → `voice_detected_` + `callbacks_.on_vad_change`。
7. 创建 1 Hz/1 s 的 `audio_power_timer_`（`AUDIO_POWER_TIMEOUT_MS=15000` 后自动关 I2S 通道）。

`SetModelsList()`（`audio_service.cc:700`）按 ESP32-S3/P4 与模型存在与否选择唤醒词实现：

| 模型 | 实现 |
|---|---|
| 含 `ESP_MN_PREFIX`（multi-net 命令词） | `CustomWakeWord`（S3/P4） |
| 含 `ESP_WN_PREFIX`（WakeNet） | `AfeWakeWord`（S3/P4）/ `EspWakeWord`（其它芯片） |
| 都没有 | `wake_word_ = nullptr`（无唤醒词） |

---

## 4. 上行链路（MIC → 网络）

### 4.1 整体数据流

```
 (MIC)
   │  I2S 32-bit @ 16 kHz
   ▼
NoAudioCodecSimplex::Read   ─┐
   │  int16 PCM             │ AudioInputTask（每 10 ms 一次，160 samples）
   ▼                        │
ReadAudioData               │ ── (重采样 if 需要) ──> 喂入：
   │ ┌──────── wake_word_->Feed(data)             [仅 WakeWord 模式]
   │ └──────── audio_processor_->Feed(data)       [Listening 模式]
   ▼
AfeAudioProcessor::Feed
   │  累积到 chunk_size，调 afe_iface_->feed()
   ▼
AudioProcessorTask（"audio_communication"）
   │  afe_iface_->fetch_with_delay()              ← AEC + NS + VAD
   │  attach VAD state（speaking？）
   │  按 60 ms = 960 samples 切帧，触发 output_callback_
   ▼
AudioService::PushTaskToEncodeQueue
   │  写入 audio_encode_queue_（满了会阻塞 wait）
   │  附带 server-AEC timestamp（出队 timestamp_queue_）
   ▼
OpusCodecTask
   │  esp_opus_enc_process(960 samples → Opus payload)
   │  封装 AudioStreamPacket{16k, 60ms, ts, opus}
   ▼
audio_send_queue_
   │  callbacks_.on_send_queue_available → 主循环 set MAIN_EVENT_SEND_AUDIO
   ▼
Application::Run() 主循环
   │  while (PopPacketFromSendQueue())
   ▼
protocol_->SendAudio(packet)            ─→ WebSocket / MQTT-UDP → 云端
```

### 4.2 关键细节

- **采样率契约**：上行始终 16 kHz / mono / int16，与 codec 输入采样率不一致时由 `input_resampler_` 转换。
- **chunk 协议**：AFE 的 `get_feed_chunksize()` 通常是 256 samples（16 ms），代码用 `input_buffer_` 滚动缓冲拼接。
- **帧聚合**：AFE 输出可能不是整 60 ms，`output_buffer_` 在 `AudioProcessorTask` 中累积到 `frame_samples_=960` 才回调。
- **背压**：`audio_encode_queue_` 容量 2，满了时 `Feed` 一侧通过 `audio_queue_cv_.wait()` 阻塞，自然反压到 AFE。
- **VAD**：`AfeAudioProcessor` 监测 `res->vad_state` 边沿，`speaking` 边沿触发 `on_vad_change` → 主循环更新 LED。
- **唤醒词分支**：`EnableWakeWordDetection(true)` 时，`AudioInputTask` 同时把 PCM `Feed` 给 `wake_word_`；唤醒后 `on_wake_word_detected` → 主循环切到 `Listening` 状态，进而 `EnableVoiceProcessing(true)`。
- **唤醒词回放**：`wake_word_->EncodeWakeWordData()` 把唤醒前后的 PCM 编 Opus，`PopWakeWordPacket()` 通过 `protocol_->SendAudio()` 发给服务端，让云端二次校验唤醒。

### 4.3 模式切换（`audio_service.cc:549/579`）

| 模式 | EventGroup bit | 行为 |
|---|---|---|
| WakeWord | `AS_EVENT_WAKE_WORD_RUNNING` | 仅唤醒词识别 |
| Listening (AFE) | `AS_EVENT_AUDIO_PROCESSOR_RUNNING` | AEC/NS/VAD + 上传 |
| Audio Testing | `AS_EVENT_AUDIO_TESTING_RUNNING` | 录最多 10 s 进 `audio_testing_queue_`，关闭时整体 move 到 `audio_decode_queue_` 自播 |

切换时会 `esp_ae_rate_cvt_reset(input_resampler_)` 防止上一模式残留 chunk 引发 buffer overflow。Listening 进入时 `audio_input_need_warmup_=true`，输入任务先 `vTaskDelay(120 ms)` 跳过开声门 pop。

---

## 5. 下行链路（网络 → 扬声器）

### 5.1 整体数据流

```
                         (云端)
                            │  WebSocket binary frame / MQTT-UDP
                            ▼
   websocket_protocol.cc / mqtt_protocol.cc
   on_incoming_audio_(packet)               ← BinaryProtocol2/3 解包
                            ▼
   Application::OnIncomingAudio (application.cc:498)
   if (state == Speaking) audio_service_.PushPacketToDecodeQueue(packet)
                            ▼
                  audio_decode_queue_
                            ▼
            OpusCodecTask
   ┌────────────────────────────────────┐
   │ SetDecodeSampleRate(packet rate)   │ 首次/采样率变化时重开 decoder
   │ esp_opus_dec_decode → PCM          │
   │ if dec_sr != codec_out_sr:         │
   │   esp_ae_rate_cvt_process          │ 24k→codec_out_sample_rate
   └────────────────────────────────────┘
                            ▼
                audio_playback_queue_  (容量 2)
                            ▼
            AudioOutputTask
                            ▼
   NoAudioCodecSimplex::Write
                            │  音量 gamma² × PCM → 32-bit
                            │  i2s_channel_write
                            ▼
                       (Speaker)
```

### 5.2 关键细节

- **采样率自适应**：每个 `AudioStreamPacket` 自带 `sample_rate` 和 `frame_duration`。`SetDecodeSampleRate()`（`audio_service.cc:448`）发现变化就关掉 decoder 重开，并按需重建 `output_resampler_`。日志：`Resampling audio from %d to %d`。
- **解码音量控制**：解码出 `int16 PCM`，**不**在解码侧做音量；音量在 `NoAudioCodec::Write()` 内乘 `pow(volume/100, 2)*65536`，可线性切换响度。
- **背压**：`audio_playback_queue_` 容量 2 限制堆积；满了时 OpusCodecTask 会停下让 AudioOutputTask 消费。
- **server-AEC**：若 `task->timestamp > 0`（云端给出原始时戳），输出后 `timestamp_queue_.push_back()`。下一次上行编码 `PushTaskToEncodeQueue(SendQueue, ...)` 出队 timestamp 写到 `packet->timestamp`，让服务端做远端回声对齐。
- **本地 AEC（设备侧）**：`CONFIG_USE_DEVICE_AEC=y` 时，AFE 启用 `aec_init=true` + `vad_init=false`，处理器在 `EnableDeviceAec(true)` 切换 `disable_vad → enable_aec`。

### 5.3 ResetDecoder

被 `EnableVoiceProcessing(true)` 和"Listening 状态切回"调用：

```cpp
ResetDecoder()  // audio_service.cc:668
  -> esp_opus_dec_reset(opus_decoder_)
  -> timestamp_queue_, audio_decode_queue_, audio_playback_queue_, audio_testing_queue_ 全清
```

避免说话开始时还有残余 TTS 在播。

---

## 6. 本地音效播放（PlaySound）

`AudioService::PlaySound(ogg)`（`audio_service.cc:633`）用于本地 OGG 音效（提示音、错误音）：

```
flash 内 OGG bytes
   ▼
OggDemuxer::Process
   ▼
OnDemuxerFinished → AudioStreamPacket{sample_rate, 60ms, opus_payload}
   ▼
PushPacketToDecodeQueue(wait=true)        ← 复用下行解码链路
```

调用点：`Lang::Sounds::OGG_SUCCESS` / `OGG_POPUP` / `OGG_ERR_*` / `OGG_EXCLAMATION`。

---

## 7. 电源管理

`audio_power_timer_`（每 1 s 触发 `CheckAndUpdateAudioPowerState`，`audio_service.cc:682`）：

- 输入空闲 > 15 s → `codec_->EnableInput(false)`（关 RX I2S）
- 输出空闲 > 15 s → `codec_->EnableOutput(false)`，但 **duplex 且 RX 在用时不关 TX**（部分板子停 TX 时钟会让 RX 也卡住）
- 两路都关后停掉定时器

`ReadAudioData` / `AudioOutputTask` 在写入数据前若发现通道未启用，会按 1 s 间隔重启定时器并 `EnableInput/Output(true)`。

---

## 8. 当前 board（bread-compact-wifi）参数固化

| 参数 | 值 | 来源 |
|---|---|---|
| Codec | `NoAudioCodecSimplex` | `compact_wifi_board.cc:174` |
| 输入采样率 | 16000 Hz | `config.h:6` |
| 输出采样率 | 24000 Hz | `config.h:7` |
| 输入通道 | 1 | I2S_STD_SLOT_LEFT 默认 |
| 输入 reference | 否（无回采） | NoAudioCodec 默认 |
| 处理器 | `AfeAudioProcessor`（如果 `CONFIG_USE_AUDIO_PROCESSOR=y`） | `audio_service.cc:25` |
| 唤醒词 | `AfeWakeWord` 或 `CustomWakeWord`（S3） | `audio_service.cc:700` |
| Opus 帧长 | 60 ms | `audio_service.h:39` |
| Opus 上行码率 | VBR auto | `AS_OPUS_ENC_CONFIG` |
| 服务端默认采样率 | 24000 Hz / 60 ms | `protocol.h:86` |

> 注：当前 `compact_wifi_board.cc` 已删掉 SSD1306 显示屏初始化，`Display = NoDisplay`，对音频链路无任何影响。

---

## 9. 关键时序示例（一次完整对话）

```
[Idle]
  └─ EnableWakeWordDetection(true)
     └─ AudioInputTask 持续 wake_word_->Feed
[Wake]
  └─ on_wake_word_detected → MAIN_EVENT_WAKE_WORD_DETECTED
     └─ Schedule: protocol_->OpenAudioChannel
        └─ on_audio_channel_opened → SetDeviceState(Listening)
[Listening]
  └─ EnableWakeWordDetection(false) + EnableVoiceProcessing(true)
     ├─ ResetDecoder（清残留 TTS）
     ├─ audio_input_need_warmup_=true（120ms 静默）
     └─ AudioInputTask 切到 audio_processor_->Feed
        └─ AFE → encode_queue → opus_codec → send_queue
           └─ MAIN_EVENT_SEND_AUDIO → protocol_->SendAudio (websocket binary)
[Speaking]
  └─ 服务端推 Opus → on_incoming_audio → decode_queue
     └─ opus_codec → playback_queue → AudioOutputTask → I2S → 喇叭
[Idle]
  └─ ResetDecoder + EnableVoiceProcessing(false) + EnableWakeWordDetection(true)
```

---

## 10. 可能的扩展点

1. **更换 codec 芯片**：实现新的 `AudioCodec` 子类（参考 `es8311_audio_codec.cc`），重写 `Read/Write/EnableInput/EnableOutput`，在 board 的 `GetAudioCodec()` 返回。
2. **关闭 AFE 处理**：`menuconfig` 关掉 `USE_AUDIO_PROCESSOR` → 走 `NoAudioProcessor` 直通，CPU/PSRAM 占用骤降，但失去 AEC/NS。
3. **上行码率/帧长**：改 `AS_OPUS_ENC_CONFIG()` 的 `frame_duration` / `bitrate`；注意服务端是否兼容。
4. **音频调试**：开 `CONFIG_USE_AUDIO_DEBUGGER`，`AudioDebugger` 会把原始 PCM 通过 UDP 发出，便于在 PC 端 Audacity 分析。
5. **设备侧 AEC**：开 `CONFIG_USE_DEVICE_AEC` + `input_reference=true` 的 codec（如 ES7210 + ES8311），避免依赖 server AEC。
