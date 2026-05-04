# 语音监听模式与 Power VAD 设计文档

本文档分两部分：
1. **现状梳理**：当前固件 `manual` / `auto` / `realtime` 三种监听模式的实现与执行流程；
2. **优化设想**：向设备端引入 Power VAD 作为粗筛的设计与改造方案，用以解决 realtime 模式下静音也持续上行带来的带宽与功耗问题。

涉及源码：

- `main/protocols/protocol.{h,cc}`
- `main/application.{h,cc}`
- `main/audio/audio_service.{h,cc}`
- `main/audio/processors/afe_audio_processor.{h,cc}`

---

## 第一部分：当前三种监听模式的实现

### 1.1 模式定义

```cpp
// main/protocols/protocol.h:38
enum ListeningMode {
    kListeningModeAutoStop,    // "auto"
    kListeningModeManualStop,  // "manual"
    kListeningModeRealtime     // "realtime"  注释：需要 AEC 支持
};
```

向服务端同步通过 `Protocol::SendStartListening`（`protocol.cc:57`）：
```json
{"type":"listen","state":"start","mode":"auto" | "manual" | "realtime"}
```

### 1.2 模式选择逻辑

```cpp
// application.cc:947
ListeningMode Application::GetDefaultListeningMode() const {
    return aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime;
}
```

`aec_mode_` 在编译期由 Kconfig 决定（`application.cc:26`）：

| 配置 | aec_mode_ | 默认监听模式 |
|---|---|---|
| `CONFIG_USE_DEVICE_AEC=y` | `kAecOnDeviceSide` | `Realtime` |
| `CONFIG_USE_SERVER_AEC=y` | `kAecOnServerSide` | `Realtime` |
| 都未启用（默认） | `kAecOff` | `AutoStop` |

进入 `Manual` 的唯一入口是 `StartListening`（按住 PTT 按钮），强制设为 `kListeningModeManualStop`（`application.cc:750/754/757`）。运行时 `SetAecMode()` 也可切换 AEC 配置，会顺带关闭并重连 audio channel。

### 1.3 三种模式的核心差异

#### A. Manual（按住说话）

- **触发**：按键按下（`touch_button_.OnPressDown` → `StartListening`）。
- **进入 Listening**：`SendStartListening("manual")` + `EnableVoiceProcessing(true)`，AFE 上行。
- **退出**：按键松开 → `MAIN_EVENT_STOP_LISTENING` → `protocol_->SendStopListening()` → **直接回 Idle**。
- **TTS 完成**：`tts.stop` 时由于 `listening_mode_ == kListeningModeManualStop`，**SetDeviceState(Idle)**（`application.cc:534`）—— 一句一答，不自动续听。
- **拾音控制权**：完全在设备端（用户手指）。
- **不依赖 AEC**：用户不会在自己说话时听 TTS。

#### B. AutoStop（VAD 自动停止）

- **触发**：唤醒词或 `ToggleChat`，且 `aec_mode_==kAecOff`（默认配置）。
- **进入 Listening**：`SendStartListening("auto")` + `EnableVoiceProcessing(true)`。
- **端点判定**：服务端通过 ASR/VAD 端点决定何时停 listen，主动下发 `tts.start`。
- **TTS 期间**：进入 Speaking 状态时 `EnableVoiceProcessing(false)`，**关闭拾音上行**，仅保留 AFE 唤醒词识别（`application.cc:909`）。
- **TTS 完成**：`tts.stop` 时回到 `Listening`，进入前调用 `WaitForPlaybackQueueEmpty()`（`application.cc:883`）阻塞等待 playback 队列清空，**避免上一句末尾被截断**。
- **打断方式**：必须喊唤醒词（仅 AFE 唤醒词支持在 Speaking 时识别）或按键。
- **不依赖 AEC**：拾音和播放在时间上完全错开。

#### C. Realtime（全双工实时）

