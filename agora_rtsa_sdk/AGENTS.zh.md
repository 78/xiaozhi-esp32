# RTSA SDK 开发者指南

## 概述

本目录包含 RTSA（Real-Time media Stream Accelerate）SDK 的示例程序。
每个 `hello_xxx` Demo 以最少的代码演示一个 SDK 功能，可作为开发者构建自己应用的起点。

你只需要一个头文件 `agora_rtc_api.h` — 它包含所有公开 API、数据类型、回调和错误码。

## Demo 列表

| Demo | 功能 | 说明 |
|------|------|------|
| `hello_rtsa` | 音视频流传输 | 通过 Agora SD-RTN 发送和接收音视频数据。支持 H.264/H.265/JPEG 视频和 PCM/Opus/AAC/G.711/G.722 音频。最全面的 Demo，覆盖 SDK 核心工作流程。 |
| `hello_rtm` | RTM 消息通道 | 通过 RTM（Real-Time Messaging）通道进行点对点消息传输。支持交互式文本聊天和自动化吞吐量测试（可配置消息大小/速率）。 |
| `hello_rdt` | 可靠数据传输 | 通过 RDT 通道在对端之间可靠传输文件。支持 MD5 校验、进度跟踪和可配置码率。 |
| `hello_rtcm` | 媒体控制消息 | 在频道内发送/接收自定义控制消息。支持广播（发给所有用户）和单播（发给指定对端）。 |
| `hello_stream_message` | 数据流 | 创建有序/可靠的数据流，在频道内发送结构化消息。 |
| `audio_player` | 音频播放 | 从 SDK 接收音频并通过 ALSA（Linux 音频子系统）播放。需要 ALSA 开发库。 |

## 目录结构

SDK 发布包中 example 目录的结构如下（由 `build-lite.sh` 打包生成）：

```
example/
├── hello_rtsa/                # 核心音视频流传输 Demo
│   ├── hello_rtsa.c           # 主 Demo：单连接
│   ├── hello_rtsa_multi.c     # 多连接变体
│   ├── app_config.h           # 配置和参数解析
│   └── CMakeLists.txt
├── hello_rtm/                 # RTM 消息 Demo（按配置包含）
├── hello_rdt/                 # 可靠数据传输 Demo（按配置包含）
├── hello_rtcm/                # 媒体控制消息 Demo（按配置包含）
├── hello_stream_message/      # 数据流 Demo（按配置包含）
├── audio_player/              # ALSA 音频播放 Demo（按配置包含）
├── out/
│   └── {ARCH}/                # Demo 可执行文件和运行时输出
├── test_data/                 # 共享测试媒体文件
│   ├── send_video.h264        # H.264 测试视频
│   ├── send_video.h265        # H.265 测试视频
│   ├── send_audio_16k_1ch.pcm # 16kHz 单声道 PCM 音频
│   ├── send_audio_8k_1ch.pcm  # 8kHz 单声道 PCM 音频
│   ├── send_audio.opus        # Opus 编码音频
│   ├── send_audio.g722        # G.722 编码音频
│   ├── send_audio.pcma        # G.711 A-law 音频
│   ├── send_audio.pcmu        # G.711 μ-law 音频
│   └── send_audio_*.aac       # AAC 编码音频（8k/16k/32k/48k）
├── third-party/               # 预编译的第三方库
│   ├── file_parser/           # 媒体文件解析库
│   ├── json_parser/           # JSON 解析库（jsmn）
│   └── build.sh               # 第三方库重新编译脚本
├── scripts/                   # CMake 辅助脚本
│   ├── env.cmake              # 环境检测
│   ├── toolchain.cmake        # 交叉编译工具链
│   └── check.cmake            # 依赖检查
├── build.sh                   # 统一构建脚本（通过 `-a/-f/-t/-b` 传参）
└── CMakeLists.txt             # 顶层 CMake 配置（自动发现所有 Demo 子目录）
```

注意：发布包中包含的 Demo 取决于 SDK 构建配置。`hello_rtsa` 始终包含；其他 Demo（hello_rtm、hello_rdt、hello_rtcm、hello_stream_message、audio_player）仅在 SDK 打包时启用了对应功能时才会包含。

## 构建

### 前置条件

- SDK 已安装到 `../agora_sdk/`（头文件在 `include/`，库文件在 `lib/{ARCH}/`）
- 第三方库在 `third-party/` 下（如需重新编译：`./third-party/build.sh -a <ARCH> -f <TOOLCHAIN> -t <BUILD_TYPE>`）
- CMake >= 2.4，GCC 支持 C99

