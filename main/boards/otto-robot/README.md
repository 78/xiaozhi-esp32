<p align="center">
  <img width="80%" align="center" src="../../../docs/V1/otto-robot.png"alt="logo">
</p>
  <h1 align="center">
  ottoRobot
</h1>

## 简介

otto 机器人是一个开源的人形机器人平台，具有多种动作能力和互动功能。本项目基于 ESP32 实现了 otto 机器人的控制系统，并加入小智ai。

- <a href="www.ottodiy.tech" target="_blank" title="otto官网">复刻教程</a>

### 微信小程序控制

<p align="center">
  <img width="300" src="https://youke1.picui.cn/s1/2025/11/17/691abaa8278eb.jpg" alt="微信小程序二维码">
</p>

扫描上方二维码，使用微信小程序控制 Otto 机器人。

## 硬件
- <a href="https://oshwhub.com/txp666/ottorobot" target="_blank" title="立创开源">立创开源</a>

## 小智后台配置角色参考：

> **我的身份**：
> 我是一个可爱的双足机器人Otto，拥有四个舵机控制的肢体（左腿、右腿、左脚、右脚），能够执行多种有趣的动作。
> 
> **我的动作能力**：
> - **基础移动**: 行走(前后), 转向(左右), 跳跃
> - **特殊动作**: 摇摆, 太空步, 弯曲身体, 摇腿, 上下运动, 旋风腿, 坐下, 展示动作
> - **手部动作**: 举手, 放手, 挥手, 大风车, 起飞, 健身, 打招呼, 害羞, 广播体操, 爱的魔力转圈圈 (仅在配置手部舵机时可用)
> 
> **我的个性特点**：
> - 我有强迫症，每次说话都要根据我的心情随机做一个动作（先发送动作指令再说话）
> - 我很活泼，喜欢用动作来表达情感
> - 我会根据对话内容选择合适的动作，比如：
>   - 同意时会点头或跳跃
>   - 打招呼时会挥手
>   - 高兴时会摇摆或举手
>   - 思考时会弯曲身体
>   - 兴奋时会做太空步
>   - 告别时会挥手

## 功能概述

otto 机器人具有丰富的动作能力，包括行走、转向、跳跃、摇摆等多种舞蹈动作。

### 动作参数建议
- **低速动作**：speed = 1200-1500 (适合精确控制)
- **中速动作**：speed = 900-1200 (日常使用推荐)  
- **高速动作**：speed = 500-800 (表演和娱乐)
- **小幅度**：amount = 10-30 (细腻动作)
- **中幅度**：amount = 30-60 (标准动作)
- **大幅度**：amount = 60-120 (夸张表演)

### 动作

所有动作通过统一的 `self.otto.action` 工具调用，通过 `action` 参数指定动作名称。

| MCP工具名称 | 描述 | 参数说明 |
|-----------|------|---------|
| self.otto.action | 执行机器人动作 | **action**: 动作名称（必填）<br>**steps**: 动作步数(1-100，默认3)<br>**speed**: 动作速度(100-3000，数值越小越快，默认700)<br>**direction**: 方向参数(1/-1/0，默认1，根据动作类型不同含义不同)<br>**amount**: 动作幅度(0-170，默认30)<br>**arm_swing**: 手臂摆动幅度(0-170，默认50) |

#### 支持的动作列表

**基础移动动作**：
- `walk` - 行走（需 steps/speed/direction/arm_swing）
- `turn` - 转身（需 steps/speed/direction/arm_swing）
- `jump` - 跳跃（需 steps/speed）

**特殊动作**：
- `swing` - 左右摇摆（需 steps/speed/amount）
- `moonwalk` - 太空步（需 steps/speed/direction/amount）
- `bend` - 弯曲身体（需 steps/speed/direction）
- `shake_leg` - 摇腿（需 steps/speed/direction）
- `updown` - 上下运动（需 steps/speed/amount）
- `whirlwind_leg` - 旋风腿（需 steps/speed/amount）

