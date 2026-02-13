# BluFi 配网（集成 esp-wifi-connect）

本文档说明如何在小智固件中启用和使用 BluFi（BLE Wi‑Fi 配网），并结合项目内置的 `esp-wifi-connect` 组件完成 Wi‑Fi 连接与存储。官方
BluFi
协议说明请参考 [Espressif 文档](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/api-guides/ble/blufi.html)。

## 前置条件

- 需要支持 BLE 的芯片与固件配置。
- 在 `idf.py menuconfig` 中启用 `WiFi Configuration Method -> Esp Blufi`（`CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING=y`
  ）。如果想使用 BluFi，必须关闭同一菜单下的 Hotspot 选项，否则默认使用 Hotspot 配网模式。

- 保持默认的 NVS 与事件循环初始化（项目的 `app_main` 已处理）。
- CONFIG_BT_BLUEDROID_ENABLED、CONFIG_BT_NIMBLE_ENABLED这两个宏应二选一，不能同时启用。
## 工作流程

1) 手机端通过 BluFi（如官方 EspBlufi App 或自研客户端）连接设备，发送 Wi‑Fi SSID/密码，手机端可以通过blufi协议获取设备端扫描到的WiFi列表。
2) 设备侧在 `ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP` 中将凭据写入 `SsidManager`（存储到 NVS，属于 `esp-wifi-connect` 组件）。
3) 随后启动 `WifiStation` 扫描并连接；状态通过 BluFi 返回。
4) 配网成功后设备会自动连接新 Wi‑Fi；失败则返回失败状态。

## 使用步骤

1. 配置：在 menuconfig 开启 `Esp Blufi`。编译并烧录固件。
2. 触发配网：设备首次启动且没有已保存的 Wi‑Fi 时会自动进入配网。
3. 手机端操作：打开 EspBlufi App（或其他 BluFi 客户端），搜索并连接设备，可以选择是否加密，按提示输入 Wi‑Fi SSID/密码并发送。
4. 观察结果：
    - 成功：BluFi 报告连接成功，设备自动连接 Wi‑Fi。
    - 失败：BluFi 返回失败状态，可重新发送或检查路由器。

## 注意事项

- BluFi 配网不支持与热点配网同时开启。如果热点配网已经启动，则默认使用热点配网。请在 menuconfig 中只保留一种配网方式。
- 若多次测试，建议清除或覆盖存储的 SSID（`wifi` 命名空间），避免旧配置干扰。
- 如果使用自定义 BluFi 客户端，需遵循官方协议帧格式，参考上文官方文档链接。
- 官方文档中已提供EspBlufi APP下载地址
- 由于IDF5.5.2的blufi接口发生变化,5.5.2版本编译后蓝牙名称为"Xiaozhi-Blufi",5.5.1版本中蓝牙名称为"BLUFI_DEVICE"
