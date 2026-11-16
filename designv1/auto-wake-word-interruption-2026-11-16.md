# Auto 唤醒词打断实现与验收

日期：2026-11-16。工作目录 `D:\work\xiaozhi-realtime-v1`，Waveshare ESP32-S3 Touch LCD 3.5B，ESP-IDF v6.1，COM3。

## 行为与修改

Auto 的 Speak 阶段继续向本地 AFE / WakeNet 提供麦克风数据，普通讲话不上云。检测到指定唤醒词后取消当前回复、丢弃旧上行、清除本地解码与播放队列，再切回 Listen，排空在途写入和硬件尾音后恢复收音与上行。无需重新建立 WebSocket。

本轮仅修改 `main/application.cc`：

- Speak 的本地唤醒词检测条件对阿里云 Auto 放开，Manual 保持关闭，小智原路径保持原行为。
- Auto 播放中唤醒后清除 `aliyun_response_done_`、`pending_manual_commit_`，调用 `DiscardUplink()`、`ResetDecoder()`，记录 `wake_word_interrupt` 事件。
- 复用现有 `AbortSpeaking()` 使旧回复代次失效：云端仍生成时发送 response.cancel，旧音频及迟到 response.done 被响应 ID / 代次过滤。云端已完成时仅清除本地余音，不发送无活动回复的取消请求。
- 复用播放代次隔离、20 ms 分块写入、DMA 尾部等待和非阻塞的延迟监听流程；进入 Listen 后沿用提示音。

设备 AEC 保持关闭，NSNet2 与原始 ES8311 诊断采集保持原有设置。

## 构建与烧录

构建成功，应用大小 `0x27f1d0`，应用分区剩余 37%；COM3 烧录并通过 Hash of data verified / Done。`build/xiaozhi.bin` SHA-256：`c1d4457f37a91ce3791a3a6b56958ff1517fd6b585d4427ff6997090780b43ff`。

触及文件 clang-format 和 diff 空白检查通过。主机测试 147 项，28 跳过、6 个错误，与此前基线一致：5 个 Windows 目录锁定/清理错误、1 个缺少主机 C++ 编译器。本轮没有更改共享 Protocol 契约，没有重做其他芯片、MQTT/UDP 或真实小智服务回归。

## 真机声学测试

电脑扬声器播放 Windows 语音合成的“Hi, ESP”/“Hi E S P”，由设备实际麦克风、WakeNet 检测；没有通过 HTTP 注入唤醒事件。HTTP 仅用于开始/停止测试会话、模式及音量控制。设备日志实际出现 `Wake word detected: Hi,ESP (state: 6)` 和 `Abort speaking`。

原始配置：Auto、音量 100、诊断等级 2、原始 PCM 开启、代理和诊断分别指向电脑 7321 / 7319，API Key 存在。先做待机识别校准。

音量 100 的首次播放中测试连续三次未识别，不能认定满音量可稳定打断。将设备音量临时降至 60 后完成以下三轮；没有改变检测灵敏度。初次失败脚本结束后另有一次唤醒事件，但没有同步声源记录，不计入声学通过结果。

| 测试 | 诊断会话 | 结果 |
| --- | --- | --- |
| 云端生成中打断一 | `8a9788f14842a900a3a74cff` | 云端 response.cancel → response.done(status=cancelled)，新问题完成 |
| 云端生成中打断二 | `45842e1a6e54acca6cbbeccb` | 普通讲话不打断；唤醒后取消成功，新问题完成 |
| 云端已结束、本地仍播放 | `e2afaccc4d7d69afbcfd6905` | 先确认 completed 且设备仍 Speak；普通讲话不打断；唤醒清除余音，新问题完成；未发送 response.cancel |

三轮均为同一设备 boot_id `1b571b1f2e8c7560`，诊断 gap=0。对应代理会话依次为 `585cc5fcf03ca79eeedf6979`、`00d2241c281f710c18261d32`、`5e8e86da2a8514f47a03a1b8`。

打断前的 Speak 区间，设备云端 PCM 上行均为 0，原始输入块分别为 437、989、2072。唤醒后 1.5 秒检查窗口内没有新的 pcm_playback 诊断帧，随后新问题正常完成。第二轮旧 response.done 在取消后约 2.6 秒才到达，未把设备拉回旧播放状态。

从代码处理后的 `wake_word_interrupt` 标记到 `listening_start` 为约 60～67 ms，到首个设备上行 PCM 记录为约 218～238 ms。这不包含唤醒词识别所需时间，也不是麦克风或扬声器的声学实测延迟；调试下行轨不代表实际扬声器输出。

Manual 回归：切到 Manual，录音并提交后进入 Speak，播放两段同样的唤醒测试声，未触发唤醒事件且设备继续 Speak；随后停止并恢复 Auto。

## 结束状态与限制

结束时为 Auto / idle，恢复原音量 100，Wi-Fi、代理地址、诊断设置和 API Key 保留，串口释放，只关闭本轮自行启动的 Go 测试进程。可使用 VS Code 的 Go Debug 配置重新启动服务。

当前功能已实现并在音量 60 的现有摆位下通过声学测试；满音量、远距离、轻声、多种真人声音，以及回复自身包含唤醒词时的误触发率仍需验证。没有 AEC，不能保证所有声学条件下可靠识别，也不支持普通讲话直接插话。

日志在本机临时目录：`xiaozhi-auto-wake-build.log`、`xiaozhi-auto-wake-flash.log`、`xiaozhi-auto-wake-host-tests.log`、`xiaozhi-auto-wake-volume60-serial.log`、`xiaozhi-auto-wake-after-done-serial.log`、`xiaozhi-auto-wake-manual-serial.log`、`xiaozhi-auto-wake-analysis.json`。录音与事件位于 `%LOCALAPPDATA%\xiaozhi-auto-wake-validation`，不入库。