- **触发**：唤醒词或 `ToggleChat`，且 `aec_mode_` 为 `kAecOnDeviceSide` 或 `kAecOnServerSide`。
- **进入 Listening**：`SendStartListening("realtime")` + `EnableVoiceProcessing(true)`。
- **TTS 期间**：进入 Speaking 状态时 **`EnableVoiceProcessing` 保持 true**（`application.cc:909` 的 `if (mode != Realtime)` 跳过关闭），**唤醒词检测也不重启**，**拾音常开**。
- **打断**：用户随时说话即可打断 TTS（barge-in）。Server 拿到上行流，做 VAD 决策后下发 abort。
- **TTS 完成**：直接回到 `Listening`，链路从未中断。
- **强依赖 AEC**：
  - **device-side AEC**（`AfeAudioProcessor::Initialize` 中 `aec_init=true, vad_init=false`，`afe_audio_processor.cc:59`）：AFE 把麦克风混入的扬声器回声消掉，输出干净人声；
  - **server-side AEC**：device 上行原始混音，timestamp 字段携带最近一次播放包的时戳，云端按时戳对齐做远端 AEC。

### 1.4 关键状态机片段

#### `HandleStateChangedEvent` 进入 Speaking（`application.cc:906`）
```cpp
case kDeviceStateSpeaking:
    if (listening_mode_ != kListeningModeRealtime) {  // 关键判断
        audio_service_.EnableVoiceProcessing(false);
        audio_service_.EnableWakeWordDetection(audio_service_.IsAfeWakeWord());
    }
    audio_service_.ResetDecoder();
    break;
```

#### TTS stop 收尾（`application.cc:531`）
```cpp
if (state == kDeviceStateSpeaking) {
    if (listening_mode_ == kListeningModeManualStop) {
        SetDeviceState(kDeviceStateIdle);     // Manual: 一句一答
    } else {
        SetDeviceState(kDeviceStateListening); // Auto / Realtime: 持续对话
    }
}
```

#### 进入 Listening 时的 Auto 同步（`application.cc:883`）
```cpp
if (listening_mode_ == kListeningModeAutoStop) {
    audio_service_.WaitForPlaybackQueueEmpty();  // 等 TTS 真放完再开 MIC
}
```

### 1.5 AEC 与 VAD 的互斥关系

`AfeAudioProcessor` 在配置上让 AEC 和 VAD 二选一，`EnableDeviceAec` 切换时一开一关（`afe_audio_processor.cc:189`）：

```cpp
if (enable) {                         // 设备端 AEC 开启
    afe_iface_->disable_vad(afe_data_);
    afe_iface_->enable_aec(afe_data_);
} else {                              // 关闭 AEC
    afe_iface_->disable_aec(afe_data_);
    afe_iface_->enable_vad(afe_data_);
}
```

互斥的原因：

1. **资源约束**：S3 上 AEC（NN + 滤波）+ NS + Opus 编码已经接近算力上限，再叠 VAD 模型挤占 PSRAM 与 CPU；
2. **决策权归属**：realtime 模式下的端点判定权在云端（拿到 AEC 后的纯人声 + 结合 ASR partial），device VAD 的产出仅用于驱动 LED（`callbacks_.on_vad_change` 在 `application.cc:233`），无消费者；
3. **简化心智模型**：避免设备/服务端 VAD 同时存在导致的状态分叉。

### 1.6 三模式特征对照表

| 维度 | Manual | AutoStop | Realtime |
|---|---|---|---|
| Mode 字段 | `"manual"` | `"auto"` | `"realtime"` |
| 谁结束 listening | 设备（按键松开） | 服务端（VAD/端点） | 服务端（用户打断或回合结束） |
| TTS 期间拾音 | n/a（已 Idle） | **关闭** | **保持开启** |
| TTS 期间唤醒词 | n/a | 仅 AFE 唤醒词 | 不切换 |
| TTS 后状态 | **Idle** | Listening | Listening |
| `WaitForPlaybackQueueEmpty` | 否 | **是** | 否 |
| AEC 依赖 | 不需要 | 不需要 | **必须** |
| 上行流量特征 | 按键按下时间段 | listen 段，TTS 段静默 | **全程不间断** |
| 打断 TTS | 不适用 | 喊唤醒词 / 按键 | 直接说话（barge-in） |
| 默认配置触发 | 按住 PTT | `kAecOff`（默认） | `kAecOnDeviceSide` 或 `kAecOnServerSide` |

### 1.7 Realtime 模式的代价

| 项 | 量级 |
|---|---|
| 上行带宽 | Opus 16k mono VBR ≈ 12-24 kbps × 100% 时间 |
| WiFi TX 占空 | 每 60ms 一包，~17 包/秒，全程 |
| 设备功耗 | MIC + I2S RX + Opus 编码 + WiFi TX 全程在线，无法 light-sleep |
| 服务端算力 | 持续 Opus 解码 + 远端 AEC（server 模式）+ ASR streaming，按通话时长计费 |

