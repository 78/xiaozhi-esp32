# Realtime Debug V1

Go WebSocket 透明代理、设备诊断 TCP 收集器和本地调试网页。当前固件支持 Manual / Auto；Auto 是半双工，播放期间暂停上行，但允许指定唤醒词在本地触发打断。普通讲话不会中断播放。

## 启动

需要 Go 1.22 或更新版本。在本目录运行：

```powershell
go run ./cmd/realtime-debug --collector-listen 0.0.0.0:7319 --proxy-listen 0.0.0.0:7321 --web-listen 127.0.0.1:7322 --upstream-url 'wss://<workspace>.<region>.maas.aliyuncs.com/api-ws/v1/realtime?model=qwen3.8-omni-flash-realtime'
```

设备统一配置页中设置：

- WebSocket 地址：`ws://<电脑局域网IP>:7321/realtime`。
- 诊断地址：`tcp://<电脑局域网IP>:7319`，打开诊断，等级设为 2，并打开原始输入 PCM。
- 保存服务配置后重启。API Key 留空会保留旧值，页面不会返回密钥。

浏览器打开 `http://127.0.0.1:7322/`。不传 `--web-listen` 时默认端口是 7320。默认 TCP 收集器仅监听 127.0.0.1，设备接入需显式指定局域网地址。只有传入 `--upstream-url` 才启动代理。

代理按各方向的原始顺序转发 WebSocket 应用层消息。Authorization 仅在内存中转交上游，不写入记录。诊断端口为明文开发接口，应使用可信局域网；网页默认只监听本机。

## 页面与关联标识

左侧会话列表，右侧先显示对话文本，再显示音轨；原始事件默认折叠。支持搜索、事件详情、请求标识、波形、WAV 和完整导出。代理与诊断收集器的连接状态分别显示。

记录真实握手响应和事件中的 `reqid`、`request_id`，保留字段位置、来源与作用范围，同时保留 cloud session、response、event ID。握手 reqid 不能解释为每一轮对话的 reqid；服务端没有返回的标识不会补造。

代理会话与原始音频会话通过设备发送的 `X-Trace-Boot-Id` / `X-Trace-Session-Id` 显式关联。不会仅凭时间接近就合并不同设备或会话。设备 ID 使用设备 MAC。

## 三条音轨

| 页面音轨 | 来源 | 解释 |
| --- | --- | --- |
| 原始输入 input | ES8311 成功读取后、软件处理和重采样前 | 当前板为 24 kHz / 单声道 / PCM16；Auto 播放期间仍采集，通过独立诊断通道上传 |
| 云端上行 uplink | 代理实际收到的 input_audio_buffer.append | 设备处理后送往云端的内容；Auto 播放期间应暂停 |
| 云端下行 playback | 云端 response.audio.delta | 云端返回的音频，不代表扬声器实际播放结果 |

音频格式变化、回复边界、暂停和样本不连续会分段；WAV 不会把暂停两侧拼成一条连续录音。波形按 min/max 降采样，最多 600 个桶。设备单调时钟与电脑接收时钟分别保留，不应直接相减计算网络单程延迟。

设备状态事件区分云端 `response.done` 与本地播放排空。Auto 在回复生成且本地队列、解码和 I2S DMA 尾部都结束后恢复 Listen；不能仅凭云端完成事件判定播放结束。边界上已在途的上行消息仍可能晚于代理看到 response.created 到达。

## 存储与容量

默认数据目录为操作系统用户缓存目录下的 `xiaozhi-realtime-debug`，不在仓库内。会话包含 `manifest.json`、`events.jsonl`、`payloads.dat` 和 `pcm-*.dat`。代理及关联诊断会话分别存储；完整导出 ZIP 内包含各自的会话 ZIP。保留旧 AEC 记录的读取兼容性，当前固件和页面不提供 AEC 实验流程。

| 参数 | 默认值 | 行为 |
| --- | --- | --- |
| --data-dir | 用户缓存目录 | 指定录音目录 |
| --max-session-mib | 256 | 单会话记录上限 |
| --max-total-mib | 1024 | 总记录上限；达到后停止记录，不删除已有录音 |
| --retention-days | 0 | 0 不清理；显式设为正数后，仅在启动时删除超过天数的已结束会话 |

录制与网络转发分离，代理记录队列限制为 256 帧 / 16 MiB。记录来不及时产生 gap，转发继续。达到容量会标记 truncated。检查 gaps、truncated、interrupted 和 live 后再判断录音是否完整。重启时从完整索引行恢复，裁去未索引数据尾部，将未结束会话标记 interrupted。

事件中的 `proxy_received_at`、`forward_start_at`、`forward_done_at` 表示代理网络时间；顶层 `received_at` 是写入记录时间。记录器延迟不应解释为网络延迟。

## 诊断帧

TCP 帧为 `uint32_be length`，然后是 `uint16_be version=1`、`uint16_be kind`、`uint32_be metadata_length`、UTF-8 JSON 元数据、负载。单帧最多 256 KiB，元数据最多 4 KiB。

首帧 hello 包含 boot_id、device_id、firmware_version。后续帧携带 boot_id、trace_session_id、seq、mono_us。PCM 附带采样率、声道、位深、字节序；原始输入还携带样本位置和采集时间。设备原始采集回调只复制到有界队列，JSON 和网络发送由独立任务处理。队列满或诊断断线均应检查 gap，不能把诊断丢失解释为云端丢包。

## 验证

设备侧诊断帧队列最多 256 帧：PSRAM 构建最多 512 KiB，无 PSRAM 构建最多 32 KiB，包含节点字节。容量检查在负载分配之前完成；PSRAM 构建将帧明确放在 PSRAM，申请失败会丢弃诊断数据，后续记录 trace_gap。出现 gap 的音轨不能视为完整录音。

```powershell
go test ./...
go vet ./...
go build -o realtime-debug.exe ./cmd/realtime-debug
```

Windows 上请不要覆盖运行中的 exe。实机步骤和已知验证限制见 [V1 实施记录](../../designv1/implementation-record.md)。录音、鉴权信息、设备配置快照不应加入版本库。