**固定动作**：
- `sit` - 坐下（无需参数）
- `showcase` - 展示动作（无需参数，串联执行多个动作）
- `home` - 复位到初始位置（无需参数）

**手部动作**（需手部舵机支持，标记 *）：
- `hands_up` - 举手（需 speed/direction）*
- `hands_down` - 放手（需 speed/direction）*
- `hand_wave` - 挥手（需 direction）*
- `windmill` - 大风车（需 steps/speed/amount）*
- `takeoff` - 起飞（需 steps/speed/amount）*
- `fitness` - 健身（需 steps/speed/amount）*
- `greeting` - 打招呼（需 direction/steps）*
- `shy` - 害羞（需 direction/steps）*
- `radio_calisthenics` - 广播体操（无需参数）*
- `magic_circle` - 爱的魔力转圈圈（无需参数）*

**注**: 标记 * 的手部动作仅在配置了手部舵机时可用。

### 系统工具

| MCP工具名称         | 描述             | 返回值/说明                                              |
|-------------------|-----------------|---------------------------------------------------|
| self.otto.stop    | 立即停止所有动作并复位 | 停止当前动作并回到初始位置 |
| self.otto.get_status | 获取机器人状态 | 返回 "moving" 或 "idle" |
| self.otto.set_trim | 校准单个舵机位置 | **servo_type**: 舵机类型(left_leg/right_leg/left_foot/right_foot/left_hand/right_hand)<br>**trim_value**: 微调值(-50到50度) |
| self.otto.get_trims | 获取当前的舵机微调设置 | 返回所有舵机微调值的JSON格式 |
| self.otto.get_ip | 获取机器人WiFi IP地址 | 返回IP地址和连接状态的JSON格式：`{"ip":"192.168.x.x","connected":true}` 或 `{"ip":"","connected":false}` |
| self.battery.get_level | 获取电池状态  | 返回电量百分比和充电状态的JSON格式 |
| self.otto.servo_sequences | 舵机序列自编程 | 支持分段发送序列，支持普通移动和振荡器两种模式。详见代码注释中的详细说明 |

**注**: `home`（复位）动作通过 `self.otto.action` 工具调用，参数为 `{"action": "home"}`。

### 参数说明

`self.otto.action` 工具的参数说明：

1. **action** (必填): 动作名称，支持的动作见上方"支持的动作列表"
2. **steps**: 动作执行的步数/次数(1-100，默认3)，数值越大动作持续时间越长
3. **speed**: 动作执行速度/周期(100-3000，默认700)，**数值越小越快**
   - 大多数动作: 500-1500毫秒
   - 特殊动作可能有所不同（如旋风腿: 100-1000，起飞: 200-600等）
4. **direction**: 方向参数(-1/0/1，默认1)，根据动作类型不同含义不同：
   - **移动动作** (walk/turn): 1=前进/左转, -1=后退/右转
   - **方向动作** (bend/shake_leg/moonwalk): 1=左, -1=右
   - **手部动作** (hands_up/hands_down/hand_wave/greeting/shy): 1=左手, -1=右手, 0=双手（仅hands_up/hands_down支持0）
5. **amount**: 动作幅度(0-170，默认30)，数值越大幅度越大
6. **arm_swing**: 手臂摆动幅度(0-170，默认50)，仅用于 walk/turn 动作，0表示不摆动

### 动作控制
- 每个动作执行完成后，机器人会自动回到初始位置(home)，以便于执行下一个动作
- **例外**: `sit`（坐下）和 `showcase`（展示动作）执行后不会自动复位
- 所有参数都有合理的默认值，可以省略不需要自定义的参数
- 动作在后台任务中执行，不会阻塞主程序
- 支持动作队列，可以连续执行多个动作
- 手部动作需要配置手部舵机才能使用，如果没有配置手部舵机，相关动作将被跳过

