# BluFi 配网（集成 esp-wifi-connect）

本文档说明如何在小智固件中启用和使用 BluFi（BLE Wi‑Fi 配网），并结合项目内置的 `esp-wifi-connect` 组件完成 Wi‑Fi 连接与存储。官方
BluFi
协议说明请参考 [Espressif 文档](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/api-guides/ble/blufi.html)。

## 前置条件

- 需要支持 BLE 的芯片与固件配置。
- 当前固件固定使用 BluFi 配网（`CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING=y`），不再走 Hotspot/Acoustic 配网分支。
- 保持默认的 NVS 与事件循环初始化（项目的 `app_main` 已处理）。
- CONFIG_BT_BLUEDROID_ENABLED、CONFIG_BT_NIMBLE_ENABLED这两个宏应二选一，不能同时启用。
## 工作流程

1) 手机端通过 BluFi（如官方 EspBlufi App 或自研客户端）连接设备，发送 Wi‑Fi SSID/密码，手机端可以通过blufi协议获取设备端扫描到的WiFi列表。
2) 设备侧在 `ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP` 中将凭据写入 `SsidManager`（存储到 NVS，属于 `esp-wifi-connect` 组件）。
3) 随后启动 `WifiStation` 扫描并连接；状态通过 BluFi 返回。
4) 配网成功后设备会自动连接新 Wi‑Fi；失败则返回失败状态。

## 使用步骤

1. 配置：默认即为固定 BluFi 配网，编译并烧录固件。
2. 触发配网：设备首次启动且没有已保存的 Wi‑Fi 时会自动进入配网。
3. 手机端操作：打开 EspBlufi App（或其他 BluFi 客户端），搜索并连接设备，可以选择是否加密，按提示输入 Wi‑Fi SSID/密码并发送。
4. 观察结果：
    - 成功：BluFi 报告连接成功，设备自动连接 Wi‑Fi。
    - 失败：BluFi 返回失败状态，可重新发送或检查路由器。

## 注意事项

- 固件当前固定走 BluFi 配网，不再支持运行时切换到热点配网。
- 若多次测试，建议清除或覆盖存储的 SSID（`wifi` 命名空间），避免旧配置干扰。
- 如果使用自定义 BluFi 客户端，需遵循官方协议帧格式，参考上文官方文档链接。
- 官方文档中已提供EspBlufi APP下载地址
- 由于IDF5.5.2的blufi接口发生变化,5.5.2版本编译后蓝牙名称为"Xiaozhi-Blufi",5.5.1版本中蓝牙名称为"BLUFI_DEVICE"

## 扩展方案：设备 -> App 自定义 `0x13` 数据通道

按约定使用 BluFi 的 **DATA 帧 subtype=0x13（Custom Data）**。
当前仅实现设备单向发送（设备 -> App），且不做分片。

### 载荷格式 v1

自定义数据 payload 固定 8 字节：

- `Byte[0] = 0x01`：协议版本
- `Byte[1] = 0x01`：消息类型（WiFi MAC）
- `Byte[2..7]`：6 字节 WiFi MAC 原始字节

示例：

```text
01 01 AA BB CC DD EE FF
```

### 当前实现补充

- 在安全协商（DH）成功后，设备会连续发送 3 次上述 v1 格式 payload（间隔约 120ms）。
- App 收到 `subtype=0x13` 的 custom data 后，按 v1 格式解析 8 字节载荷即可；重复帧可按 MAC 去重。

### 兼容性与约束

- 建议 App 对未知版本或未知消息类型做忽略处理。
- 建议自定义消息不要承载明文敏感信息（如完整密码）。
- 当 BLE 已断开或已完成配网并主动断连后，停止发送自定义消息。