### 构建命令

SDK 发布包中的 Demo 统一通过 `build.sh` 构建，构建参数通过命令行选项传递，不再使用环境变量传参。

```bash
# 构建所有 Demo（使用默认工具链）
./build.sh -a <ARCH>

# 使用自定义工具链构建
./build.sh -a <ARCH> -f /path/to/toolchain.cmake

# 指定构建类型
./build.sh -a <ARCH> -t debug

# 重新构建
./build.sh -a <ARCH> -f /path/to/toolchain.cmake -t release -b rebuild
```

`CMakeLists.txt` 会自动发现所有包含 `CMakeLists.txt` 的 Demo 子目录并编译。编译产物输出到 `out/{ARCH}/` 目录，随包附带的测试媒体文件保留在 `test_data/` 目录。

## SDK API 参考

所有 API 声明在 `agora_rtc_api.h` 中，引用方式：

```c
#include "agora_rtc_api.h"
```

### SDK 生命周期

每个应用都遵循以下生命周期：

```
agora_rtc_init()                       初始化 SDK（每个进程仅一次）
  └─► agora_rtc_create_connection()    创建连接实例
        └─► agora_rtc_join_channel()   加入频道
              ├─► 发送/接收循环         发送和接收音视频/数据
              └─► agora_rtc_leave_channel()
        └─► agora_rtc_destroy_connection()
  └─► agora_rtc_fini()                释放所有 SDK 资源
```

### 初始化

```c
// 1. 定义事件回调
agora_rtc_event_handler_t handler = { 0 };
handler.on_join_channel_success = my_on_join_success;
handler.on_audio_data           = my_on_audio_data;
handler.on_video_data           = my_on_video_data;
handler.on_error                = my_on_error;
// ... 按需注册更多回调

// 2. 配置服务选项
rtc_service_option_t opt = { 0 };
opt.area_code = AREA_CODE_GLOB;                // 区域：AREA_CODE_CN、AREA_CODE_NA 等
opt.log_cfg.log_level = RTC_LOG_INFO;
opt.log_cfg.log_path  = "io.agora.rtc_sdk";

// 3. 初始化 SDK
int ret = agora_rtc_init(app_id, &handler, &opt);
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `app_id` | `const char *` | Agora App ID |
| `event_handler` | `agora_rtc_event_handler_t *` | 回调处理器（见事件回调章节） |
| `option` | `rtc_service_option_t *` | 服务选项：area_code、日志配置、domain_limit |

### 连接与频道

```c
// 创建连接
connection_id_t conn_id;
agora_rtc_create_connection(&conn_id);

// 配置频道选项
rtc_channel_options_t ch_opt = { 0 };
ch_opt.auto_subscribe_audio = true;
ch_opt.auto_subscribe_video = true;
ch_opt.enable_audio_jitter_buffer = true;       // 平滑音频播放
ch_opt.enable_audio_mixer = false;              // 将所有远端音频混合为一路

// 音频编码器配置（仅 PCM 输入时需要，发送已编码音频如 AAC/Opus 时跳过）
ch_opt.audio_codec_opt.audio_codec_type = AUDIO_CODEC_TYPE_OPUS;  // 将 PCM 编码为 Opus
ch_opt.audio_codec_opt.pcm_sample_rate  = 16000;
ch_opt.audio_codec_opt.pcm_channel_num  = 1;

// 加密（可选）
ch_opt.crypto_opt.enable = true;
ch_opt.crypto_opt.mode   = AES_128_GCM2;       // 推荐模式
sprintf(ch_opt.crypto_opt.key, "%s", key_str);
memcpy(ch_opt.crypto_opt.salt, salt_bytes, 32); // GCM2 模式必须设置

// RDT（可选）
ch_opt.enable_rdt = true;