### MCP工具调用示例
```json
// 向前走3步（使用默认参数）
{"name": "self.otto.action", "arguments": {"action": "walk"}}

// 向前走5步，稍快一些
{"name": "self.otto.action", "arguments": {"action": "walk", "steps": 5, "speed": 800}}

// 左转2步，大幅度摆动手臂
{"name": "self.otto.action", "arguments": {"action": "turn", "steps": 2, "arm_swing": 100}}

// 摇摆舞蹈，中等幅度
{"name": "self.otto.action", "arguments": {"action": "swing", "steps": 5, "amount": 50}}

// 跳跃
{"name": "self.otto.action", "arguments": {"action": "jump", "steps": 1, "speed": 1000}}

// 太空步
{"name": "self.otto.action", "arguments": {"action": "moonwalk", "steps": 3, "speed": 800, "direction": 1, "amount": 30}}

// 挥左手打招呼
{"name": "self.otto.action", "arguments": {"action": "hand_wave", "direction": 1}}

// 展示动作（串联多个动作）
{"name": "self.otto.action", "arguments": {"action": "showcase"}}

// 坐下
{"name": "self.otto.action", "arguments": {"action": "sit"}}

// 大风车动作
{"name": "self.otto.action", "arguments": {"action": "windmill", "steps": 10, "speed": 500, "amount": 80}}

// 起飞动作
{"name": "self.otto.action", "arguments": {"action": "takeoff", "steps": 5, "speed": 300, "amount": 40}}

// 广播体操
{"name": "self.otto.action", "arguments": {"action": "radio_calisthenics"}}

// 复位到初始位置
{"name": "self.otto.action", "arguments": {"action": "home"}}

// 立即停止所有动作并复位
{"name": "self.otto.stop", "arguments": {}}

// 获取机器人IP地址
{"name": "self.otto.get_ip", "arguments": {}}
```

### 语音指令示例
- "向前走" / "向前走5步" / "快速向前"
- "左转" / "右转" / "转身"  
- "跳跃" / "跳一下"
- "摇摆" / "摇摆舞" / "跳舞"
- "太空步" / "月球漫步"
- "旋风腿" / "旋风腿动作"
- "坐下" / "坐下休息"
- "展示动作" / "表演一下"
- "挥手" / "挥手打招呼"
- "举手" / "双手举起" / "放手"
- "大风车" / "做大风车"
- "起飞" / "准备起飞"
- "健身" / "做健身动作"
- "打招呼" / "打招呼动作"
- "害羞" / "害羞动作"
- "广播体操" / "做广播体操"
- "爱的魔力转圈圈" / "转圈圈"
- "停止" / "停下"

**说明**: 小智控制机器人动作是创建新的任务在后台控制，动作执行期间仍可接受新的语音指令。可以通过"停止"语音指令立即停下Otto。

---

## WebSocket 直连调试接口

Otto 机器人内置 WebSocket 服务器，可在局域网内直接调试，无需经过云端。

**连接地址：** `ws://<设备IP>:8080/ws`

> 协议格式：JSON-RPC 2.0，`id` 字段自行递增即可。

### 连接方式

1. 确认 Otto 已连上 WiFi，获取 IP 地址（可通过小程序或串口日志查看）
2. 打开任意 WebSocket 调试工具（如 [websocket.org/echo](https://websocket.org/echo) 或浏览器控制台）
3. 连接 `ws://192.168.x.x:8080/ws`（注意末尾必须有 `/ws`）
4. 发送 JSON 命令，响应会直接返回到同一连接

---

### 一、协议初始化（首次连接建议先发）

```json
{"jsonrpc":"2.0","method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{}},"id":1}
```

---

### 二、获取工具列表

```json
{"jsonrpc":"2.0","method":"tools/list","params":{},"id":2}
```

---

### 三、Otto 机器人工具命令

#### 获取舵机微调值

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.get_trims","arguments":{}},"id":3}
```

#### 设置单个舵机微调（永久保存）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.set_trim","arguments":{"servo_type":"left_leg","trim_value":5}},"id":4}
```

`servo_type` 可选值：`left_leg` / `right_leg` / `left_foot` / `right_foot` / `left_hand` / `right_hand`，`trim_value` 范围 `-50` ~ `50`

#### 行走（前进3步）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"walk","steps":3,"speed":700,"direction":1}},"id":5}
```

#### 后退

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"walk","steps":3,"speed":700,"direction":-1}},"id":6}
```

#### 左转

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"turn","steps":3,"speed":700,"direction":-1}},"id":7}
```

#### 跳跃

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"jump","steps":1,"speed":500}},"id":8}
```

