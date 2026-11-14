# 阅读样式（Reading message style）

适合小尺寸 LCD 的显示样式：表情放在状态栏，屏幕其余部分显示完整的对话，回答边说边能读完，而不是底部一行横滚。

- 打开：`CONFIG_USE_READING_MESSAGE_STYLE=y`（显示样式里与默认、微信样式并列）。
- 演示：[`reading-message-style/demo.html`](reading-message-style/demo.html)（单个 HTML 文件，浏览器直接打开，八个场景可以播放、暂停、切换逐字 / 按句）。
- 首个使用的板子：FoloToy AI Passport（ESP32-C3，240×320，无 PSRAM）。

## 布局

| 区域 | 内容 |
|---|---|
| 状态栏 | 表情、状态文字（或临时通知，如"音量 85"）、网络和电量图标；底色随状态变化 |
| 正文 | 按"一问一答"分轮显示：问题灰色，回答黑色；新的一轮从新的一页开始；更早的轮次变灰 |
| 尾随行 | "聆听中..."、等待回答的"..."、系统提示（激活、错误）都放在最新一轮的末尾 |
| 右下角 ▼ | 往回翻看时出现，表示下面还有新内容 |

状态栏底色：待命 / 连接中 灰，聆听 蓝，等待回答 黄，说话 绿。颜色由主题色混合得到，浅色、深色主题都适用。

## 不同状态下怎么变

| 事件 | 正文 | 状态栏 | 动图 |
|---|---|---|---|
| 开始新对话（连接中） | 清空 | 连接中... | [一问一答](reading-message-style/01-answer.gif) |
| 聆听 | 末尾一行"聆听中..." | 聆听中...，蓝色 | 同上 |
| 收到问题（stt） | 问题上屏，下面是"..." | 请稍候...，2 秒起计秒，黄色 | [长时间思考](reading-message-style/04-waiting.gif) |
| 收到回答的一句（tts sentence_start） | 接在回答后面，不覆盖；还没读到的部分灰色，读到哪里黑到哪里 | 说话中...，绿色 | [一问一答](reading-message-style/01-answer.gif) |
| 追问 | 整页翻，新问题到页顶，上一轮变灰 | — | [连续追问](reading-message-style/02-follow-up.gif) |
| 说话中被打断 | 没读到的部分保持灰色，后面的句子不再出现 | 回到聆听中... | [中途打断](reading-message-style/03-interrupt.gif) |
| 对话结束（回到待命） | 最后一轮变灰留在屏上，下次对话开始时才清空 | 时间 | [结束与待机](reading-message-style/05-end-and-idle.gif) |
| 网络错误 | 已收到的保留，末尾出现系统提示 | 错误 | [网络断开](reading-message-style/06-network.gif) |
| 翻页 / 调音量（板子按键） | 往回翻时停止自动跟随，翻到底恢复；音量只在状态栏临时显示 | 音量 N | [翻页与音量](reading-message-style/07-paging-volume.gif) |

注意：服务器（例如 xiaozhi.me）可能在第一句文字到达之前就发 `tts start`；等待行和计秒会一直保持到第一句到达。

## 逐字跟读怎么工作

一句的文字比它的声音早到（解码队列最多缓存约 1.2 秒音频），所以点亮跟着**实际播放**走：

1. 收到 `sentence_start` 时给这句分配一个编号，之后收到的音频包都打上编号和句内位置。
2. 音频服务每播放一帧就上报（编号，位置），显示层只记录这个值，不阻塞音频任务；一个 100 ms 的 LVGL 定时器在朗读时读取它。
3. 播放时还不知道一句的音频有多长，所以按每个字的估计时长换算成字数，并用前几句的实际时长校准；300 ms 没有新的帧就当这句读完，朗读结束时整段点亮（被打断时除外）。

`CONFIG_READING_MESSAGE_STYLE_FOLLOW_WORDS`（默认开）选择逐字跟读；关掉则每句音频开始时整句点亮。运行时可以用设备端 MCP 工具 `self.screen.set_subtitle_mode`（`words` / `sentences`）切换，选择保存在 NVS。

## 显示层接口

| 接口 | 作用 | 默认实现 |
|---|---|---|
| `ScrollChat(direction)` | 板子按键翻页；返回是否翻动 | 返回 false |
| `OnDeviceStateChanged(state)` | 每次状态变化后调用，样式据此跟随对话 | 什么都不做 |
| `AddSpokenSentence(id, text)` | 一句带编号的回答 | 当作 assistant 消息显示 |
| `SetSpeechProgress(id, position_ms)` | 播放进度，在音频输出任务里调用，不能阻塞 | 什么都不做 |
| `OnSpeechInterrupted()` | 回答被打断 | 什么都不做 |
| `SetSpeechFollowWords(words)` | 逐字 / 按句 | 返回 false |

## FoloToy AI Passport：开机按键提示

Passport 板子（[`main/boards/folotoy/ai-passport`](../main/boards/folotoy/ai-passport/README.md)）每次启动时在按键旁边标出它们的作用：左边电源键，右边上、下、确定键。提示显示 3 秒，变灰 1 秒后消失；正文出现文字时立刻消失，状态栏文字不影响。还没连上 Wi-Fi 时轻触确定键会进入配网，所以确定键先写“轻触 进入配网”，连上后换成“轻触 开始/结束对话”。见[开机按键提示](reading-message-style/08-key-hints.gif)。

## 限制

- 只保留最近两轮、最多 1800 字节（没有 PSRAM 的板子内存紧）；单个回答更长时删掉最早的句子并在开头显示"..."。
- 问题和回答用同一种文字字体，问题只用灰色区分，不加前缀（避免新增翻译）。
- 翻回旧的一轮时，服务器补发的罕见字字形可能已经被挤出缓存，显示成方框。