这正是 realtime 不作为默认的根本原因。AutoStop 在 TTS 期间彻底关上行，省电省带宽；Realtime 用常时上行换 barge-in 体验。

---

## 第二部分：引入 Device Power VAD 的设计设想

### 2.1 背景与目标

服务端当前已实现两级 VAD：

```
[设备上行音频流] → [Power VAD（能量阈值粗筛）] → [TenVAD（NN 精判）] → [ASR streaming]
                                                    ↓
                                          超时无人声 → 视为 utterance 结束
```

服务端可接受任意时刻的上行流。Realtime 模式问题在于：**静音段也在持续上传 Opus 包**，对端的 Power VAD 大部分时间都在判"否"。如果把这一级粗筛**下沉到设备端**，由设备只在判定为"可能有语音"时才发起上行，则可以：

- 上行带宽节省约 60%（典型对话静音占比 60-70%）；
- 设备 WiFi TX 占空降低 → 续航显著改善；
- 服务端 Opus 解码量、TenVAD 推理量按比例下降；
- TenVAD 收到的输入信噪比更高（粗筛已挡掉大量低能量噪声），false positive 也下降。

### 2.2 适用前提

**只在 device-side AEC 路径上启用**（即 `aec_mode_==kAecOnDeviceSide`，`AfeAudioProcessor` 的 `aec_init=true`）。

理由：
- AFE 输出 = 已消回声的纯人声 → 在该信号上做能量阈值判定可靠，TTS 播放期间 barge-in 仍然准确；
- 若无 device AEC（codec 无回采参考），AFE 输出仍带扬声器回声，TTS 期间 power 一直过阈，gate 形同虚设。

`server-side AEC` / `kAecOff` 配置保持现有行为不变。

### 2.3 算法构成

#### 2.3.1 Power 计算

放在 `AfeAudioProcessor` 输出后（即 NS 之后），按 60ms 帧（960 samples @ 16kHz）计算短时能量或 RMS：

```cpp
int64_t energy = 0;
for (int i = 0; i < frame_samples; i++)
    energy += int32_t(pcm[i]) * pcm[i];
// S3 上 < 50 µs / 帧
```

#### 2.3.2 自适应阈值

不使用硬编码阈值，避免环境差异下的失效：

- 维护"近 1-2 秒能量的 percentile-25"作为噪声底；
- 触发阈值 = 噪声底 × 4-6 倍（dB 域 +12 dB 左右）；
- 噪声底仅在 SILENCE 状态下更新，避免被 speech 拉高。

#### 2.3.3 状态机

```
        energy >= threshold            energy < threshold (持续 hangover_ms)
SILENCE ────────────────────► SPEECH ──────────────────────────────────────► SILENCE
   ↑                            │
   │                            │ energy < threshold
   │                            ▼
   │                         HANGOVER
   │                            │
   │      energy >= threshold (重置回 SPEECH)
   └────────────────────────────┘
   hangover 超时
```

| 状态 | 行为 |
|---|---|
| `SILENCE` | 帧入 pre-roll ring buffer，**不上行**；持续更新噪声底 |
| `SPEECH` | 帧直接进 `audio_encode_queue_`，正常上行 |
| `HANGOVER` | 帧仍上行；hangover 计时器递减；期间能量回弹则重置回 SPEECH |

#### 2.3.4 Pre-roll Buffer（前驱缓冲）

防止吞首字。Power VAD 触发的瞬间，最近 200-300ms 的 PCM 已经在那里（"你好"的"你"在能量爬坡段，可能尚未越过阈值）。

实现：维持一个 ring buffer 存最近 N 帧（例 5 × 60ms = 300ms）。

- SILENCE 态每帧只入 buffer，不上行；
- SILENCE → SPEECH 跳变瞬间：**先把 ring buffer 里的全部帧依次入 encode_queue 上行**，再处理当前帧；
- 服务端 TenVAD 据此能拿到完整起音段。

#### 2.3.5 Hangover（拖尾保护）

防止吞尾字与误切短停顿。能量一掉到阈值下就停发，会切掉句末弱尾音（"好嘞"的"嘞"），也会把"你好——稍等——再问你"内部短停顿误判成两次 utterance。

实现：能量回落后维持 500-800ms 的 hangover 窗口继续上传，期间能量再次过阈则计时器重置。

#### 2.3.6 推荐参数

