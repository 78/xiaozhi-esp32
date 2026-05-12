
① 「abort 消息会打断客户端播放」—— 不准确

abort 只是告知 server「我要停」，客户端是否真的立刻静音，取决于客户端自己当前走的是哪条路径，不是 server 决定的：

触发路径: 唤醒词打断（state=Speaking）
是否本地清队列: 是——SetListeningMode(...) → EnableVoiceProcessing(true) 内部 ResetDecoder() 清 decode/playback 队列（audio_service.cc:588, 668-680）
听感: 立刻静音
────────────────────────────────────────
触发路径: StartListening 按键打断（state=Speaking，路径 B）
是否本地清队列: 是——同上
听感: 立刻静音
────────────────────────────────────────
触发路径: ToggleChatState 单击（state=Speaking，路径 A）
是否本地清队列: 否——仅 AbortSpeaking，不切状态。即使 server 回 tts stop 切到 idle，也不调 ResetDecoder
听感: 本地 decode + playback 队列里残留的几百毫秒 TTS 会继续播完

OpusCodecTask 和 AudioOutputTask 不看 device state，谁清队列谁负责。Server 端只要做到「不再下发新的 TTS
包」即可，没办法通过下行消息强制客户端立刻静音（除非自己清队列）—— 这是客户端协议设计的事实。

② 「停止 LLM/TTS 处理」—— 对

Server 收到 abort 应当：
1. 取消正在进行的 LLM 推理
2. 停止 TTS 生成
3. 停止后续 TTS 音频包下发
4. 路径 A 还需要回一条 tts stop，否则客户端卡 Speaking（前一轮分析过）

③ 「同时开启一个新的 turn，语音流上行开始」—— 不一定

新 turn 是否立即开启由客户端决定，server 只是被通知方：

触发源: 唤醒词打断
客户端是否自动开新 turn？: 是——立刻 SendStartListening(GetDefaultListeningMode())，并 EnableVoiceProcessing(true) 启动上行
server 看到的后续消息: abort → listen start → 音频流
────────────────────────────────────────
触发源: StartListening 按键（路径 B）
客户端是否自动开新 turn？: 是——同上
server 看到的后续消息: abort → listen start → 音频流
────────────────────────────────────────
触发源: ToggleChatState 单击（路径 A）
客户端是否自动开新 turn？: 否——只发 abort，状态留 Speaking，等用户再点一次才进入 idle→listening 流程
server 看到的后续消息: 只有 abort，需自己回 tts stop；后续没动静直到下次用户交互

所以 server 不能假设「收到 abort 后一定会看到新 listen start」，得允许「只 abort 不开新轮」这种情况。

④ 「reason=wake_word_detected 不做特殊处理」—— 取决于 server 实现

xiaozhi-esp32 这边是客户端，它有意把信号区分出来发（protocol.cc:42-49）：
- {"type":"abort"} ← 物理交互
- {"type":"abort","reason":"wake_word_detected"} ← 唤醒词

如果你的 server 当前对两者一视同仁，功能上没问题——客户端在两种路径下都会自己发 listen start 把新 turn 开起来，server 不需要靠 reason 才能知道「要进新
turn」。

但 reason 字段是免费信号，建议至少记录到日志/埋点里，方便后续判断：
- 「唤醒词打断率」高 → 用户喜欢中途插话（UX 信号）
- 「按键打断率」高 → TTS 太长/太啰嗦（产品信号）
- 上下文衔接：唤醒词打断通常意味着新话题，是否复用 session/对话历史可以基于这个判断

最小可用的 server 处理建议

on_abort(msg):
    1. cancel_inflight_llm()
    2. cancel_inflight_tts()
    3. stop_sending_audio_packets()
    4. send({"type":"tts","state":"stop"})      # 兜底，路径 A 必须，其它路径无害
    5. log("abort_reason", msg.reason or "none")
    # 不需要主动开新 turn，等客户端 listen start

幂等、KISS，能 cover 所有三种路径。



