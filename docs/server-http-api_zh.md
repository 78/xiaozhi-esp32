# 服务器 HTTP 接口实现指南

本文档基于固件代码梳理设备与服务器之间所有 HTTP 交互，**面向服务器开发者**，描述各接口的语义、必选/可选字段、设备侧行为预期，便于后端按规范实现。

> 相关文档：
> - [`ota-protocol_zh.md`](ota-protocol_zh.md) — 引导接口的详细字段语义（本文为其超集）
> - [`mqtt-udp_zh.md`](mqtt-udp_zh.md) — MQTT 控制 + UDP 音频协议
> - [`websocket_zh.md`](websocket_zh.md) — WebSocket 二进制帧协议
> - [`mcp-protocol_zh.md`](mcp-protocol_zh.md) — MCP capability / tool 协议

---

## 1. 接口全景

设备在生命周期中会调用 7 类 HTTP 接口：

| 编号 | 接口                  | 方法      | URL 来源                  | 必选？ |
|------|-----------------------|-----------|----------------------------|--------|
| [A]  | 引导（版本 / 配置下发）| POST/GET  | 固件 `CONFIG_OTA_URL`       | **必选** |
| [B]  | 设备激活              | POST      | `<OTA_URL>activate`        | 可选（首次激活才用） |
| [C]  | 固件包下载            | GET       | A 返回的 `firmware.url`    | 可选（有 OTA 升级需求时） |
| [D]  | 资源包下载            | GET       | MCP 工具 `self.assets.set_download_url` 设置 | 可选（按需更新表情包/字体） |
| [E]  | 图像理解              | POST      | MCP capability `vision.url` | 可选（设备有摄像头时） |
| [F]  | 屏幕截图上传          | POST      | LLM 调用 `self.screen.snapshot_to_jpeg` 时传 URL | 可选 |
| [G]  | 图片预览下载          | GET       | LLM 调用 `self.screen.preview_image` 时传 URL  | 可选 |

```
┌───────────────────────────────────────────────────────────────┐
│  设备启动流程                                                  │
├───────────────────────────────────────────────────────────────┤
│                                                               │
│  [A] OTA 引导  ─────►  返回 mqtt/websocket/firmware/activation │
│         │                                                     │
│         ├─►  [B] 激活 (若返回 activation.challenge)            │
│         │                                                     │
│         ├─►  [C] 固件下载 (若 firmware.version 更新)           │
│         │                                                     │
│         └─►  建立 MQTT / WebSocket 长连接                      │
│                                                               │
│  会话期间                                                     │
│         │                                                     │
│         ├─►  [E] 图像理解 (摄像头场景)                        │
│         ├─►  [F] 截图上传 (LLM 工具触发)                      │
│         ├─►  [G] 图片下载 (LLM 工具触发)                      │
│         └─►  [D] 资源包下载 (MCP 工具设置 URL 后下次启动)      │
│                                                               │
└───────────────────────────────────────────────────────────────┘
```

---

## 2. 公共约定

### 2.1 设备身份请求头

所有设备主动发起的请求（[A][B][E][F]）都会带以下身份头，**服务器应据此鉴权 / 路由**：

| Header              | 值来源                          | 出现接口        |
|---------------------|---------------------------------|-----------------|
| `Device-Id`         | `SystemInfo::GetMacAddress()`   | A, B, E         |
| `Client-Id`         | `Board::GetUuid()`              | A, B, E         |
| `Serial-Number`     | efuse `USER_DATA`（如已烧录）   | A, B            |
| `User-Agent`        | `<board-name>/<fw-version>`     | A, B            |
| `Activation-Version`| `"1"` 或 `"2"`                  | A, B            |
| `Accept-Language`   | `zh-CN` / `en-US` 等            | A, B            |
| `Authorization`     | `Bearer <token>`                | E（如有 token） |

#### ⚠️ 术语澄清：三个"client_id"不要混淆