| 参数 | 推荐值 | 说明 |
|---|---|---|
| Pre-roll | 300 ms（5 帧） | 覆盖典型起音段 |
| Hangover | 600 ms（10 帧） | 覆盖句末弱尾音和短停顿 |
| 噪声底窗口 | 2 s | 自适应估计 |
| 触发倍数 | 4-6 倍噪声底 | dB 域 +12 dB |
| 噪声底初值 | 取 AFE 启动后前 500ms 的均值 | 避免冷启动误触发 |

### 2.4 与服务端的契约调整

需要服务端配合：

#### 2.4.1 TenVAD 上下文管理
若 TenVAD 是 stateful NN，断流后 speech burst 重新开始时**需 reset 内部状态**。若是 stateless per-frame 模型，无影响。

#### 2.4.2 端点超时切换为 wall-clock
当前"超时无人声 → 结束"如果是基于"数静音帧"实现，需要改为基于 wall clock：
- 因为静音帧根本不会到达；
- 改成"距上次收到帧已超过 T 秒 → utterance 结束"。

#### 2.4.3 推荐新增控制信令
仅靠"流断了"判断 speech 边界稍嫌隐式，建议新增显式信令：

```json
{"type":"listen","state":"speech_start"}   // SILENCE → SPEECH 时设备发出
{"type":"listen","state":"speech_end"}     // HANGOVER → SILENCE 时设备发出
```

收益：
- 服务端立即 reset TenVAD context；
- ASR streaming 更早 commit partial result；
- 更可靠地区分"网络抖动断流"与"设备主动停发"。

#### 2.4.4 ASR streaming 兼容性
许多流式 ASR（FunASR、Paraformer 等）需要连续帧维持声学模型状态。需确认链路是 `Opus → 解码 → TenVAD → ASR`，TenVAD 作 ASR 的 gate —— 这种结构下 ASR 只看到 voice 段，反而更干净。如果 ASR 直接接收所有帧，则需要在服务端 PowerVAD/TenVAD gate 后再喂 ASR。

### 2.5 代码改动点

#### 2.5.1 新增类 `PowerVadGate`

位置：`main/audio/processors/power_vad_gate.{h,cc}`

```cpp
class PowerVadGate {
public:
    enum class Decision { kDrop, kForward, kForwardWithPreroll };
    struct Output {
        Decision decision;
        std::vector<std::vector<int16_t>> preroll_frames;  // 仅 kForwardWithPreroll 非空
    };

    PowerVadGate(int frame_samples, int sample_rate);
    Output Process(std::vector<int16_t>&& frame);   // 不持有传入帧的所有权语义
    bool IsSpeechActive() const;                    // SILENCE = false, 其它 true
    void Reset();

private:
    enum class State { kSilence, kSpeech, kHangover };
    State state_ = State::kSilence;
    std::deque<std::vector<int16_t>> preroll_;
    int hangover_frames_left_ = 0;
    float noise_floor_ = 0;
    float threshold_multiplier_ = 5.0f;
    // 噪声底估计窗口、自适应更新逻辑等
};
```

#### 2.5.2 在 `AfeAudioProcessor::AudioProcessorTask` 中插入

`afe_audio_processor.cc:166`，把当前每帧直接 `output_callback_(...)` 改为：

```cpp
if (output_callback_) {
    while (output_buffer_.size() >= frame_samples_) {
        std::vector<int16_t> frame(output_buffer_.begin(),
                                   output_buffer_.begin() + frame_samples_);
        output_buffer_.erase(output_buffer_.begin(),
                             output_buffer_.begin() + frame_samples_);

#ifdef CONFIG_USE_DEVICE_POWER_VAD
        auto out = power_gate_->Process(std::move(frame));
        switch (out.decision) {
            case PowerVadGate::Decision::kDrop:
                break;
            case PowerVadGate::Decision::kForwardWithPreroll:
                // 触发 speech_start 信令
                if (on_speech_start_) on_speech_start_();
                for (auto& pf : out.preroll_frames)
                    output_callback_(std::move(pf));
                break;
            case PowerVadGate::Decision::kForward:
                output_callback_(std::move(frame));
                break;
        }
        // 检测 SPEECH/HANGOVER → SILENCE 边沿，触发 speech_end 信令
#else
        output_callback_(std::move(frame));
#endif
    }
}
```

#### 2.5.3 信令注入

