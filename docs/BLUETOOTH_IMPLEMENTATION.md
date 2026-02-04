# Bluetooth Implementation Report

## Overview

Bluetooth Low Energy (BLE) has been enabled on the LUNA ESP32-S3 device for WiFi provisioning via the ESP-BluFi protocol.

## Configuration Changes

### sdkconfig

```
CONFIG_BT_ENABLED=y
CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING=y
```

### Binary Size Impact

| Build | Size | Free Space |
|-------|------|------------|
| Without BT | 0x2b2f60 (2.83 MB) | 31% free |
| With BT | 0x301330 (3.15 MB) | 24% free |

**Additional RAM usage**: ~80KB for Bluetooth stack

## How BluFi Works

1. **Trigger**: BluFi activates when device enters WiFi config mode (no saved/working WiFi credentials)
2. **Discovery**: Device advertises as BLE peripheral with name based on device ID
3. **Pairing**: Mobile app connects via BLE and sends WiFi credentials securely
4. **Connection**: Device receives SSID/password and connects to WiFi
5. **Cleanup**: BluFi resources released after successful WiFi connection

## Code Flow

```
wifi_board.cc:
  └─ EnterConfigMode()
       └─ #ifdef CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING
            └─ Blufi::GetInstance().init()  // Start BLE advertising

  └─ OnNetworkEvent(Connected)
       └─ Blufi::GetInstance().deinit()     // Release BLE resources
```

## BluFi Protocol

- **Encryption**: AES-128 for secure credential transfer
- **Authentication**: DH key exchange
- **Fragmentation**: Large packets split for BLE MTU limits

## Testing BluFi

To test Bluetooth provisioning:

1. Remove hardcoded WiFi or use invalid credentials
2. Device will fail WiFi scan and enter config mode
3. BluFi will initialize and start BLE advertising
4. Use ESP BluFi app (iOS/Android) or LUNA mobile app to configure

## Mobile App Integration

The LUNA mobile app should implement BluFi client:

```dart
// Flutter example using flutter_blue_plus
class BlufiProvisioning {
  Future<void> configureWifi(String ssid, String password) async {
    // 1. Scan for LUNA device (name starts with "Luna-")
    // 2. Connect via BLE
    // 3. Negotiate encryption keys
    // 4. Send WiFi credentials
    // 5. Wait for confirmation
  }
}
```

## Current Behavior

| Scenario | Behavior |
|----------|----------|
| WiFi credentials valid | Connects immediately, BluFi not started |
| WiFi credentials invalid | Enters config mode, BluFi starts |
| No saved credentials | Enters config mode, BluFi starts |

## Fallback Methods

The device supports multiple provisioning methods (priority order):

1. **Hardcoded credentials** (development only)
2. **BluFi** (BLE provisioning) - NOW ENABLED
3. **Hotspot** (WiFi AP mode) - `Luna-XXXX` network
4. **Acoustic** (disabled) - Sound-based provisioning

## Security Considerations

- BluFi uses encrypted channel for credential transfer
- Device MAC address visible during BLE advertising
- Consider adding PIN/passkey for pairing in production

## Resources

- [ESP-IDF BluFi Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi-security.html#wi-fi-provisioning)
- [ESP BluFi Android App](https://github.com/EspressifApp/EspBlufiForAndroid)
- [ESP BluFi iOS App](https://github.com/EspressifApp/EspBlufiForiOS)

---

**Date**: February 4, 2026
**Firmware Version**: 2.2.2
**ESP-IDF Version**: v5.5.2
