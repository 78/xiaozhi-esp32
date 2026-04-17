# BluFi Provisioning (with `esp-wifi-connect`)

This document explains how to enable and use BluFi (BLE-based WiFi provisioning) in the XiaoZhi firmware, together with the in-tree `esp-wifi-connect` component that handles WiFi connection and credential storage. See the official [Espressif BluFi documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/ble/blufi.html) for the protocol details.

## Prerequisites

- A chip and firmware configuration that support BLE.
- In `idf.py menuconfig`, enable `WiFi Configuration Method -> Esp Blufi` (`CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING=y`). If you want to use BluFi, disable the Hotspot option in the same menu; otherwise hotspot provisioning wins by default.
- Keep the default NVS and event-loop initialization provided by the project's `app_main`.
- Exactly one of `CONFIG_BT_BLUEDROID_ENABLED` / `CONFIG_BT_NIMBLE_ENABLED` must be selected; they are mutually exclusive.

## Workflow

1. A phone (using the official EspBlufi app or another BluFi client) connects to the device over BLE and sends the target WiFi SSID / password. The phone can also request the list of WiFi networks scanned by the device through the BluFi protocol.
2. In `ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP`, the device stores the credentials into `SsidManager` (persisted in NVS by the `esp-wifi-connect` component).
3. The device then launches `WifiStation` to scan and connect; progress is reported back over BluFi.
4. If provisioning succeeds, the device connects to the new WiFi automatically. If it fails, an error status is sent back.

## Steps

1. **Configure**: turn on `Esp Blufi` in menuconfig, then build and flash the firmware.
2. **Trigger provisioning**: at first boot with no stored WiFi credentials the device enters provisioning automatically.
3. **Phone side**: open the EspBlufi app (or another BluFi client), scan and connect to the device, optionally enable encryption, then enter the WiFi SSID / password and send them.
4. **Observe the result**:
   - Success: BluFi reports success and the device connects to WiFi.
   - Failure: BluFi reports failure; retry or check the router.

## Notes

- BluFi cannot be used at the same time as hotspot provisioning. If hotspot provisioning is already enabled, the device will use it. Keep only one provisioning method in menuconfig.
- When running repeated tests, clear or overwrite the stored SSID (`wifi` NVS namespace) to avoid stale credentials interfering with the next run.
- If you write your own BluFi client, follow the official protocol frame format linked above.
- The EspBlufi app download links are listed in the official documentation.
- Because the BluFi API changed in IDF 5.5.2, firmware built with 5.5.2 advertises the Bluetooth name as `"Xiaozhi-Blufi"`, while 5.5.1 uses `"BLUFI_DEVICE"`.