// 加入频道
agora_rtc_join_channel(conn_id, "my_channel", uid, token, &ch_opt);
// 等待 on_join_channel_success 回调后再发送数据
```

### 发送音频

```c
audio_frame_info_t info = { 0 };
info.data_type = AUDIO_DATA_TYPE_PCM;           // 或 AUDIO_DATA_TYPE_OPUS、AUDIO_DATA_TYPE_AACLC 等
agora_rtc_send_audio_data(conn_id, pcm_buf, pcm_len, &info);
```

支持的 `audio_data_type_e` 值：

| 枚举值 | 编码格式 | 采样率 |
|--------|----------|--------|
| `AUDIO_DATA_TYPE_PCM` (100) | 原始 PCM（需启用 audio_codec） | 由 `pcm_sample_rate` 配置 |
| `AUDIO_DATA_TYPE_OPUS` (1) | Opus | 16kHz |
| `AUDIO_DATA_TYPE_OPUSFB` (2) | Opus Full Band | 48kHz |
| `AUDIO_DATA_TYPE_PCMA` (3) | G.711 A-law | 8kHz |
| `AUDIO_DATA_TYPE_PCMU` (4) | G.711 μ-law | 8kHz |
| `AUDIO_DATA_TYPE_G722` (5) | G.722 | 16kHz |
| `AUDIO_DATA_TYPE_AACLC_8K` (6) | AAC-LC | 8kHz |
| `AUDIO_DATA_TYPE_AACLC_16K` (7) | AAC-LC | 16kHz |
| `AUDIO_DATA_TYPE_AACLC` (8) | AAC-LC | 48kHz |
| `AUDIO_DATA_TYPE_HEAAC` (9) | HE-AAC | 32kHz |

内置音频编码器（`audio_codec_type_e`，仅 PCM 输入时使用）：

| 枚举值 | 编码器 |
|--------|--------|
| `AUDIO_CODEC_DISABLED` (0) | 不编码（直通） |
| `AUDIO_CODEC_TYPE_OPUS` (1) | Opus 编码器 |
| `AUDIO_CODEC_TYPE_G722` (2) | G.722 编码器 |
| `AUDIO_CODEC_TYPE_G711A` (3) | G.711 A-law 编码器 |
| `AUDIO_CODEC_TYPE_G711U` (4) | G.711 μ-law 编码器 |

### 发送视频

```c
video_frame_info_t info = { 0 };
info.data_type   = VIDEO_DATA_TYPE_H264;        // 或 H265、GENERIC_JPEG
info.frame_type  = VIDEO_FRAME_AUTO_DETECT;     // SDK 自动检测关键帧/非关键帧
info.frame_rate  = 25;                          // 0 = 使用实际时间戳
info.stream_type = VIDEO_STREAM_HIGH;
agora_rtc_send_video_data(conn_id, frame_buf, frame_len, &info);
```

支持的 `video_data_type_e` 值：

| 枚举值 | 格式 |
|--------|------|
| `VIDEO_DATA_TYPE_H264` (2) | H.264 |
| `VIDEO_DATA_TYPE_H265` (3) | H.265 |
| `VIDEO_DATA_TYPE_GENERIC_JPEG` (20) | JPEG |
| `VIDEO_DATA_TYPE_YUV420` (0) | YUV420 |
| `VIDEO_DATA_TYPE_GENERIC` (6) | 通用格式 |

带宽估计 — 根据网络状况调整编码器码率：

```c
// 设置初始 BWE 参数
agora_rtc_set_bwe_param(conn_id, min_bps, max_bps, start_bps);

// 通过回调响应网络变化
void on_target_bitrate_changed(connection_id_t conn_id, uint32_t target_bps) {
    // 将编码器码率调整为 target_bps
}

// 处理关键帧请求（由丢包触发）
void on_key_frame_gen_req(connection_id_t conn_id, uint32_t uid, video_stream_type_e type) {
    // 立即生成并发送关键帧
}
```

### 接收音视频

数据通过事件回调传递。在 `agora_rtc_init()` 之前注册回调：

```c
// 接收单个远端用户的音频
void on_audio_data(connection_id_t conn_id, uint32_t uid, uint16_t sent_ts,
                   const void *data, size_t len, const audio_frame_info_t *info) {
    // info->data_type 标识音频格式
    // 处理或保存音频数据
}

// 接收混音后的音频（所有远端用户混合为一路）
// 需要：ch_opt.enable_audio_mixer = true
void on_mixed_audio_data(connection_id_t conn_id, const void *data, size_t len,
                         const audio_frame_info_t *info) {
    // 每 20ms 回调一次，数据为混合后的 PCM
}

