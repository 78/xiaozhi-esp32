# Flash Partition Table Reference — `ostb-xiaozhi-3st` (16MB Flash)

This document contains the exact partition layout dumped from the physical flash memory (`0x8000`) of the **D-Solution / OSTB Xiaozhi 2-Mic (2" Touchscreen, SKU: `ostb-xiaozhi-3st`)**.

---

## Partition Table Summary

- **Total Flash Size:** 16 MB (`0x1000000` bytes)
- **Application Slots:** Dual 5.25 MB OTA slots (`ota_0` & `ota_1`)
- **Assets Storage:** 5.375 MB SPIFFS partition (`assets`) for fonts, wake-word models, and UI emojis

| Partition Name | Type | SubType | Start Offset | Size (Hex) | Size (Dec / Human) | Flash Address Range | Description |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **`nvs`** | `data` | `nvs` | `0x00009000` | `0x4000` | 16 KB (16,384 B) | `0x009000` – `0x00D000` | Non-Volatile Storage (WiFi, config, alarms) |
| **`otadata`** | `data` | `ota` | `0x0000D000` | `0x2000` | 8 KB (8,192 B) | `0x00D000` – `0x00F000` | OTA boot partition selector |
| **`phy_init`** | `data` | `phy` | `0x0000F000` | `0x1000` | 4 KB (4,096 B) | `0x00F000` – `0x010000` | RF PHY calibration data |
| **`ota_0`** | `app` | `ota_0` | **`0x00020000`** | `0x540000` | 5,376 KB (5.25 MB) | `0x020000` – `0x560000` | Primary app slot |
| **`ota_1`** | `app` | `ota_1` | **`0x00560000`** | `0x540000` | 5,376 KB (5.25 MB) | `0x560000` – `0xAA0000` | Secondary app slot (OTA updates) |
| **`assets`** | `data` | `spiffs` | **`0x00AA0000`** | `0x560000` | 5,504 KB (5.375 MB) | `0xAA0000` – `0x1000000` | SPIFFS storage (assets/models) |

---

## ESP-IDF Partition Table CSV Format

Save this as `partitions.csv` or reference in your project:

```csv
# ESP-IDF Partition Table
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   16K,
otadata,  data, ota,     0xd000,   8K,
phy_init, data, phy,     0xf000,   4K,
ota_0,    app,  ota_0,   0x20000,  5376K,
ota_1,    app,  ota_1,   0x560000, 5376K,
assets,   data, spiffs,  0xaa0000, 5504K,
```

---

## Memory Map Diagram

```
+-------------------------------------------------------------+ 0x00000000
| Bootloader / Header Padding                                 |
+-------------------------------------------------------------+ 0x00008000
| Partition Table (4KB)                                       |
+-------------------------------------------------------------+ 0x00009000
| NVS (16KB)                                                  |
+-------------------------------------------------------------+ 0x0000D000
| OTA Data (8KB)                                              |
+-------------------------------------------------------------+ 0x0000F000
| PHY Init (4KB)                                              |
+-------------------------------------------------------------+ 0x00020000
| OTA_0 App Partition (5.25 MB)                               |
+-------------------------------------------------------------+ 0x00560000
| OTA_1 App Partition (5.25 MB)                               |
+-------------------------------------------------------------+ 0x00AA0000
| Assets SPIFFS Partition (5.375 MB)                          |
+-------------------------------------------------------------+ 0x01000000 (16 MB)
```