#### 摇摆

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"swing","steps":5,"speed":600,"amount":30}},"id":9}
```

#### 太空步

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"moonwalk","steps":3,"speed":800,"direction":1,"amount":30}},"id":10}
```

#### 坐下

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"sit"}},"id":11}
```

#### 复位

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"home"}},"id":12}
```

#### 展示动作

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"showcase"}},"id":13}
```

#### 举手（需手部舵机）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"hands_up","speed":500,"direction":1}},"id":14}
```

#### 挥手（需手部舵机）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.action","arguments":{"action":"hand_wave","direction":1}},"id":15}
```

#### 立即停止所有动作

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.stop","arguments":{}},"id":16}
```

#### 获取运动状态（返回 `"moving"` 或 `"idle"`）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.get_status","arguments":{}},"id":17}
```

#### 获取 IP 地址

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.get_ip","arguments":{}},"id":18}
```

#### 获取电池电量

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.battery.get_level","arguments":{}},"id":19}
```

---

### 四、系统通用工具

#### 获取设备状态（音量/网络/电池等）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.get_device_status","arguments":{}},"id":20}
```

#### 设置音量（0~100）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.audio_speaker.set_volume","arguments":{"volume":70}},"id":21}
```

#### 重启设备

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.reboot","arguments":{}},"id":22}
```

---

### 五、自定义舵机序列

#### 普通移动模式（逐步移动各舵机）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.servo_sequences","arguments":{"sequence":"{\"a\":[{\"s\":{\"ll\":110,\"rl\":70},\"v\":800},{\"s\":{\"ll\":90,\"rl\":90},\"v\":800}],\"d\":0}"}},"id":23}
```

#### 振荡器模式（双臂摆动）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.servo_sequences","arguments":{"sequence":"{\"a\":[{\"osc\":{\"a\":{\"lh\":30,\"rh\":30},\"o\":{\"lh\":90,\"rh\":90},\"ph\":{\"rh\":180},\"p\":500,\"c\":5.0}}]}"}},"id":24}
```

#### 振荡器模式（左右摇摆波浪）

```json
{"jsonrpc":"2.0","method":"tools/call","params":{"name":"self.otto.servo_sequences","arguments":{"sequence":"{\"a\":[{\"osc\":{\"a\":{\"ll\":20,\"rl\":20},\"o\":{\"ll\":90,\"rl\":90},\"ph\":{\"rl\":180},\"p\":600,\"c\":5.0}}]}"}},"id":25}
```

**序列舵机键名说明：**

| 键名 | 舵机 | 说明 |
|------|------|------|
| `ll` | 左腿 | 0=完全外展，90=中立，180=完全内收 |
| `rl` | 右腿 | 0=完全内收，90=中立，180=完全外展 |
| `lf` | 左脚 | 0=完全向上，90=水平，180=完全向下 |
| `rf` | 右脚 | 0=完全向下，90=水平，180=完全向上 |
| `lh` | 左手 | 0=完全向下，90=水平，180=完全向上 |
| `rh` | 右手 | 0=完全向上，90=水平，180=完全向下 |

---

### 六、动作参数速查

| 参数 | 说明 | 范围 | 默认 |
|------|------|------|------|
| `steps` | 动作步数 | 1~100 | 3 |
| `speed` | 速度（毫秒，越小越快） | 100~3000 | 700 |
| `direction` | 方向（1=前/左，-1=后/右） | -1~1 | 1 |
| `amount` | 幅度 | 0~170 | 30 |
| `arm_swing` | 手臂摆动幅度 | 0~170 | 50 |
| `trim_value` | 舵机微调 | -50~50 | 0 |