// 接收远端用户的视频
void on_video_data(connection_id_t conn_id, uint32_t uid, uint16_t sent_ts,
                   const void *data, size_t len, const video_frame_info_t *info) {
    // info->data_type、info->frame_type、info->stream_type
    // 处理或保存视频数据
}
```

### 静音控制

```c
// 停止/恢复发送本地音视频
agora_rtc_mute_local_audio(conn_id, true);      // 静音本地音频
agora_rtc_mute_local_video(conn_id, true);      // 停止本地视频

// 停止/恢复接收远端音视频
agora_rtc_mute_remote_audio(conn_id, remote_uid, true);  // uid=0 表示所有用户
agora_rtc_mute_remote_video(conn_id, remote_uid, true);  // uid=0 表示所有用户
```

### RTM（实时消息通道）

RTM 提供独立于 RTC 频道的点对点消息通道。

```c
// 定义 RTM 事件回调
agora_rtm_handler_t rtm_handler = { 0 };
rtm_handler.on_rtm_event = my_on_rtm_event;              // 登录/登出/踢出事件
rtm_handler.on_rtm_data  = my_on_rtm_data;               // 收到消息
rtm_handler.on_rtm_send_data_result = my_on_send_result;  // 发送确认

// 登录
agora_rtc_login_rtm(rtm_uid, rtm_token, &rtm_handler);
// 等待 on_rtm_event 回调 RTM_EVENT_TYPE_LOGIN

// 发送消息（最大 31KB，最多 60 条/秒）
agora_rtc_send_rtm_data(peer_rtm_uid, msg, msg_len, msg_id, custom_type);

// 登出
agora_rtc_logout_rtm();
```

RTM 事件类型（`rtm_event_type_e`）：`RTM_EVENT_TYPE_LOGIN` (0)、`RTM_EVENT_TYPE_KICKOFF` (1)、`RTM_EVENT_TYPE_EXIT` (2)。

### RDT（可靠数据传输）

RDT 在频道内提供可靠的点对点数据通道。通过 `ch_opt.enable_rdt = true` 启用。

```c
// 发送数据（两种流类型）
// RDT_STREAM_CMD:  可靠、高优先级，最大 256 字节/包，100 包/秒
// RDT_STREAM_DATA: 可靠、受拥塞控制，最大 1024 字节/包
agora_rtc_send_rdt_msg(conn_id, remote_uid, RDT_STREAM_DATA, data, len);

// 查询通道状态
rdt_status_info_t info;
agora_rtc_get_rdt_status_info(conn_id, remote_uid, &info);
// info.state: RDT_STATE_CLOSED / OPENED / BLOCKED / PENDING / BROKEN
```

RDT 回调：
- `on_rdt_state(conn_id, uid, state)` — 通道状态变化
- `on_rdt_msg(conn_id, uid, type, msg, len)` — 收到 RDT 消息

### RTCM（媒体控制消息）

在频道内发送自定义控制消息（最大 1024 字节）。

```c
// 广播给频道内所有用户（remote_uid = 0）
agora_rtc_send_media_ctrl_msg(conn_id, 0, payload, length);

