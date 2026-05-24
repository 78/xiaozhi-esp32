# 小智 ESP32 固件 HTTP 接口说明

本文档梳理固件除 **MQTT + UDP** 与 **WebSocket** 两类 AI 语音对话协议之外，所有通过 HTTP/HTTPS 与外部服务交互的接口。内容直接来自代码（含文件:行号引用），便于服务器端按约定实现或排查。

## 接口总览

| # | 接口 | 方法 | URL 来源 | 鉴权 | 触发时机 | 主调用入口 |
|---|------|------|---------|------|---------|-----------|
| 1 | OTA 版本检查 / 服务配置下发 | `POST`（无系统信息时 `GET`） | `CONFIG_OTA_URL`，可被 NVS `wifi/ota_url` 覆盖 | `Device-Id` / `Client-Id` / `Serial-Number` / `Activation-Version` 等 header | 网络就绪后启动 `ActivationTask` | `Ota::CheckVersion()` `main/ota.cc:77` |
| 2 | 设备激活 | `POST` | 接口 1 的 URL 拼 `activate` | 同上 + HMAC-SHA256 challenge body | 接口 1 返回 `activation.challenge` 时 | `Ota::Activate()` `main/ota.cc:458` |
| 3 | 固件镜像下载 | `GET` | 接口 1 响应的 `firmware.url` | 无（URL 自带签名/直链） | 发现新版本后 | `Ota::Upgrade()` `main/ota.cc:267` |
| 4 | 资源包下载 | `GET` | NVS `assets/download_url` | 无 | 通过 MCP 工具下发 URL 后 | `Assets::Download()` `main/assets.cc:428` |
| 5 | 摄像头图像「看图说话」 | `POST`（multipart, chunked） | MCP `vision.url`（通过 `SetExplainUrl` 注入） | `Authorization: Bearer <token>` + `Device-Id` / `Client-Id` | 调用相机 `Explain()` | `Esp32Camera::Explain()` `main/boards/common/esp32_camera.cc:236` 等 |
| 6 | 屏幕截图上传 | `POST`（multipart） | 调用方在 MCP 工具参数中传入 | 无（URL 由调用方保证） | MCP 工具 `self.screen.snapshot` | `main/mcp_server.cc:209` |
| 7 | 图片预览下载 | `GET` | 同上 | 无 | MCP 工具 `self.screen.preview_image` | `main/mcp_server.cc:250` |

> 接口 1 同时承担「服务端时间下发 (`server_time`)」和「MQTT / WebSocket 协议配置下发 (`mqtt` / `websocket`)」两项职责，是设备启动后第一件做的事。

---

## 1. OTA 版本检查 / 服务配置下发

设备启动联网后第一时间调用，用一次往返完成「拿活码 → 拿 MQTT/WS 接入信息 → 校时 → 判断是否有新固件」。

- **代码位置**：`main/ota.cc:77` `Ota::CheckVersion()`，HTTP 头部由 `Ota::SetupHttp()` `main/ota.cc:55` 统一设置。
- **请求方法**：`board.GetSystemInfoJson()` 返回非空时为 `POST`，为空时退化为 `GET`（`main/ota.cc:94`）。
- **URL**：`Ota::GetCheckVersionUrl()` `main/ota.cc:46`
  - 优先读 NVS：`Settings("wifi").GetString("ota_url")`
  - 为空时回退到 Kconfig 默认值 `CONFIG_OTA_URL`，默认值为 `https://api.tenclass.net/xiaozhi/ota/`（见 `main/Kconfig.projbuild:3`）。
- **请求 Header**（`main/ota.cc:60-69`）：

  | Header | 取值 | 说明 |
  |--------|------|------|
  | `Activation-Version` | `"2"`（efuse 烧录了 Serial Number）/ `"1"` | 区分激活协议版本 |
  | `Device-Id` | `SystemInfo::GetMacAddress()` | 设备 MAC，小写、冒号分隔 |
  | `Client-Id` | `Board::GetInstance().GetUuid()` | 板级生成的 UUID |
  | `Serial-Number` | efuse `USER_DATA` 块（32 字节，首字节非 0 才有） | 仅当 efuse 中存在时下发 |
  | `User-Agent` | `SystemInfo::GetUserAgent()` | 包含板型 + 应用版本 |
  | `Accept-Language` | `Lang::CODE` | 编译期 i18n 语言代码 |
  | `Content-Type` | `application/json` | 固定 |