代码和协议里出现了三个名字相似但含义完全不同的标识符，**服务器实现时必须分清**：

| 名字                    | 来源                                       | 用途                                        | 全局唯一性保证                  |
|-------------------------|--------------------------------------------|---------------------------------------------|--------------------------------|
| HTTP `Device-Id` 头     | `SystemInfo::GetMacAddress()` (efuse MAC)  | HTTP 接口的**强主键**，识别物理设备            | ✅ IEEE 分配，出厂烧录，硬件唯一 |
| HTTP `Client-Id` 头     | `Board::GetUuid()`（首次启动本地生成 UUID v4）| HTTP 接口的**辅键**，识别"软件实例"            | ⚠️ 122 bit 随机，理论唯一；首次生成后落 NVS，erase-flash 后会重置 |
| MQTT `client_id`        | OTA 接口下发的 `mqtt.client_id` 字段        | **设备连接 broker 时的 MQTT 协议层 client_id** | ❌ **由服务器自行编码保证**，固件不参与 |

**关键关系**：

```
┌─────────────────────────────────────────────────────────────────┐
│  HTTP Client-Id  ≠  MQTT client_id                              │
│  ─────────────       ──────────────                             │
│  设备本地生成的     vs  OTA 服务器下发的                          │
│  UUID v4                MQTT 连接 ID                            │
│                                                                 │
│  设备身份的强主键是 Device-Id (MAC)，                            │
│  HTTP Client-Id 只是辅助。                                      │
└─────────────────────────────────────────────────────────────────┘
```

**服务器实现规则**：

1. **HTTP 接口鉴权 / 设备识别 → 用 `Device-Id`（MAC）做主键**
   - `Client-Id`（UUID）仅用来区分"同一物理设备 erase-flash 后的不同 NVS 实例"，不要单独用它做主键
   - 仅靠 `Client-Id` 找设备的话，erase-flash 后设备会"消失"
2. **生成 OTA 响应里的 `mqtt.client_id` → 必须包含 MAC 才能保证 broker 上唯一**
   - 推荐编码：`device-<mac-without-colons>` / `<product_key>@@@<mac>` / `<tenant_id>:<mac>` 等
   - **不要**直接把 HTTP `Client-Id` 头（UUID）当 MQTT client_id 下发：理论上能用，但 erase-flash 后 UUID 会变，broker 上变成"新设备"，历史会话/订阅状态丢失
3. **broker 端**靠 MQTT 协议自身的"同名 client_id 后到者踢前者"规则保证连接唯一性 —— 前提是 OTA 服务器下发的 client_id **不撞**

**关于 UUID 的弱熵风险**：`Board::GenerateUuid()` 用 `esp_fill_random()`，但 ESP-IDF 文档明确说"WiFi/BT 未启动前只能产生伪随机数"。多设备首次同时上电时 UUID 理论上有微小碰撞概率。这是另一个**不要把 UUID 当强唯一标识**的理由——`Device-Id`(MAC) 才是。

### 2.2 HTTPS / TLS 注意事项

- 设备 TLS 验证依赖**真实的系统时间**，开机时若 RTC 不准会校验失败。**接口 [A] 务必返回 `server_time`**，否则后续走 HTTPS 的接口（[C][D][E] 等）会全部失败。
- 自签证书：固件未内置自定义 CA，开发环境建议用受信 CA 签发或暂时用 `http://`。

### 2.3 失败处理

- 设备对接口 [A] 有**最多 10 次重试**，初始 10s、每次失败延迟 ×2（`application.cc:399`）。
- 对接口 [B] 同样最多 10 次，状态码 `202` 用 3s 间隔，其他错误用 10s。
- 其他接口（[C][D][E][F][G]）失败**不重试**，直接报错。

---

## 3. 接口 [A]：引导（版本 / 配置下发）

