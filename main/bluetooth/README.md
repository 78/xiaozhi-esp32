# KCX_BT_EMITTER Bluetooth Module Integration

Tài liệu này mô tả chi tiết cách triển khai và giải thuật scan/connect cho module phát Bluetooth KCX_BT_EMITTER V1.7.

---

## Mục Lục

- [Tổng Quan](#tổng-quan)
- [Cấu Trúc Files](#cấu-trúc-files)
- [Kconfig Configuration](#kconfig-configuration)
- [Kiến Trúc BtEmitter Component](#kiến-trúc-btemitter-component)
- [Giải Thuật Scan & Connect](#giải-thuật-scan--connect)
- [MCP Tools Reference](#mcp-tools-reference)
- [AT Commands Reference](#at-commands-reference)
- [Hướng Dẫn Sử Dụng](#hướng-dẫn-sử-dụng)

---

## Tổng Quan

### Module KCX_BT_EMITTER V1.7

Module Bluetooth 5.3 audio transmitter cho phép ESP32 phát audio đến loa/tai nghe Bluetooth.

**Đặc điểm kỹ thuật:**
- Bluetooth 5.3 với aptX HD, AAC, SBC codec
- Chế độ Transmitter (TX) - phát audio đến thiết bị BT
- Giao tiếp: GPIO (basic) hoặc UART AT Commands (advanced)  
- Điện áp: 3.3V logic, 5V power supply

### Vấn đề cần giải quyết

**Trước khi có implementation này:**
- Module tự động vào chế độ pairing khi bật nguồn → pair với bất kỳ thiết bị nào gần đó
- Không kiểm soát được thiết bị nào được kết nối
- Code Bluetooth bị duplicate ở nhiều board khác nhau
- Không có cơ chế scan, chọn lọc thiết bị

**Sau khi implement:**
- Kiểm soát hoàn toàn quá trình scan và connect
- Có thể chọn kết nối thiết bị cụ thể theo MAC address hoặc tên
- Lưu danh sách thiết bị yêu thích để auto-connect
- Cấu hình GPIO linh hoạt qua Kconfig

---

## Cấu Trúc Files

```
main/
├── bluetooth/
│   ├── bt_emitter.h          # Header: class BtEmitter declaration
│   └── bt_emitter.cc         # Implementation: GPIO + UART control
├── mcp_server.cc             # MCP tools registration 
├── CMakeLists.txt            # Build configuration
└── Kconfig.projbuild         # Menuconfig options

boards/iotforce-esp-puppy-s3/
├── config.h                  # Board-specific (BT pins moved to Kconfig)
└── esp_puppy_s3.cc          # Board init (BT code removed, now centralized)
```

---

## Kconfig Configuration

### Cách bật Bluetooth Module

```bash
idf.py menuconfig
```

Đi tới: **Xiaozhi Assistant → Bluetooth KCX_BT_EMITTER Module**

### Các Options

| Option | Type | Default | Mô tả |
|--------|------|---------|-------|
| `ENABLE_BLUETOOTH_MODULE` | bool | n | Bật/tắt module Bluetooth |
| `BLUETOOTH_MODE_GPIO` | choice | - | Chế độ GPIO đơn giản |
| `BLUETOOTH_MODE_UART` | choice | ✓ | Chế độ UART AT Commands (nâng cao) |
| `BLUETOOTH_CONNECT_PIN` | int | 3 | GPIO cho chân CONNECT |
| `BLUETOOTH_LINK_PIN` | int | 46 | GPIO cho chân LINK |
| `BLUETOOTH_UART_TX_PIN` | int | 9 | GPIO cho UART TX |
| `BLUETOOTH_UART_RX_PIN` | int | 10 | GPIO cho UART RX |
| `BLUETOOTH_UART_BAUD` | int | **115200** | UART baudrate (V1.7 requires 115200) |
| `BLUETOOTH_AUTO_CONNECT_ENABLED` | bool | y | Tự động connect khi khởi động |
| `BLUETOOTH_AUTO_CONNECT_COUNT` | int | 3 | Số thiết bị lưu auto-connect |

---

## Kiến Trúc BtEmitter Component

### Class Diagram

```
┌─────────────────────────────────────────────────────┐
│                    BtEmitter                         │
│              (Singleton Pattern)                     │
├─────────────────────────────────────────────────────┤
│ - initialized_: bool                                 │
│ - state_: BluetoothState                            │
│ - uart_port_: uart_port_t                           │
│ - scanned_devices_: vector<BluetoothDevice>         │
│ - connected_device_: BluetoothDevice                │
├─────────────────────────────────────────────────────┤
│ + GetInstance(): BtEmitter&                         │
│ + Initialize(): bool                                │
│ + IsInitialized(): bool                             │
│ + GetState(): BluetoothState                        │
│ + GetStateString(): const char*                     │
├─────────────────────────────────────────────────────┤
│ GPIO Mode (Basic):                                  │
│ + EnterPairingMode()                                │
│ + DisconnectAndClear()                              │
│ + IsConnected(): bool                               │
├─────────────────────────────────────────────────────┤
│ UART Mode (Advanced):                               │
│ + SendCommand(cmd, response, timeout): bool         │
│ + TestConnection(): bool                            │
│ + ResetModule(): bool                               │
│ + GetVersion(): string                              │
│ + StartScan(timeout): bool                          │
│ + StopScan(): bool                                  │
│ + GetScannedDevices(): vector<BluetoothDevice>      │
│ + ConnectToDevice(mac): bool                        │
│ + ConnectToDeviceByName(name): bool                 │
│ + Disconnect(): bool                                │
│ + AddToAutoConnect(mac): bool                       │
│ + AddToAutoConnectByName(name): bool                │
│ + GetAutoConnectList(): vector<string>              │
│ + ClearAutoConnectList(): bool                      │
│ + GetVolume(): int                                  │
│ + SetVolume(level): bool                            │
└─────────────────────────────────────────────────────┘
```

### State Machine

```
                    ┌─────────────┐
                    │UNINITIALIZED│
                    └──────┬──────┘
                           │ Initialize()
                           ▼
          ┌────────────┬───────┬────────────┐
          │            │ IDLE  │            │
          │            └───┬───┘            │
          │                │                │
    StartScan()      EnterPairingMode()   (LINK=HIGH)
          │                │                │
          ▼                ▼                ▼
     ┌─────────┐    ┌───────────┐    ┌───────────┐
     │SCANNING │    │CONNECTING │    │ CONNECTED │
     └────┬────┘    └─────┬─────┘    └─────┬─────┘
          │               │                │
     timeout/done    success/fail    Disconnect()
          │               │                │
          ▼               ▼                ▼
     ┌────────┐    ┌───────────┐    ┌──────────────┐
     │  IDLE  │    │CONNECTED/ │    │DISCONNECTING │
     └────────┘    │   IDLE    │    └──────┬───────┘
                   └───────────┘           │
                                           ▼
                                      ┌────────┐
                                      │  IDLE  │
                                      └────────┘
```

---

## Giải Thuật Scan & Connect

### Tổng Quan Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                    USER REQUEST                                  │
│         "Kết nối bluetooth với loa Sony"                        │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ STEP 1: MCP Server nhận request                                 │
│ → Gọi tool "self.bluetooth.scan" hoặc "self.bluetooth.connect"  │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ STEP 2: BtEmitter::StartScan()                                  │
│ → Gửi AT+SCAN qua UART                                          │
│ → Chờ response (timeout 8s)                                     │
│ → Parse response để lấy danh sách thiết bị                      │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ STEP 3: ParseScanResponse()                                     │
│ → Extract MACADD và NAME từ response                            │
│ → Lưu vào scanned_devices_ vector                               │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ STEP 4: MCP trả về danh sách devices                            │
│ → AI đọc và hiển thị cho user chọn                              │
│ → Hoặc AI tự match tên "Sony" trong danh sách                   │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ STEP 5: BtEmitter::ConnectToDevice(mac) hoặc ByName(name)       │
│ → Gửi AT+CONADD=<MAC> qua UART                                  │
│ → Chờ kết nối (timeout 5s)                                      │
│ → Kiểm tra LINK pin = HIGH → Connected                          │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ STEP 6: Thông báo kết quả                                       │
│ → Success: "Đã kết nối với Sony SRS-XB13"                       │
│ → Fail: "Không thể kết nối với thiết bị"                        │
└─────────────────────────────────────────────────────────────────┘
```

### Chi Tiết Giải Thuật StartScan()

```cpp
bool BtEmitter::StartScan(int timeout_ms) {
    // 1. Đặt state = Scanning
    SetState(BluetoothState::Scanning);
    
    // 2. Xóa danh sách cũ
    scanned_devices_.clear();
    
    // 3. Gửi AT+SCAN qua UART
    std::string response;
    bool success = SendCommand("AT+SCAN", response, timeout_ms);
    
    // 4. Parse response
    if (success) {
        ParseScanResponse(response);
    }
    
    // 5. Trở về Idle
    SetState(BluetoothState::Idle);
    
    return !scanned_devices_.empty();
}
```

### Chi Tiết Giải Thuật ParseScanResponse()

```
Response Format từ Module:
OK+SCAN NEWDEVICES:3 MACADD:1A2B3C4D5E6F NAME:Sony MACADD:AABBCCDDEEFF NAME:JBL MACADD:112233445566 NAME:AirPods

Parsing Algorithm:
```

```cpp
bool BtEmitter::ParseScanResponse(const std::string& response) {
    size_t pos = 0;
    
    while ((pos = response.find("MACADD:", pos)) != std::string::npos) {
        BluetoothDevice device;
        
        // Extract MAC (12 hex chars sau "MACADD:")
        size_t mac_start = pos + 7;
        device.mac_address = response.substr(mac_start, 12);
        
        // Validate MAC (chỉ chấp nhận hex)
        for (char c : device.mac_address) {
            if (!isxdigit(c)) continue; // skip invalid
        }
        
        // Extract NAME (nếu có)
        size_t name_pos = response.find("NAME:", pos);
        if (name_pos != std::string::npos) {
            size_t name_start = name_pos + 5;
            size_t name_end = response.find_first_of("\r\n ", name_start);
            device.name = response.substr(name_start, name_end - name_start);
        }
        
        // Lưu device vào danh sách
        scanned_devices_.push_back(device);
        pos = mac_start + 12;
    }
    
    return !scanned_devices_.empty();
}
```

### Chi Tiết Giải Thuật ConnectToDevice()

```cpp
bool BtEmitter::ConnectToDevice(const std::string& mac_address) {
    // 1. Normalize MAC (xóa dấu : và chuyển uppercase)
    std::string mac = NormalizeMac(mac_address);  // "1A:2B:3C" → "1A2B3C..."
    
    // 2. Đặt state = Connecting
    SetState(BluetoothState::Connecting);
    
    // 3. Gửi AT+CONADD=<MAC>
    std::string cmd = "AT+CONADD=" + mac;
    std::string response;
    SendCommand(cmd.c_str(), response, 5000);
    
    // 4. Chờ module thực hiện kết nối
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 5. Kiểm tra LINK pin
    if (IsConnected()) {
        SetState(BluetoothState::Connected);
        
        // Lưu thông tin device đang kết nối
        connected_device_.mac_address = mac;
        connected_device_.is_connected = true;
        
        // Tìm tên từ scan results
        for (const auto& dev : scanned_devices_) {
            if (dev.mac_address == mac) {
                connected_device_.name = dev.name;
                break;
            }
        }
        
        return true;
    }
    
    SetState(BluetoothState::Idle);
    return false;
}
```

### Giải Thuật ConnectToDeviceByName()

```cpp
bool BtEmitter::ConnectToDeviceByName(const std::string& name) {
    // Case-insensitive partial match trong scanned_devices_
    for (const auto& dev : scanned_devices_) {
        std::string dev_name_lower = toLower(dev.name);
        std::string search_lower = toLower(name);
        
        // Partial match: "sony" matches "Sony SRS-XB13"
        if (dev_name_lower.find(search_lower) != std::string::npos) {
            return ConnectToDevice(dev.mac_address);
        }
    }
    
    return false; // Không tìm thấy
}
```

---

## MCP Tools Reference

### GPIO Mode (Basic)

| Tool | Mô tả | Params |
|------|-------|--------|
| `self.bluetooth.pair` | Vào chế độ pairing (100ms pulse) | - |
| `self.bluetooth.disconnect` | Ngắt kết nối và xóa bộ nhớ (3s pulse) | - |
| `self.bluetooth.get_status` | Lấy trạng thái kết nối | - |

### UART Mode (Advanced) - Bổ sung

| Tool | Mô tả | Params |
|------|-------|--------|
| `self.bluetooth.scan` | Scan thiết bị gần đó | - |
| `self.bluetooth.connect_device` | Kết nối theo MAC hoặc tên | `mac_address` OR `name` |
| `self.bluetooth.disconnect_soft` | Ngắt kết nối, giữ bộ nhớ | - |
| `self.bluetooth.add_favorite` | Thêm vào auto-connect list | `mac_address` OR `name` |
| `self.bluetooth.clear_favorites` | Xóa auto-connect list | - |
| `self.bluetooth.get_favorites` | Lấy danh sách favorites | - |
| `self.bluetooth.set_volume` | Đặt volume 0-31 | `level` |
| `self.bluetooth.reset` | Reset module | - |

### Ví Dụ Response

**Scan:**
```json
{
  "success": true,
  "count": 3,
  "devices": [
    {"index": 0, "name": "Sony SRS-XB13", "mac": "1A2B3C4D5E6F"},
    {"index": 1, "name": "JBL Flip 5", "mac": "AABBCCDDEEFF"},
    {"index": 2, "name": "AirPods Pro", "mac": "112233445566"}
  ]
}
```

**Get Status:**
```json
{
  "state": "Connected",
  "connected": true,
  "device_name": "Sony SRS-XB13",
  "device_mac": "1A2B3C4D5E6F",
  "volume": 20
}
```

---

## AT Commands Reference

| Command | Mô tả | Response |
|---------|-------|----------|
| `AT+` | Test kết nối | `OK+` |
| `AT+REST` | Reset module | `OK+REST POWERON` |
| `AT+GMR?` | Lấy version | `OK+GMR:V1.7` |
| `AT+SCAN` | Scan thiết bị | `OK+SCAN NEWDEVICES:N MACADD:xxx NAME:yyy` |
| `AT+CONADD=<MAC>` | Kết nối theo MAC | `OK+CONADD` |
| `AT+DISCON` | Ngắt kết nối | `OK+DISCON DISCONNECTED` |
| `AT+STATUS` | Lấy trạng thái | `OK+STATUS:0` hoặc `OK+STATUS:1` |
| `AT+ADDLINKADD=<MAC>` | Thêm vào auto-connect | `OK+ADDLINKADD` |
| `AT+ADDLINKNAME=<NAME>` | Thêm theo tên | `OK+ADDLINKNAME` |
| `AT+VMLINK?` | Lấy auto-connect list | `OK+VMLINK:...` |
| `AT+CLEARLINK` | Xóa auto-connect list | `OK+CLEARLINK` |
| `AT+VOL?` | Lấy volume | `OK+VOL:20` |
| `AT+VOL=<0-31>` | Đặt volume | `OK+VOL:20` |

---

## Hướng Dẫn Sử Dụng

### 1. Cấu Hình

```bash
idf.py menuconfig
# Xiaozhi Assistant → Bluetooth KCX_BT_EMITTER Module
# [*] Enable KCX_BT_EMITTER Bluetooth Module
# (X) UART AT Commands (Advanced)
# GPIO Configuration → CONNECT Pin = 3, LINK Pin = 46
# UART Configuration → TX = 9, RX = 10, Baud = 115200
```

### 2. Kết Nối Phần Cứng

```
ESP32-S3          KCX_BT_EMITTER
---------         --------------
GPIO 3  ───────── CONNECT/PAIR
GPIO 46 ───────── LINK
GPIO 9  ───────── RX (module nhận)
GPIO 10 ───────── TX (module phát)
3.3V    ───────── VCC (hoặc 5V + level shifter)
GND     ───────── GND
```

### 3. Sử Dụng Qua Voice

**User:** "Kết nối bluetooth với loa Sony"

**AI Flow:**
1. Gọi `self.bluetooth.scan` → Lấy danh sách thiết bị
2. Tìm "Sony" trong danh sách
3. Gọi `self.bluetooth.connect_device` với MAC tương ứng
4. Trả lời: "Đã kết nối với Sony SRS-XB13"

**User:** "Ngắt kết nối bluetooth"

**AI Flow:**
1. Gọi `self.bluetooth.disconnect`
2. Trả lời: "Đã ngắt kết nối Bluetooth"

---

## Troubleshooting

| Vấn đề | Nguyên nhân | Giải pháp |
|--------|-------------|-----------|
| Module không phản hồi AT | Sai baudrate hoặc RX/TX đảo ngược | Kiểm tra dây, đổi TX/RX |
| Scan không thấy thiết bị | Thiết bị chưa bật discoverable | Bật chế độ pairing trên thiết bị đích |
| Kết nối không thành công | Thiết bị đã pair với thiết bị khác | Xóa pairing cũ trên cả 2 thiết bị |
| LINK pin luôn = 0 | Pull-up không hoạt động | Kiểm tra điện trở pull-up |

---

## References

- [KCX_BT_EMITTER Manual](https://manuals.plus/kcx/kcx-kcx_bt_emitter-wireless-bluetooth-audio-signal-transceiver-board-manual)
- [schreibfaul1/ESP32-KCX-BT-EMITTER Arduino Library](https://github.com/schreibfaul1/ESP32-KCX-BT-EMITTER)
- [ESP-IDF UART Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html)
