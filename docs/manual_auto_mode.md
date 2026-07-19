按键触发和激活词触发，是否可以自由切换?

## 解答

两层的行为不一样，结论先说：固件侧可以自由切换，但 xzserver 在同一个 session 内模式只升不降——manual → auto 允许一次，auto → manual 会被忽略。跨 session 则完全自由。

固件侧（每次触发独立设置）

每次交互都重新设置 listening mode，没有锁定：
- 按键按下 → HandleStartListeningEvent → kListeningModeManualStop，发 listen start (mode=manual)；
- 唤醒词 → HandleWakeWordDetectedEvent → GetDefaultListeningMode()（无 AEC 板是 auto），发 listen start (mode=auto)。

音频通道如果还开着就直接复用，只换 mode。

xzserver 侧（session 内单向升级一次）

orchestrator.go 的 ensureMode 明确写了：mode 在 session 内只允许从初始 manual 单向升级一次（manual → auto/realtime/text），运行中切换忽略。session 的生命周期 = 一次 hello 到通道关闭（goodbye/超时）。

所以同一通道内的实际行为：

- 先按键后唤醒词：manual → auto 升级成功，之后都按 VAD 模式跑，正常。
- 先唤醒词后按键：服务端保持 VAD 模式。按键的音频照样会被服务端 VAD 切段识别，对话大体能跑通，但「松开按键即结束」的语义丢了——turn 什么时候结束由服务端 VAD 静音判定说了算，松开时发的 listen stop 在 VAD 模式下是 no-op。在你这个噪音环境下（服务端 VAD 停不下来）体验会明显变差。另外这种情况下 manual 首包延迟打印的起点（松开按键）和服务端实际切段时刻对不上，量出来的数会偏大。
- 跨 session：通道关闭后（对话结束空闲超时或 goodbye），下次触发重新 hello、新 session，mode 重新协商，怎么切都行。

建议

调试延迟时两种方式分开测：重启对话（等通道关掉或重启设备）后用纯按键测 manual 打印，或纯唤醒词测 auto 打印，避免混用时落进「auto session 里的假 manual」这种歧义状态。如果以后真需要 session 内自由切换，改的是 xzserver 的 ensureMode（放开双向切换需要处理 VAD gate 的启停），固件不用动。