- **请求 Body**：`Board::GetSystemInfoJson()`（`main/boards/common/board.cc`）拼出的设备信息，结构示例：

  ```json
  {
    "version": 2,
    "language": "zh-CN",
    "flash_size": 16777216,
    "minimum_free_heap_size": "...",
    "mac_address": "xx:xx:xx:xx:xx:xx",
    "uuid": "....",
    "chip_model_name": "esp32s3",
    "chip_info": { "model": 9, "cores": 2, "revision": 2, "features": 0 },
    "application": {
      "name": "xiaozhi",
      "version": "1.x.x",
      "compile_time": "2026-..T..Z",
      "idf_version": "...",
      "elf_sha256": "..."
    },
    "partition_table": [ { "label": "...", "type": 0, "subtype": 0, "address": 0, "size": 0 } ],
    "board": { ... }
  }
  ```

- **预期响应**：`HTTP 200` + JSON。各字段解析逻辑见 `main/ota.cc:116-244`：

  ```jsonc
  {
    "activation": {                 // 未激活时下发，激活后不再返回
      "message": "...",            // 显示给用户的提示文案
      "code":    "123456",         // 屏幕展示的 6 位激活码
      "challenge": "<hex/string>", // 设备用 HMAC-SHA256 回签
      "timeout_ms": 30000           // 单次激活轮询超时
    },
    "mqtt": {                       // 任意 string/number 字段；逐项写入 NVS "mqtt" 命名空间
      "endpoint": "...",
      "client_id": "...",
      "username": "...",
      "password": "...",
      "publish_topic": "...",
      "subscribe_topic": "..."
    },
    "websocket": {                  // 任意 string/number 字段；逐项写入 NVS "websocket" 命名空间
      "url": "wss://...",
      "token": "..."
    },
    "server_time": {                // 用于 settimeofday()
      "timestamp": 1716528000000,   // ms 时间戳
      "timezone_offset": 480         // 分钟，可选
    },
    "firmware": {
      "version": "1.2.3",
      "url":     "https://.../firmware.bin",
      "force":   0                  // =1 时强制下发，即使版本号未更新
    }
  }
  ```

  解析要点：
  - `mqtt` / `websocket` 整段透传：每个 string/number 字段都会按 key 写入对应 NVS namespace；服务端可借此**动态切换接入信息**。
  - `firmware.version` 与本机 `esp_app_get_description()->version` 做点分号语义比较（`Ota::IsNewVersionAvailable` `main/ota.cc:406`），新版本才置 `has_new_version_`。
  - `firmware.force == 1` 时强制升级。
  - 没有 `firmware` / `server_time` / `mqtt` / `websocket` 节都允许，仅打 warning，不会让流程失败。

- **失败处理**：`Open()` 失败返回 `GetLastError()`；HTTP 非 200 直接返回 status code；JSON 解析失败返回 `ESP_ERR_INVALID_RESPONSE`。

---

## 2. 设备激活（HMAC-SHA256 Challenge）

仅当接口 1 返回 `activation.challenge` 时触发；上层 `Application::ActivationTask()` 最多重试若干次，直到拿到 200。

- **代码位置**：`Ota::Activate()` `main/ota.cc:458`，body 生成在 `Ota::GetActivationPayload()` `main/ota.cc:421`。
- **请求方法**：`POST`
- **URL**：在接口 1 的 URL 上拼接 `activate`，自动补斜杠（`main/ota.cc:464-469`）。例如：
  - `https://api.tenclass.net/xiaozhi/ota/` → `https://api.tenclass.net/xiaozhi/ota/activate`