> 这是**唯一硬编码在固件里的接口**，所有后续连接信息都从这里来。完整字段语义见 [`ota-protocol_zh.md`](ota-protocol_zh.md)，本节聚焦服务器实现。

### 3.1 请求

```
POST /xiaozhi/ota/   HTTP/1.1
Host: <your-server>
Activation-Version: 2
Device-Id: aa:bb:cc:dd:ee:ff
Client-Id: 01HXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
Serial-Number: SN20260101000001            # 仅 efuse 烧录过时
User-Agent: bread-compact-wifi/2.2.6
Accept-Language: zh-CN
Content-Type: application/json

{
  "version": 2,
  "flash_size": 16777216,
  "minimum_free_heap_size": 123456,
  "mac_address": "aa:bb:cc:dd:ee:ff",
  "uuid": "01HXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX",
  "chip_model_name": "esp32s3",
  "chip_info": { "model": 9, "cores": 2, "revision": 0, "features": 50 },
  "application": { "name": "xiaozhi", "version": "2.2.6", "compile_time": "...", "idf_version": "v5.4" },
  "partition_table": [ ... ],
  "ota": { "label": "ota_0" },
  "board": { "type": "bread-compact-wifi", "name": "...", "ssid": "...", "rssi": -50, "channel": 6, "ip": "192.168.x.x", "mac": "..." }
}
```

> 当请求 body 为空时设备改用 GET。极少出现，按 POST 实现即可。

### 3.2 响应（200 OK）

```json
{
  "server_time": {
    "timestamp": 1736035200000,
    "timezone_offset": 480
  },

  "firmware": {
    "version": "2.2.7",
    "url": "https://cdn.example.com/firmware/2.2.7.bin",
    "force": 0
  },

  "mqtt": {
    "endpoint": "mqtt.example.com:8883",
    "client_id": "device-aabbccddeeff",
    "username": "device",
    "password": "secret",
    "keepalive": 240,
    "publish_topic": "device-server/aabbccddeeff"
  },
  // 注：上面的 mqtt.client_id 是 **MQTT 协议层的 client_id**（broker 上的连接 ID），
  //     由服务器自行编码下发。**不要**和请求里的 HTTP `Client-Id` 头（设备本地 UUID）混淆。
  //     编码建议含 MAC，确保 broker 上全局唯一。详见 §2.1 术语澄清。

  "websocket": {
    "url": "wss://ws.example.com/xiaozhi/v1/",
    "token": "abc123",
    "version": 1
  },

  "activation": {
    "code": "123456",
    "message": "请在 App 内输入激活码 123456",
    "challenge": "random-server-nonce",
    "timeout_ms": 30000
  }
}
```

### 3.3 关键实现规则

| 字段           | 服务器应当                                                                 |
|----------------|----------------------------------------------------------------------------|
| `server_time`  | **强烈建议返回**，避免 TLS 校验失败。`timestamp` 单位毫秒，`timezone_offset` 单位分钟 |
| `mqtt` & `websocket` | **二选一**。若同时返回，设备只用 `mqtt`（MQTT 优先级更高）。强制走 WebSocket → 不要返回 `mqtt` 字段 |
| `firmware`     | 仅当有更新时返回；`version` 按 `1.2.3` 段比较，`force: 1` 可绕过版本比较     |
| `activation`   | 仅在设备未激活时返回 `challenge`。设备会循环 POST 激活接口直到成功            |
| `mqtt.publish_topic` | 设备唯一的**上行 topic**。下行 topic 由 broker 按 client_id 路由，不在此处下发 |

### 3.4 字段写入设备的 NVS

返回的 `mqtt.*` 和 `websocket.*` 子字段会被**逐字段写入 NVS**（不会清理旧字段），所以服务器**每次都应返回完整对象**，不要只返回 diff。

---

## 4. 接口 [B]：设备激活

### 4.1 URL

```
POST <OTA_URL>activate
# 例如：POST https://api.example.com/xiaozhi/ota/activate
```

