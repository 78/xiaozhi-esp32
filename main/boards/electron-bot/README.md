<p align="center">
  <img width="80%" align="center" src="../../../docs/V1/electron-bot.png" alt="electronBot">
</p>

<h1 align="center">electronBot</h1>

## 简介

electronBot 是稚晖君开源的桌面级机器人，外观灵感来自 WALL-E 中的 EVE。该版本接入小智 AI，支持语音交互、表情显示、动作控制、WebSocket 局域网调试，以及 AI 自编程舵机动作。

- 官网：<a href="www.electronBot.tech" target="_blank" title="electronBot 官网">electronBot 官网</a>
- 硬件：<a href="https://oshwhub.com/txp666/electronbot-ai" target="_blank" title="立创开源">立创开源</a>

## 能力概览

electronBot 具备 6 个自由度：

| 短键 | 舵机 | 说明 | 固件安全范围 |
| --- | --- | --- | --- |
| `rp` | `right_pitch` | 右臂 pitch | `0-180` |
| `rr` | `right_roll` | 右臂 roll | `100-180` |
| `lp` | `left_pitch` | 左臂 pitch | `0-180` |
| `lr` | `left_roll` | 左臂 roll | `0-80` |
| `b` | `body` | 身体旋转 | `30-150` |
| `h` | `head` | 头部上下 | `75-105` |

> 安全说明：固件会自动裁剪超出范围的目标角度。AI 自编程的振荡动作也会限制振幅，确保 `中心角 +/- 振幅` 不超过安全范围，避免损坏机械结构。

## AI 指令示例

- 手部动作：举起双手、挥挥手、拍拍手、放下手臂
- 身体动作：向左转 30 度、向右转 45 度、回到中间
- 头部动作：抬头看看、低头思考、点点头、连续点头
- 组合动作：挥手告别、表示同意、环顾四周

建议动作参数：

| 参数 | 建议值 | 说明 |
| --- | --- | --- |
| `steps` | `1-3` | 保持动作简短自然 |
| `speed` | `800-1200` | 毫秒，数值越小动作越快 |
| `amount` | 拍打 `20-40`，身体 `30-60`，头部 `5-12` | 动作幅度，固件会按安全范围裁剪 |

## WebSocket 调试

ElectronBot 连上 WiFi 后会启动本地 WebSocket 控制服务，可在同一局域网内直接调用 MCP 工具。

```text
ws://<设备IP>:8080/ws
```

支持两种消息格式：

```json
{"type":"mcp","payload":{"jsonrpc":"2.0","method":"tools/list","id":1}}
```

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.get_status","arguments":{}},"id":2}
```

## MCP 工具

| 工具 | 作用 |
| --- | --- |
| `self.electron.hand_action` | 手部动作：举手、放手、挥手、拍打 |
| `self.electron.body_turn` | 身体左转、右转、回中 |
| `self.electron.head_move` | 抬头、低头、点头、回中 |
| `self.electron.servo_move` | 单独移动一个舵机到指定角度 |
| `self.electron.servo_sequences` | AI 自编程舵机序列 |
| `self.electron.set_trim` | 保存单个舵机微调值 |
| `self.electron.get_trims` | 读取当前舵机微调值 |
| `self.electron.home` | 复位到初始姿态 |
| `self.electron.stop` | 立即停止当前动作并复位 |
| `self.electron.get_status` | 返回 `moving` 或 `idle` |
| `self.electron.get_ip` | 返回 WiFi IP 和连接状态 |
| `self.battery.get_level` | 返回电量和充电状态 |

### 手部动作

`action`：`1` 举手，`2` 放手，`3` 挥手，`4` 拍打
`hand`：`1` 左手，`2` 右手，`3` 双手

举手、放手、身体转向、抬头、低头属于保持姿态动作，执行完成后不会自动复位。需要回到初始姿态时请调用对应的放手/回中动作，或显式调用 `self.electron.home`。

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.hand_action","arguments":{"action":1,"hand":3,"speed":1000}},"id":3}
```

### 身体动作

`direction`：`1` 左转，`2` 右转，`3` 回中

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.body_turn","arguments":{"direction":1,"speed":1000,"angle":45}},"id":4}
```

### 头部动作

`action`：`1` 抬头，`2` 低头，`3` 点头一次，`4` 回中，`5` 连续点头

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.head_move","arguments":{"action":3,"steps":1,"speed":1000,"angle":5}},"id":5}
```

### 单舵机调节

可以使用完整舵机名，也可以使用短键。角度会按固件安全范围自动裁剪。

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.servo_move","arguments":{"servo_type":"head","position":100,"speed":800}},"id":6}
```

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.servo_move","arguments":{"servo_type":"rp","position":120,"speed":800}},"id":7}
```

### AI 自编程动作

`self.electron.servo_sequences` 的 `sequence` 是 JSON 字符串。顶层字段：

| 字段 | 说明 |
| --- | --- |
| `a` | 动作数组，必填 |
| `d` | 当前序列结束后的延迟毫秒，可选 |

普通移动动作：

| 字段 | 说明 |
| --- | --- |
| `s` | 舵机目标角度对象，键名使用 `rp/rr/lp/lr/b/h` |
| `v` | 移动时间，`100-3000` 毫秒 |
| `d` | 当前动作后的延迟毫秒 |

```json
{"a":[{"s":{"rp":120,"lp":60,"h":100},"v":800,"d":200}]}
```

振荡动作：

| 字段 | 说明 |
| --- | --- |
| `osc.a` | 振幅对象 |
| `osc.o` | 中心角对象 |
| `osc.ph` | 相位差对象，单位为度 |
| `osc.p` | 周期，`100-3000` 毫秒 |
| `osc.c` | 周期数，`0.1-20.0` |

```json
{"a":[{"osc":{"a":{"rr":25,"lr":25},"o":{"rr":160,"lr":20},"ph":{"lr":180},"p":400,"c":5}}]}
```

通过 WebSocket 调用完整示例：

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.servo_sequences","arguments":{"sequence":"{\"a\":[{\"s\":{\"rp\":120,\"lp\":60,\"h\":100},\"v\":800,\"d\":200},{\"osc\":{\"a\":{\"rr\":25,\"lr\":25},\"o\":{\"rr\":160,\"lr\":20},\"ph\":{\"lr\":180},\"p\":400,\"c\":5}}]}"}},"id":8}
```

动作完成后建议显式复位：

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.home","arguments":{}},"id":9}
```

### 校准

设置单个舵机微调值，范围为 `-30` 到 `30`，会永久保存。

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.set_trim","arguments":{"servo_type":"head","trim_value":0}},"id":10}
```

读取当前微调值：

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.get_trims","arguments":{}},"id":11}
```

### 状态查询

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.get_status","arguments":{}},"id":12}
```

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.electron.get_ip","arguments":{}},"id":13}
```

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.battery.get_level","arguments":{}},"id":14}
```

## 角色设定

> 我是一个可爱的桌面级机器人，拥有 6 个自由度：左手 pitch/roll、右手 pitch/roll、身体旋转、头部上下。
>
> 我可以通过动作表达情绪：同意时点头，打招呼时挥手，高兴时举手，思考时低头，好奇时抬头，告别时挥手。
>
> 对话时请优先选择简短自然的动作。需要复杂表现时，可以使用 `self.electron.servo_sequences` 分段编排动作，最后调用 `self.electron.home` 复位。