- **请求 Header**：与接口 1 完全相同（同样走 `SetupHttp()`）。
- **请求 Body**：

  ```json
  {
    "algorithm": "hmac-sha256",
    "serial_number": "<efuse 中的 32 字节序列号>",
    "challenge":     "<服务端在接口 1 中下发的 challenge 原文>",
    "hmac":          "<challenge 经 efuse HMAC_KEY0 计算得到的 SHA-256，hex 小写>"
  }
  ```

  HMAC 由 ESP 硬件 `esp_hmac_calculate(HMAC_KEY0, challenge, ...)` 计算（`main/ota.cc:431`），密钥本身**不出芯片**。
  设备没烧 Serial Number 时 body 直接发 `"{}"`，等同于通知服务端「该设备无法走 HMAC 激活」。

- **响应处理**（`main/ota.cc:481-491`）：

  | Status | 含义 | 调用方动作 |
  |--------|------|-----------|
  | 200 | 激活成功 | 跳出激活循环，继续启动流程 |
  | 202 | 等待用户在网页/小程序确认 | 返回 `ESP_ERR_TIMEOUT`，由 `ActivationTask` 间隔重试 |
  | 其他 | 失败 | 打印 body 后返回 `ESP_FAIL` |

---

## 3. 固件镜像下载

- **代码位置**：`Ota::Upgrade()` `main/ota.cc:267`，由 `Ota::StartUpgrade()` 触发。
- **请求方法**：`GET`
- **URL**：接口 1 响应的 `firmware.url`，**不附加任何业务 Header**（认证假设嵌入 URL 或由 CDN 控制）。
- **响应要求**：必须 `HTTP 200`，且 `Content-Length` 非 0（`main/ota.cc:287, 292`）。
- **下载行为**：
  - 4 KB 缓冲区，边读边写到 `esp_ota_get_next_update_partition()` 指向的分区；
  - 第一次拼齐 `esp_image_header_t + esp_image_segment_header_t + esp_app_desc_t` 后才 `esp_ota_begin`，能在头部不匹配时尽早中断；
  - 每秒回调一次 `progress`/`speed`，供 UI 显示；
  - 完成后 `esp_ota_set_boot_partition()`，下次重启从新分区启动；首次启动由 `Ota::MarkCurrentVersionValid()` `main/ota.cc:247` 提交，避免回滚。

---

## 4. 资源包（Assets）下载

用于热更新表情/图片等只读资源，**URL 由服务端通过 MCP 工具 `self.assets.set_download_url` 下发**，存到 NVS `assets/download_url`，由 `Application::CheckAssetsVersion()` 异步调用 `Assets::Download()`。

- **代码位置**：`Assets::Download()` `main/assets.cc:428`；MCP 工具注册见 `main/mcp_server.cc:290`。
- **请求方法**：`GET`
- **URL**：调用方通过 MCP 工具传入的任意 URL。
- **请求 Header**：无（裸 GET）。
- **响应要求**：
  - `HTTP 200`；
  - `Content-Length` 必须存在且 ≤ assets 分区大小（`main/assets.cc:443-457`）。
- **下载行为**：按 flash sector 边擦边写到 `assets` 分区，结束后 `InitializePartition()` 重新内存映射。

---

## 5. 摄像头「看图说话」上传（Vision Explain）

带相机的板卡（`esp32_camera`、`esp_video`、SenseCAP Watcher 的 `sscma_camera`）共用同一组 HTTP 约定。

- **代码位置**：
  - `Esp32Camera::Explain()` `main/boards/common/esp32_camera.cc:236`
  - `EspVideo::Explain()` `main/boards/common/esp_video.cc:945`
  - `SscmaCamera::Explain()` `main/boards/sensecap-watcher/sscma_camera.cc:677`
- **URL / Token 来源**：MCP 在初始化阶段下发 `capabilities.vision = { url, token }`（`main/mcp_server.cc:334-351`），写入相机对象的 `explain_url_` / `explain_token_`。
- **请求方法**：`POST`
- **请求 Header**：

  | Header | 取值 | 说明 |
  |--------|------|------|
  | `Device-Id` | MAC | 设备识别 |
  | `Client-Id` | UUID | 同 OTA |
  | `Authorization` | `Bearer <explain_token>` | 仅当 token 非空 |
  | `Content-Type` | `multipart/form-data; boundary=----ESP32_CAMERA_BOUNDARY` | |
  | `Transfer-Encoding` | `chunked` | 因 JPEG 边编码边发，提前发送 header 时不知 Content-Length |