`<OTA_URL>` 不带尾斜杠时拼成 `/activate`，带尾斜杠时拼成 `activate`（`ota.cc:464-469`）。两种情况服务器都应该路由到同一个 handler。

### 4.2 请求

请求头同接口 [A]。请求 body：

```json
{
  "algorithm": "hmac-sha256",
  "serial_number": "SN20260101000001",
  "challenge": "random-server-nonce",
  "hmac": "5d8e...hex-of-hmac-sha256(efuse_key, challenge)..."
}
```

- `algorithm` 固定 `hmac-sha256`
- `serial_number` 必须和 [A] 请求头里的 `Serial-Number` 一致
- `challenge` 必须等于上一次 [A] 响应里下发的 `activation.challenge`
- `hmac` = HMAC-SHA256(efuse 中的 HMAC_KEY0, challenge) 的 hex 字符串

> 设备未烧 SN 时 body 直接是 `"{}"`（`ota.cc:422-424`）。这种情况服务器应判断鉴权失败。

### 4.3 响应

| 状态码 | 语义                          | 服务器实现要点 |
|--------|-------------------------------|----------------|
| `200`  | 激活成功                      | 标记设备已激活，落库 |
| `202`  | 等待用户输入                  | 用户尚未在 App/控制台确认激活码。设备 3s 后重试 |
| 其他   | 失败                          | 设备 10s 后重试，最多 10 次 |

> Body 内容设备**不解析**。可空可填，主要看状态码。

### 4.4 服务器侧校验流程

```
收到激活请求
   ├─► 校验 Device-Id / Client-Id / Serial-Number 三者关联
   ├─► 查 SN 对应的 efuse HMAC 密钥（出厂烧录时入库）
   ├─► 服务端计算 expected_hmac = HMAC-SHA256(key, challenge)
   ├─► 对比 expected_hmac == 请求中的 hmac
   ├─► 检查用户是否已在 App 输入激活码
   │      ├─► 已输入  → 200
   │      └─► 未输入  → 202
   └─► hmac 不匹配 / 其他错误 → 4xx/5xx
```

---

## 5. 接口 [C]：固件包下载

### 5.1 URL

由接口 [A] 响应中的 `firmware.url` 给出。**设备不会带任何身份头**，纯净 HTTP GET。

```
GET <firmware.url>   HTTP/1.1
```

### 5.2 响应

- 状态码必须是 `200`
- `Content-Length` 必须正确（设备依赖它显示进度，`ota.cc:292-296`）
- Body 是 ESP-IDF 标准 OTA 镜像（`.bin`）
- 设备按 4KB 流式接收并写入 OTA 分区

### 5.3 服务器实现要点

- 用 CDN 或静态文件服务器即可，**不需要任何业务逻辑**
- URL 可以是 query 鉴权（`?token=xxx`）或预签名 URL；身份验证逻辑放在 OTA 服务器一侧，下发 URL 时附带签名
- 设备**不支持 HTTP 重定向**到不同 host（已实际验证过的限制建议确认；保险起见尽量返回 200 直接给数据，避免 30x）

---

## 6. 接口 [D]：资源包下载

### 6.1 URL 下发路径

**不是通过 OTA 接口下发**，而是通过 MCP 工具 `self.assets.set_download_url` 在会话期间设置（`mcp_server.cc:290-300`）：

```jsonc
// LLM 通过 MCP 调用
{
  "method": "tools/call",
  "params": {
    "name": "self.assets.set_download_url",
    "arguments": { "url": "https://cdn.example.com/assets/v3.bin" }
  }
}
```

设备把 URL 写入 NVS（命名空间 `assets`，键 `download_url`），**下次启动**时在 `CheckAssetsVersion()` 阶段消费并清空（`application.cc:340-396`）。

### 6.2 请求与响应

和接口 [C] 几乎一样：