// 单播给指定用户
agora_rtc_send_media_ctrl_msg(conn_id, peer_uid, payload, length);
```

回调：`on_media_ctrl_msg(conn_id, uid, payload, length)`。

### 数据流（Data Stream）

创建有序/可靠的数据流，用于结构化消息传输（最大 1KB/条，60 PPS，6 Kbps）。

```c
int stream_id;
agora_rtc_create_data_stream(conn_id, &stream_id, true /* reliable */, true /* ordered */);
agora_rtc_send_stream_message(conn_id, stream_id, data, length);
```

回调：`on_stream_message(conn_id, uid, stream_id, data, length, sent_ts)`。

### 工具 API

| API | 说明 |
|-----|------|
| `agora_rtc_get_version()` | 获取 SDK 版本号 |
| `agora_rtc_err_2_str(err)` | 将错误码转为可读字符串 |
| `agora_rtc_set_log_level(level)` | 设置日志级别（RTC_LOG_DEBUG ~ RTC_LOG_EMERG） |
| `agora_rtc_config_log(size, count)` | 配置日志文件大小和滚动数量 |
| `agora_rtc_renew_token(conn_id, token)` | Token 过期前续期 |
| `agora_rtc_set_params(conn_id, json)` | 通过 JSON 字符串设置私有参数 |
| `agora_rtc_get_connection_info(conn_id, info)` | 获取连接信息（uid、channel_name） |

### 事件回调汇总

在调用 `agora_rtc_init()` 之前，在 `agora_rtc_event_handler_t` 中注册回调：

| 回调 | 触发时机 |
|------|----------|
| `on_join_channel_success` | 成功加入频道（可以开始发送数据） |
| `on_connection_lost` | 与服务器断开超过 10 秒 |
| `on_reconnecting` | 连接中断，SDK 正在重连 |
| `on_rejoin_channel_success` | 断线重连成功 |
| `on_user_joined` | 远端用户加入频道 |
| `on_user_offline` | 远端用户离开（主动退出/超时掉线/变为观众） |
| `on_user_mute_audio` | 远端用户静音/取消静音 |
| `on_user_mute_video` | 远端用户停止/恢复视频 |
| `on_audio_data` | 收到远端用户的音频帧 |
| `on_mixed_audio_data` | 收到混音后的音频（需启用 audio_mixer） |
| `on_video_data` | 收到远端用户的视频帧 |
| `on_target_bitrate_changed` | 网络带宽变化，需调整编码器码率 |
| `on_key_frame_gen_req` | 远端请求关键帧（检测到丢包） |
| `on_token_privilege_will_expire` | Token 即将过期，需调用 `renew_token` |
| `on_media_ctrl_msg` | 收到 RTCM 消息 |
| `on_rdt_state` | RDT 通道状态变化 |
| `on_rdt_msg` | 收到 RDT 消息 |
| `on_stream_message` | 收到数据流消息 |
| `on_error` | 运行时错误 |
| `on_rtc_stats` | 定期连接统计信息 |

### 错误码

所有 API 返回 `int`：`0` = 成功，`< 0` = 失败。使用 `agora_rtc_err_2_str(err)` 获取错误信息。

常用错误码（`agora_err_code_e`）：

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0 | `ERR_OKAY` | 成功 |
| 1 | `ERR_FAILED` | 通用错误 |
| 2 | `ERR_INVALID_PARAM` | 参数无效 |
| 3 | `ERR_INVALID_STATE` | 状态无效（如静音时发送） |
| 7 | `ERR_NOT_INITIALIZED` | SDK 未初始化 |
| 101 | `ERR_INVALID_APP_ID` | App ID 无效 |
| 109 | `ERR_TOKEN_EXPIRED` | Token 已过期 |
| 110 | `ERR_INVALID_TOKEN` | Token 无效 |
| 300 | `ERR_VIDEO_SEND_OVER_BANDWIDTH_LIMIT` | 视频发送速度过快 |

## 运行示例

编译产物位于 `out/{ARCH}/` 目录下（如 `out/x86_64/`、`out/aarch64/`），测试媒体文件位于 `test_data/` 目录下。`hello_rtsa` 必须从 `out/` 目录启动，这样默认的 `../test_data/...` 路径才能正确生效。

### hello_rtsa — 音视频流传输

默认文件：视频=`send_video.h264`，音频=`send_audio_16k_1ch.pcm`（PCM 16kHz 单声道）。
当音频类型为 PCM 且编码器为 G.711 时，默认使用 `send_audio_8k_1ch.pcm`。
当未显式指定文件路径时，`hello_rtsa` 固定使用 `../test_data/` 下的相对路径，因此必须在 `out/` 目录下运行。

参数列表：

| 短参 | 长参 | 说明 |
|------|------|------|
| `-i` | `--app-id` | App ID（必填） |
| `-t` | `--token` | 鉴权 Token |
| `-c` | `--channel-id` | 频道名，默认 `hello_demo` |
| `-u` | `--user-id` | 用户 ID，默认 0 |
| `-U` | `--user-name` | 用户名（字符串 UID） |
| `-l` | `--license` | License 值 |
| `-L` | `--log-level` | 日志级别，-1 禁用日志 |
| `-v` | `--video-type` | 视频类型：2=H264, 3=H265, 20=JPEG |
| `-a` | `--audio-type` | 音频类型：3=PCMA 4=PCMU 5=G722 6=AACLC-8K 7=AACLC-16K 8=AACLC-48K 9=HEAAC 100=PCM |
| `-C` | `--audio-codec` | 音频编码器（仅 PCM）：0=禁用 1=OPUS 2=G722 3=G711A 4=G711U |
| `-f` | `--fps` | 视频帧率，默认 25 |
| `-s` | `--send-video-file` | 发送视频文件路径 |
| `-S` | `--send-audio-file` | 发送音频文件路径 |
| `-A` | `--area` | 区域码（十六进制），默认 0xFFFFFFFF（全球） |
| `-j` | `--audio-jitter-buffer` | 启用音频抖动缓冲 |
| `-m` | `--audio-mixer` | 启用音频混音 |
| `-d` | `--audio-decode` | 启用音频解码 |
| `-R` | `--recv-only` | 仅接收，不发送 |
| `-D` | `--domain-limit` | 启用域名限制 |
| | `--pcm-sample-rate` | PCM 采样率 |
| | `--pcm-channel-num` | PCM 声道数 |
| | `--pcm-duration` | PCM 帧时长（ms），默认 20 |
| | `--lan-accelerate` | 启用局域网加速 |
| | `--audio-ai-qos` | 启用音频 AI QoS |

```bash
# 长参形式
cd out
./{ARCH}/hello_rtsa --app-id <APPID> --channel-id <CHANNEL> --user-id <UID> \
    --send-video-file ../test_data/send_video.h264 --send-audio-file ../test_data/send_audio_16k_1ch.pcm