新增回调 `on_speech_boundary_`，由 `AudioService` 串到 `Application`，再调 `protocol_->SendText(...)` 发出 `speech_start` / `speech_end`。

#### 2.5.4 Kconfig

```kconfig
config USE_DEVICE_POWER_VAD
    bool "Enable device-side power VAD gating in realtime mode"
    depends on USE_DEVICE_AEC
    default n
    help
        When enabled, the device runs a cheap power-based VAD on the AFE
        output. In realtime mode, silent frames are dropped instead of
        being uploaded. Pre-roll and hangover buffers protect against
        first/last syllable truncation. Requires server-side support
        for intermittent uplink streams.
```

#### 2.5.5 不需要改动的部分

- `AudioService` 的队列与任务模型不变；
- Opus 编码、网络协议、protocol 二进制格式不变；
- `AutoStop` / `Manual` 模式因为本来就不是常时上行，不受影响（也可不启用 gate）；
- `kAecOff` / `server-side AEC` 路径保持原样。

### 2.6 风险与缓解

| 风险 | 缓解 |
|---|---|
| 阈值过高吞掉耳语级低音量讲话 | 自适应噪声底 + 保守倍数（4×）+ pre-roll 兜底 |
| 阈值过低稳态噪声常触发 | AFE NS 已压低稳态噪声 + percentile-25 噪声底估计 |
| 服务端 TenVAD context 断裂导致首帧误判 | 显式 `speech_start` 信令 + pre-roll 给 TenVAD 留预热样本 |
| 网络抖动被误识别为语音结束 | 服务端区分"timeout"和"显式 speech_end" |
| TTS barge-in 响应变慢（多了 hangover 延迟） | barge-in 触发只看 SILENCE→SPEECH 跳变即时上报，不等 hangover |
| 与 wake word 链路冲突 | wake word 检测与 power gate 在不同分支（wake word 仅 Idle 期，power gate 仅 Listening 期），不冲突 |

### 2.7 预期收益量化

以典型对话场景估算（用户讲话占比 35%，TTS 播放占比 30%，静音占比 35%）：

| 指标 | 现状 realtime | 加 power gate 后 |
|---|---|---|
| 设备上行帧占比 | 100% | ~45%（讲话 35% + pre-roll/hangover ~10%） |
| 上行带宽 | 24 kbps × 100% | 24 kbps × 45% ≈ 10.8 kbps |
| WiFi TX 占空 | 100% | 45% |
| 服务端 Opus 解码量 | 100% | 45% |
| 服务端 TenVAD 推理量 | 100% | 45% |
| TenVAD 输入 SNR | 基线 | 提升（粗筛挡掉低能量段） |
| 端到端延迟 | 基线 | 不变（gate 增加 < 100 µs/帧） |
| Barge-in 响应 | 基线 | 不变（SILENCE→SPEECH 即时上报） |

### 2.8 落地路线建议

| 阶段 | 内容 |
|---|---|
| Phase 1 | 实现 `PowerVadGate` 单测（注入正弦/静音/真实录音 PCM，验证阈值/pre-roll/hangover） |
| Phase 2 | 集成到 `AfeAudioProcessor`，加 Kconfig 开关，默认 off，端到端验证不开启时行为完全不变 |
| Phase 3 | 服务端添加 `speech_start` / `speech_end` 信令处理 + 端点检测改 wall clock |
| Phase 4 | 真机灰度：device AEC + power gate 同时打开，对比关闭组的带宽、续航、识别准确率 |
| Phase 5 | 默认开启（在 device-AEC 路径上），为 server-AEC 路径研究单独方案 |

---

## 附：当前 `bread-compact-wifi` 的语音模式归属

| 配置项 | 值 | 影响 |
|---|---|---|
| Codec | `NoAudioCodecSimplex`（无 input_reference） | 无法支持 device AEC |
| `CONFIG_USE_DEVICE_AEC` | n | 不可用 |
| `CONFIG_USE_SERVER_AEC` | 取决于 menuconfig | 可选 |
| `aec_mode_` 默认 | `kAecOff` | 默认走 AutoStop |
| 当前能用的模式 | Manual（按键）+ AutoStop（唤醒） | Realtime 不可用 |
| 能否受益于 Power VAD | 否（前提是 device AEC，本 board 不具备） | — |

要让本板子受益于 Power VAD 改造，硬件层需先升级为带回采参考通道的 codec（如 ES7210 + ES8311 组合），让 AFE 能跑 device-side AEC。