- 设备发送：`GET <url>`，无身份头
- 服务器返回：200 + `Content-Length` 正确的二进制
- 设备按扇区（4KB）流式写入资源分区
- 大小不得超过分区大小（`assets.cc:454-456`）

### 6.3 资源包格式

资源包内含表情包、字体等，格式由 `assets.cc` 解析。具体格式见 `main/assets.cc` 的 `ApplyPartition()` 实现。**服务器只负责存储和分发**，不需要理解格式。

---

## 7. 接口 [E]：图像理解（视觉问答）

### 7.1 URL 下发路径

通过 MCP capability 协商下发，**不是 OTA 接口**。MCP 连接建立时服务器返回：

```jsonc
// MCP initialize 响应中的 capabilities
{
  "capabilities": {
    "vision": {
      "url":   "https://vision.example.com/v1/explain",
      "token": "vision-api-token"
    }
  }
}
```

设备调 `camera->SetExplainUrl(url, token)` 保存（`mcp_server.cc:334-351`），之后**每次唤醒提问且带摄像头场景**时调用。

### 7.2 请求

```
POST https://vision.example.com/v1/explain   HTTP/1.1
Device-Id: aa:bb:cc:dd:ee:ff
Client-Id: 01HXXXX-...
Authorization: Bearer vision-api-token       # 仅 token 非空时
Content-Type: multipart/form-data; boundary=...
Transfer-Encoding: chunked

--<boundary>
Content-Disposition: form-data; name="question"

用户问的问题（UTF-8 文本）
--<boundary>
Content-Disposition: form-data; name="file"; filename="camera.jpg"
Content-Type: image/jpeg

<JPEG 二进制>
--<boundary>--
```

参考实现：`boards/common/esp32_camera.cc:237-310`、`boards/common/esp_video.cc:945-1010`。

### 7.3 响应

```
HTTP/1.1 200 OK
Content-Type: application/json

{ "success": true, "result": "图中是一只橘色的猫坐在窗台上..." }
```

- 状态码必须 200
- 响应体设备会原样塞回 LLM 上下文（具体如何使用看上层实现）

---

## 8. 接口 [F] / [G]：MCP 工具触发的临时 URL

这两个接口的 URL 由**云端 LLM 调用 MCP 工具时通过参数指定**，设备只是一个"按 URL 收发"的执行器。

### 8.1 [F] 截图上传 `self.screen.snapshot_to_jpeg`

LLM 调用：
```json
{ "method": "tools/call", "params": { "name": "self.screen.snapshot_to_jpeg",
   "arguments": { "url": "https://your-server.com/upload", "quality": 80 } } }
```

设备行为（`mcp_server.cc:209-241`）：
```
POST <url>
Content-Type: multipart/form-data; boundary=----ESP32_SCREEN_SNAPSHOT_BOUNDARY

--<boundary>
Content-Disposition: form-data; name="file"; filename="screenshot.jpg"
Content-Type: image/jpeg

<JPEG 二进制>
--<boundary>--
```

服务器返回 200 即可，body 内容会回显给 LLM。

### 8.2 [G] 图片下载 `self.screen.preview_image`

LLM 调用：
```json
{ "method": "tools/call", "params": { "name": "self.screen.preview_image",
   "arguments": { "url": "https://your-server.com/image.jpg" } } }
```

设备行为（`mcp_server.cc:250-278`）：
```
GET <url>
```

要求：
- 200 + 正确的 `Content-Length`
- Body 是设备屏幕支持的图片格式（一般 JPEG / PNG）

> [F][G] 的服务器实现是开放的，没有协议规定 —— 由你的 LLM 工具调用方决定怎么用。

---

## 9. 最小可用服务器实现

下面是一个**走 WebSocket** 的最小服务器（Python FastAPI 风格伪代码），仅实现接口 [A]：