# 短参形式（等价）
cd out
./{ARCH}/hello_rtsa -i <APPID> -c <CHANNEL> -u <UID> \
    -s ../test_data/send_video.h264 -S ../test_data/send_audio_16k_1ch.pcm

# 仅接收
cd out
./{ARCH}/hello_rtsa -i <APPID> -c <CHANNEL> -u <UID> -R

# 发送 H.265 视频 + PCM/Opus 音频（使用默认音频文件）
cd out
./{ARCH}/hello_rtsa -i <APPID> -c <CHANNEL> -v 3 -a 100 -C 1 \
    -s ../test_data/send_video.h265
```

### hello_rtm — RTM 消息通道

默认发送大小：1024 字节，默认发送速率：80 kbps（10 条/秒）。

参数列表：

| 短参 | 长参 | 说明 |
|------|------|------|
| `-i` | `--appId` | App ID（必填） |
| `-t` | `--token` | 鉴权 Token |
| `-l` | `--license` | License 值 |
| `-L` | `--log` | SDK 日志目录 |
| `-u` | `--rtmUid` | 自身 RTM UID（必填） |
| `-p` | `--peerUid` | 对端 RTM UID（必填） |
| `-M` | `--linkMode` | 链路模式：1=TCP 2=TCP_TLS 3=AUT 4=AUT_TLS |
| `-T` | `--testMode` | 测试模式：1=仅接收, 2=收发 |
| `-S` | `--sendSize` | 发送数据大小（字节），默认 1024 |
| `-K` | `--sendKbps` | 发送码率（kbps），默认 80 |
| `-C` | `--sendCnts` | 发送次数，默认 -1（无限） |

```bash
# 长参形式 — 交互式文本聊天
./out/{ARCH}/hello_rtm --appId <APPID> --rtmUid <SELF_UID> --peerUid <PEER_UID>

# 短参形式（等价）
./out/{ARCH}/hello_rtm -i <APPID> -u <SELF_UID> -p <PEER_UID>

# 吞吐量测试
./out/{ARCH}/hello_rtm -i <APPID> -u <SELF_UID> -p <PEER_UID> -T 2 -S 1024 -K 100
```

### hello_rdt — 可靠数据传输

默认频道：`demo`，默认发送文件：`hello_rdt`，默认码率：1000 kbps。

参数列表：

| 短参 | 长参 | 说明 |
|------|------|------|
| `-i` | `--app-id` | App ID（必填） |
| `-t` | `--token` | 鉴权 Token |
| `-c` | `--channel-id` | 频道名，默认 `demo` |
| `-u` | `--local-id` | 本地 UID（必填） |
| `-p` | `--peer-id` | 对端 UID（必填） |
| `-s` | `--send` | 启用发送模式 |
| `-k` | `--send-kbps` | 发送码率（kbps），默认 1000 |
| `-f` | `--send-file` | 发送文件路径，默认 `hello_rdt` |
| `-r` | `--send-repeat` | 文件发送完毕后重复发送 |
| `-l` | `--log-dir` | SDK 日志目录 |
| `-L` | `--log-level` | 日志级别（1-8） |
| | `--crypto-mode` | 加密模式 |
| | `--crypto-key` | 加密密钥 |
| | `--crypto-salt` | 加密盐值 |
| | `--lan-accelerate` | 启用局域网加速 |

```bash
# 长参形式 — 发送文件
./out/{ARCH}/hello_rdt --app-id <APPID> --channel-id <CHANNEL> \
    --local-id <UID> --peer-id <PEER_UID> --send --send-file <FILE_PATH>