- **请求 Body**：multipart/form-data，依次写入：
  1. `question` 字段：用户提问文本；
  2. `file` 字段（filename=`camera.jpg`，Content-Type=`image/jpeg`）：摄像头当前帧的 JPEG（`esp32_camera` 中通过独立编码线程 + 队列分块写出）；
  3. multipart 结束符 `\r\n----ESP32_CAMERA_BOUNDARY--\r\n`。
- **响应**：`HTTP 200` + 任意文本（建议 JSON），固件原样返回给上层 MCP 调用方。非 200 抛 `runtime_error`（`main/boards/common/esp32_camera.cc:310`）。

---

## 6. 屏幕截图上传（MCP 工具）

仅在编译启用 `CONFIG_LV_USE_SNAPSHOT` 时注册。

- **MCP 工具**：`self.screen.snapshot`，参数 `url`（string，调用方提供）、`quality`（1-100，默认 80）。
- **代码位置**：`main/mcp_server.cc:190-242`。
- **请求方法**：`POST`
- **URL**：完全由 MCP 调用方传入，不做白名单校验。
- **请求 Header**：仅 `Content-Type: multipart/form-data; boundary=----ESP32_SCREEN_SNAPSHOT_BOUNDARY`，**不带任何鉴权 header**——安全性由调用方在 URL 中保证（如签名 URL）。
- **请求 Body**：单字段 `file`（filename=`screenshot.jpg`，Content-Type=`image/jpeg`），值为 LVGL 截屏后压缩的 JPEG 数据。
- **响应**：`HTTP 200` 即可，body 被读出后仅做日志，不解析。

---

## 7. 图片预览下载（MCP 工具）

- **MCP 工具**：`self.screen.preview_image`，参数 `url`。
- **代码位置**：`main/mcp_server.cc:244-282`。
- **请求方法**：`GET`
- **请求 Header**：无。
- **响应要求**：`HTTP 200` + 已知 `Content-Length`；按长度一次性读完，丢给 `LvglAllocatedImage` 渲染。

---

## 横向约定与实现细节

- **HTTP 客户端**：统一通过 `Board::GetInstance().GetNetwork()->CreateHttp(timeout_seconds)` 获取（接口定义见 `managed_components/78__esp-ml307/include/http.h`），WiFi/4G 板卡各自实现。`timeout_seconds=0` 表示使用默认值（OTA/Assets 走 0；相机/MCP 工具走 3）。
- **设备身份**：所有「需要识别设备」的请求统一带 `Device-Id`（MAC）+ `Client-Id`（UUID）。`Serial-Number` 仅在 efuse 烧录后出现，对应能走 HMAC 激活的设备。
- **激活态机**：联网后 `Application::ActivationTask`（`main/application.cc`）→ `CheckAssetsVersion` → `CheckNewVersion`（含 OTA `CheckVersion`）→ 屏幕显示激活码 → 循环 `Activate`，激活成功后才会用接口 1 下发的 `mqtt` / `websocket` 配置初始化语音协议。
- **校时**：仅依赖接口 1 的 `server_time.timestamp`，**没有独立 SNTP/NTP HTTP 调用**。
- **WiFi 配网**：使用 BLE 上的 BLUFI 协议（见 `docs/blufi_zh.md`），**不存在 captive portal 或本地 HTTP 配网接口**。
- **Server 端实现要点**：
  - 接口 1 即便首启也必须返回，至少包含 `mqtt` 或 `websocket` 中的一段，否则设备会停在「Server not configured」状态；
  - 接口 1 与接口 2 通常挂在同一个 base URL 下，`activate` 是相对路径；
  - 接口 1 的 `firmware.url` 与接口 3 直接相连，若放在 CDN 注意保留 `Content-Length`；
  - 接口 5 的服务端需要兼容 `Transfer-Encoding: chunked` 的 multipart 上传。