```python
from fastapi import FastAPI, Request, Response
import time

app = FastAPI()

@app.api_route("/xiaozhi/ota/", methods=["GET", "POST"])
async def ota_bootstrap(req: Request):
    mac      = req.headers.get("Device-Id")       # 主键：硬件 MAC
    instance = req.headers.get("Client-Id")       # 辅键：设备本地 UUID（仅区分实例）
    sn       = req.headers.get("Serial-Number")
    # 1. 鉴权 / 注册新设备 —— 以 MAC 为主键，UUID 仅作辅助
    device = lookup_or_register_by_mac(mac, instance, sn)

    # 2. 组装响应
    resp = {
        "server_time": {
            "timestamp": int(time.time() * 1000),
            "timezone_offset": 480,
        },
        "websocket": {
            "url":   "ws://192.168.1.100:8000/xiaozhi/v1/",
            "token": device.session_token,
            "version": 1,
        },
        # 若要走 MQTT，client_id 要含 MAC 才能保证 broker 上唯一：
        # "mqtt": {
        #     "endpoint": "mqtt.example.com:8883",
        #     "client_id": f"device-{mac.replace(':', '')}",
        #     "username": "device",
        #     "password": device.mqtt_password,
        #     "publish_topic": f"device-server/{mac.replace(':', '')}",
        # },
    }

    # 3. 仅当未激活且支持激活时下发
    if not device.activated and sn:
        resp["activation"] = {
            "code":       device.gen_activation_code(),
            "message":    f"请输入激活码 {device.activation_code}",
            "challenge":  device.gen_challenge(),
            "timeout_ms": 30000,
        }

    # 4. 仅当固件落后于服务器时下发
    if device.fw_version < LATEST_FW:
        resp["firmware"] = {
            "version": LATEST_FW,
            "url": f"https://cdn.example.com/firmware/{LATEST_FW}.bin",
        }

    return resp


@app.post("/xiaozhi/ota/activate")
async def activate(req: Request):
    body = await req.json()
    device = lookup_by_sn(body["serial_number"])
    expected = hmac_sha256(device.efuse_key, body["challenge"])

    if body["hmac"] != expected:
        return Response(status_code=401)

    if not device.user_confirmed_in_app:
        return Response(status_code=202)         # 等用户输入激活码

    device.activated = True
    device.save()
    return Response(status_code=200)
```

把固件里 `CONFIG_OTA_URL` 改为指向这个服务器（如 `http://192.168.1.100:8000/xiaozhi/ota/`）即可。

> 如果要走 MQTT，需要额外部署一个 MQTT broker（如 EMQX）+ broker 内的 client_id 路由逻辑，详见 `docs/mqtt-udp_zh.md`。**走 WebSocket 是搭本地服务器最省事的方案**。

---

## 10. 常见坑

### 10.1 设备一直停在"连接服务器"

排查顺序：
1. OTA 接口是否返回 `200`？非 200 设备只会重试 10 次然后报错
2. 响应 body 是否合法 JSON？解析失败 = `ESP_ERR_INVALID_RESPONSE`
3. `mqtt` / `websocket` 至少要返回一个；都没返回时设备默认走 MQTT 但读不到 endpoint 会失败
4. **没返回 `server_time`** + 服务器是 HTTPS → TLS 时间校验失败，HTTP 接口看似正常但后续连接全挂

### 10.2 MQTT 连上但收不到 server hello

- 设备**不主动 SUBSCRIBE**，下行靠 broker 按 `client_id` 路由
- 普通 broker 不做这件事 → 设备发完 `hello` 后等 10 秒超时失败（`mqtt_protocol.cc:233`）
- 解决方案：用支持 hook 的 broker（EMQX 自定义插件、Mosquitto 鉴权插件等），或者用 server-golang/python 这类自带 broker 路由的现成实现

### 10.3 激活循环不退出