# 短参形式（等价）
./out/{ARCH}/hello_rdt -i <APPID> -c <CHANNEL> -u <UID> -p <PEER_UID> -s -f <FILE_PATH>

# 接收模式
./out/{ARCH}/hello_rdt -i <APPID> -c <CHANNEL> -u <UID> -p <PEER_UID>
```

### hello_rtcm — 媒体控制消息

默认频道：`demo`，默认消息内容：`hello agora`。

参数列表：

| 短参 | 长参 | 说明 |
|------|------|------|
| `-i` | `--app-id` | App ID（必填） |
| `-t` | `--token` | 鉴权 Token |
| `-c` | `--channel-id` | 频道名，默认 `demo` |
| `-u` | `--local-id` | 本地 UID（必填） |
| `-p` | `--peer-id` | 对端 UID（必填） |
| `-s` | `--send` | 发送消息内容 |
| `-r` | `--receive` | 启用接收 |
| `-l` | `--log-dir` | SDK 日志目录 |

```bash
# 长参形式
./out/{ARCH}/hello_rtcm --app-id <APPID> --channel-id <CHANNEL> \
    --local-id <UID> --peer-id <PEER_UID> --send "hello agora"

# 短参形式（等价）
./out/{ARCH}/hello_rtcm -i <APPID> -c <CHANNEL> -u <UID> -p <PEER_UID> -s "hello agora"
```

### hello_stream_message — 数据流

默认发送大小：4 字节，默认 PPS：10。

参数列表：

| 短参 | 长参 | 说明 |
|------|------|------|
| `-i` | `--app-id` | App ID（必填） |
| `-t` | `--token` | 鉴权 Token |
| `-c` | `--channel-id` | 频道名（必填） |
| `-u` | `--user-id` | 用户 ID，默认 0 |
| `-L` | `--log-level` | 日志级别，-1 禁用日志 |
| `-R` | `--recv-only` | 仅接收 |
| | `--ordered` | 创建有序数据流 |
| | `--reliable` | 创建可靠数据流 |
| | `--log_path` | 日志路径 |
| | `--send_size` | 消息大小（字节），默认 4 |
| | `--pps` | 每秒发送包数，默认 10 |

```bash
# 长参形式
./out/{ARCH}/hello_stream_message --app-id <APPID> --channel-id <CHANNEL> --user-id <UID> \
    --ordered --reliable --send_size 64 --pps 10

# 短参形式
./out/{ARCH}/hello_stream_message -i <APPID> -c <CHANNEL> -u <UID>

# 仅接收
./out/{ARCH}/hello_stream_message -i <APPID> -c <CHANNEL> -u <UID> -R
```

## 基于 Demo 开发自己的应用

1. 复制 `hello_rtsa/` 作为项目模板
2. 包含 `agora_sdk/include/` 下的 `agora_rtc_api.h` 头文件
3. 链接 `libagora-rtc-sdk.so`（静态链接则使用 `.a`）
4. 遵循 SDK 生命周期：init → create_connection → join → 发送/接收 → leave → destroy → fini
5. 根据需要的功能注册对应的事件回调（音视频、RTM、RDT、RTCM、数据流）
6. 使用 `test_data/` 目录下的测试媒体文件进行初始测试

### 快速上手代码

```c
#include "agora_rtc_api.h"

int main() {
    // 初始化
    agora_rtc_event_handler_t h = { 0 };
    h.on_join_channel_success = on_join;
    h.on_audio_data = on_audio;
    h.on_video_data = on_video;
    h.on_error = on_error;

    rtc_service_option_t opt = { 0 };
    opt.area_code = AREA_CODE_GLOB;
    agora_rtc_init("YOUR_APP_ID", &h, &opt);

    // 创建连接并加入频道
    connection_id_t conn;
    agora_rtc_create_connection(&conn);

    rtc_channel_options_t ch = { 0 };
    ch.auto_subscribe_audio = true;
    ch.auto_subscribe_video = true;
    agora_rtc_join_channel(conn, "test_channel", 0, NULL, &ch);

    // ... 等待 on_join_channel_success 回调，然后在循环中发送/接收 ...

    // 清理
    agora_rtc_leave_channel(conn);
    agora_rtc_destroy_connection(conn);
    agora_rtc_fini();
    return 0;
}
```