```c
回到你看到的 compact_wifi_board.cc:26-39：
boot_button_.OnClick(...)        → ToggleChatState();   // 路径 A
touch_button_.OnPressDown(...)   → StartListening();    // 路径 B（按下）
touch_button_.OnPressUp(...)     → StopListening();     // 路径 B（松开）

两条路径的触发条件是「绑了哪种回调」，不是同一个按键的长短按差别。OnClick 是 button 库内部按"完整一次按下+松开"才回调，OnPressDown/OnPressUp
是分别在按下/松开瞬间回调。

你的两个问题

Q1: 唤醒词模式下 abort 总是开新 turn？

是。application.cc:808-823 的 HandleWakeWordDetectedEvent：

```c
} else if (state == kDeviceStateSpeaking || state == kDeviceStateListening) {
    AbortSpeaking(kAbortReasonWakeWordDetected);
    while (audio_service_.PopPacketFromSendQueue());
    if (state == kDeviceStateListening) {
        protocol_->SendStartListening(GetDefaultListeningMode());   // 开新 turn
        ...
    } else {                                                         // Speaking
        play_popup_on_listening_ = true;
        SetListeningMode(GetDefaultListeningMode());                 // → Listening 状态 → SendStartListening
    }
}
```

唤醒词命中就是有用户在说话的客观事实，所以代码逻辑里 abort 必然伴随新一轮 listen start + 上行音频流。Server 收到 reason=wake_word_detected 的 abort
后，几乎可以无脑预期「马上会有 listen start」。

⚠️ 细节：本地唤醒词检出的那段音频帧默认 不重传上行（audio_service_.PopPacketFromSendQueue() 已经清空了），server
听到的是唤醒词之后的语音。如果用户喊「小智，几点了」，server 只听到「几点了」。

Q2: 按键模式下 abort 之后可能没有上行，是因为「按得短」？

不是看时长，看绑的是 OnClick 还是 OnPressDown。 这是物理按键库的两种独立回调，不是「短按 vs 长按」。

┌─────────────┬─────────────────────────────┬──────────────────────────────────────────────────┬───────────────────────────┐
│    回调     │          触发时机             │                    走的应用层                     │   abort 后是否开新 turn   │
├─────────────┼─────────────────────────────┼──────────────────────────────────────────────────┼───────────────────────────┤
│ OnClick     │ 按下+松开完整一次结束才回调     │ ToggleChatState（路径 A）                         │ 否                        │
├─────────────┼─────────────────────────────┼──────────────────────────────────────────────────┼───────────────────────────┤
│ OnPressDown │ 按下瞬间立刻回调               │ StartListening（路径 B，按住说话进入 listening）     │ 是（按下瞬间就开新 turn） │
├─────────────┼─────────────────────────────┼──────────────────────────────────────────────────┼───────────────────────────┤
│ OnPressUp   │ 松开瞬间回调                  │ StopListening（manual mode 收尾）                 │ ——                        │
├─────────────┼─────────────────────────────┼──────────────────────────────────────────────────┼───────────────────────────┤
│ OnLongPress │ 持续按住超阈值                 │ board 自定义                                     │ ——                        │
└─────────────┴─────────────────────────────┴──────────────────────────────────────────────────┴───────────────────────────┘

你说的「我们交互要求按键持续按下」——那对应的就是 OnPressDown / OnPressUp 这套，走路径 B：
- 在 Speaking 状态下用户按下按键 → StartListening 立刻 AbortSpeaking + SetListeningMode(Manual) → 状态切到 Listening，开始上行
- 一直按住时持续上行
- 松开按键 → SendStopListening
- 整条路径总是带新 turn，server 不会出现「收到 abort 但后续没语音」的情况

建议：如果你产品 UX 严格只允许"按住说话"，board 代码里应当只绑 OnPressDown/OnPressUp，不绑 OnClick。否则像 compact_wifi_board.cc 这样 boot_button 仍绑了
OnClick → ToggleChatState，用户点一下就会进入路径 A——server 端就会看到「孤立 abort 无后续音频」的情况。

如果硬件只有一个物理键，建议改成：
boot_button_.OnPressDown([] { Application::GetInstance().StartListening(); });
boot_button_.OnPressUp([]   { Application::GetInstance().StopListening();  });
// 不绑 OnClick
这样 server 端的 abort 处理可以简化成「一定会跟一条 listen start」，少一个分支。



## realtime 模式下 server 端下发打断

realtime = 设备 AEC（或带时间戳的裸流）+ 服务端 VAD 的全双工长会话。
客户端只在唤醒词或按键时主动 abort；自然语音打断完全是服务端语义，客户端只负责在收到tts stop 后切状态、丢弃后续包，本地解码尾巴会自然消散。

- Device-AEC 路径：afe_audio_processor.cc:191-193 开 AEC 的同时 disable_vad() —— 设备 VAD 被显式关掉
- Server-AEC 路径：设备只发裸 PCM/Opus，本来就没本地 VAD
- realtime 模式发的 listen JSON 是 "mode":"realtime"（protocol.cc:60-66），约定就是服务端持续接收、自己判段

所以全双工长流，何时收尾、何时打断完全由服务端 VAD 决定，服务端通过下发 {"type":"tts","state":"stop"} 让设备退出 Speaking 状态（application.cc:531-545）。