- 设备只看激活接口的状态码，**不看 body**
- 必须用 `200` 表示成功；`202` 表示"还在等"；其他都会被当失败重试
- 注意 `<OTA_URL>activate` 和 `<OTA_URL>/activate` 两种拼法都要支持

### 10.4 OTA 下发字段被旧值"污染"

- NVS 里 `mqtt.*` / `websocket.*` 的旧 key **不会被新响应清掉**（`ota.cc:151-161` 只写不删）
- 服务器协议要变（比如改 `publish_topic` 命名）时务必下发**所有**新字段，否则可能新旧混用
- 重置设备：`idf.py erase-flash` 或专门写一个清 NVS 的 MCP 工具

### 10.5 固件 / 资源下载 URL 不能跨 host 重定向

- 设备 HTTP 客户端对 30x 行为不强健，建议**直接返回 200 + 数据**，不用 redirect
- 预签名 URL（S3 / OSS 临时签名）可以，但**返回的 URL 直接是最终下载地址**，不要再嵌一层跳转

### 10.6 把 HTTP `Client-Id` 头当成 MQTT client_id 用

这是**最高频的踩坑点**。三个相似的标识符职责完全不同（详见 §2.1）：

- **HTTP `Client-Id` 头**：设备本地 UUID v4，erase-flash 后会变 → **不要作为设备主键**，不要直接灌进 `mqtt.client_id` 字段
- **HTTP `Device-Id` 头**：efuse MAC，硬件唯一 → 作为设备主键
- **OTA 响应里的 `mqtt.client_id`**：**服务器生成**的 MQTT 连接 ID，必须**含 MAC** 才能保证 broker 上唯一

错误示例：
```python
# ❌ 错误：把 HTTP Client-Id (UUID) 直接当 MQTT client_id 下发
resp["mqtt"]["client_id"] = req.headers["Client-Id"]
# 后果：erase-flash 后 UUID 变了，broker 上变成"新设备"，
#       原有订阅/会话状态丢失；多实例场景下也无法对应到物理设备。

# ✅ 正确：用 MAC 编码 MQTT client_id
resp["mqtt"]["client_id"] = f"device-{req.headers['Device-Id'].replace(':', '')}"
```

---

## 11. 字段对照速查

| 接口 | 方法 | 设备触发时机              | 服务器必返字段                                | 失败影响 |
|------|------|---------------------------|----------------------------------------------|----------|
| [A]  | POST | 每次开机 + 重试逻辑       | `server_time`、`mqtt` 或 `websocket` 之一    | 设备无法上线 |
| [B]  | POST | A 返回 `activation` 后    | （状态码即语义，body 任意）                   | 激活无法完成 |
| [C]  | GET  | A 返回 `firmware` 且版本新| 200 + 正确 Content-Length + bin 数据         | 不升级（不致命） |
| [D]  | GET  | MCP 设置 URL 后下次开机   | 200 + 正确 Content-Length + 资源数据          | 表情/字体不更新 |
| [E]  | POST | 摄像头会话 + 用户提问     | 200 + 文本结果                                | 视觉问答不可用 |
| [F]  | POST | LLM 工具调用              | 200（body 任意，返回给 LLM）                  | 工具调用失败 |
| [G]  | GET  | LLM 工具调用              | 200 + 图片数据                                | 工具调用失败 |

---

## 12. 与现有开源 server 的对比

| Server 实现                | A / B | C / D | E       | F / G | MQTT broker 路由 |
|----------------------------|-------|-------|---------|-------|------------------|
| xiaozhi-esp32-server (Py)  | ✅    | ✅    | ✅      | 部分  | 内建             |
| xiaozhi-esp32-server-golang| ✅    | ✅    | ✅      | 部分  | 内建             |
| 自建（仅 OTA + WebSocket） | ✅    | ❌    | ❌      | ❌    | 不需要           |

最小可用：**只实现 [A]**，返回 WebSocket 配置即可让设备进入正常对话。其他接口按需逐步补齐。
