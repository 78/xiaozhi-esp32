/**
 * @file bt_emitter.cc
 * @brief KCX_BT_EMITTER Bluetooth Module Controller Implementation
 * 
 * Supports KCX_BT_EMITTER V1.7 Bluetooth 5.3 audio transmitter module.
 * Two control modes: GPIO (basic) and UART AT Commands (advanced)
 */

#include "bt_emitter.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <algorithm>
#include <cctype>

#define TAG "BT_EMITTER"

// AT Command response markers
#define AT_OK_PREFIX "OK+"
#define AT_ERR_PREFIX "ERR"

BtEmitter& BtEmitter::GetInstance() {
    static BtEmitter instance;
    return instance;
}

BtEmitter::BtEmitter() {
    ESP_LOGI(TAG, "BtEmitter instance created");
}

BtEmitter::~BtEmitter() {
#ifdef CONFIG_BLUETOOTH_MODE_UART
    if (initialized_) {
        uart_driver_delete(uart_port_);
    }
#endif
}

void BtEmitter::SetState(BluetoothState new_state) {
    if (state_ != new_state) {
        BluetoothState old_state = state_;
        state_ = new_state;
        ESP_LOGI(TAG, "State: %s -> %s", 
                 GetStateString(), 
                 (new_state == BluetoothState::Idle ? "Idle" :
                  new_state == BluetoothState::Scanning ? "Scanning" :
                  new_state == BluetoothState::Connecting ? "Connecting" :
                  new_state == BluetoothState::Connected ? "Connected" :
                  new_state == BluetoothState::Disconnecting ? "Disconnecting" : "Unknown"));
        
        if (state_callback_) {
            state_callback_(old_state, new_state);
        }
    }
}

const char* BtEmitter::GetStateString() const {
    switch (state_) {
        case BluetoothState::Uninitialized: return "Uninitialized";
        case BluetoothState::Idle: return "Idle";
        case BluetoothState::Scanning: return "Scanning";
        case BluetoothState::Connecting: return "Connecting";
        case BluetoothState::Connected: return "Connected";
        case BluetoothState::Disconnecting: return "Disconnecting";
        default: return "Unknown";
    }
}

bool BtEmitter::Initialize() {
#ifndef CONFIG_ENABLE_BLUETOOTH_MODULE
    ESP_LOGW(TAG, "Bluetooth module disabled in config");
    return false;
#else
    if (initialized_) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing KCX_BT_EMITTER module...");
    
    // ========== GPIO Configuration ==========
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    
    // CONNECT pin - Output, default HIGH (active LOW pulse to trigger)
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = (1ULL << CONFIG_BLUETOOTH_CONNECT_PIN);
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CONNECT pin: %d", err);
        return false;
    }
    gpio_set_level((gpio_num_t)CONFIG_BLUETOOTH_CONNECT_PIN, 1);  // Default HIGH
    
    // LINK pin - Input with pull-up
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = (1ULL << CONFIG_BLUETOOTH_LINK_PIN);
    err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LINK pin: %d", err);
        return false;
    }
    
    ESP_LOGI(TAG, "GPIO initialized: CONNECT=GPIO%d, LINK=GPIO%d",
             CONFIG_BLUETOOTH_CONNECT_PIN, CONFIG_BLUETOOTH_LINK_PIN);

#ifdef CONFIG_BLUETOOTH_MODE_UART
    // ========== UART Configuration ==========
    uart_config_t uart_config = {};
    uart_config.baud_rate = CONFIG_BLUETOOTH_UART_BAUD;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    
    // Install UART driver
    err = uart_driver_install(uart_port_, 1024, 1024, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %d", err);
        return false;
    }
    
    err = uart_param_config(uart_port_, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %d", err);
        uart_driver_delete(uart_port_);
        return false;
    }
    
    err = uart_set_pin(uart_port_, 
                       CONFIG_BLUETOOTH_UART_TX_PIN, 
                       CONFIG_BLUETOOTH_UART_RX_PIN,
                       UART_PIN_NO_CHANGE, 
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %d", err);
        uart_driver_delete(uart_port_);
        return false;
    }
    
    ESP_LOGI(TAG, "UART initialized: TX=GPIO%d, RX=GPIO%d, Baud=%d",
             CONFIG_BLUETOOTH_UART_TX_PIN, CONFIG_BLUETOOTH_UART_RX_PIN,
             CONFIG_BLUETOOTH_UART_BAUD);
    
    // Test communication
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for module to be ready
    if (TestConnection()) {
        ESP_LOGI(TAG, "Module communication OK");
        std::string version = GetVersion();
        if (!version.empty()) {
            ESP_LOGI(TAG, "Module version: %s", version.c_str());
        }
    } else {
        ESP_LOGW(TAG, "Module not responding to AT commands (may need power cycle)");
    }
#endif

    initialized_ = true;
    
    // Check initial connection status
    if (IsConnected()) {
        SetState(BluetoothState::Connected);
        ESP_LOGI(TAG, "Module initialized - Already connected to a device");
    } else {
        SetState(BluetoothState::Idle);
        SetState(BluetoothState::Idle);
        ESP_LOGI(TAG, "Module initialized - Not connected");
    }

#if !CONFIG_BLUETOOTH_AUTO_CONNECT_ENABLED
    // Explicitly disable auto-connect by clearing memory if configured to do so
    ESP_LOGI(TAG, "Auto-connect disabled in config, clearing VM links...");
    std::string response;
    // AT+DELVMLINK returns "Delete_Vmlink" not "OK+"
    SendCommand("AT+DELVMLINK", response, 1000, "Delete_Vmlink");
#endif
    
    return true;
#endif // CONFIG_ENABLE_BLUETOOTH_MODULE
}

void BtEmitter::EnterPairingMode() {
#ifdef CONFIG_ENABLE_BLUETOOTH_MODULE
    ESP_LOGI(TAG, "Entering pairing mode (100ms LOW pulse)");
    SetState(BluetoothState::Scanning);
    
    gpio_set_level((gpio_num_t)CONFIG_BLUETOOTH_CONNECT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)CONFIG_BLUETOOTH_CONNECT_PIN, 1);
    
    // Wait a bit and check if connected
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (IsConnected()) {
        SetState(BluetoothState::Connected);
    }
#endif
}

void BtEmitter::DisconnectAndClear() {
#ifdef CONFIG_ENABLE_BLUETOOTH_MODULE
    ESP_LOGI(TAG, "Disconnecting and clearing memory");
    SetState(BluetoothState::Disconnecting);
    
#ifdef CONFIG_BLUETOOTH_MODE_UART
    // Use AT command to clear all saved VM links (preferred method for V1.7)
    std::string response;
    // V1.7 returns "Delete_Vmlink"
    SendCommand("AT+DELVMLINK", response, 2000, "Delete_Vmlink");
    ESP_LOGI(TAG, "DELVMLINK response: %s", response.c_str());
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Reset module to apply changes (Reset returns OK+RESET)
    SendCommand("AT+RESET", response, 2000, "OK+RESET");
    ESP_LOGI(TAG, "RESET response: %s", response.c_str());
    
    connected_device_ = BluetoothDevice();
#else
    // GPIO mode: 3s LOW pulse to disconnect and clear
    gpio_set_level((gpio_num_t)CONFIG_BLUETOOTH_CONNECT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(3000));
    gpio_set_level((gpio_num_t)CONFIG_BLUETOOTH_CONNECT_PIN, 1);
#endif
    
    SetState(BluetoothState::Idle);
    ESP_LOGI(TAG, "Disconnect and clear complete");
#endif
}

bool BtEmitter::IsConnected() {
#ifdef CONFIG_ENABLE_BLUETOOTH_MODULE
    int level = gpio_get_level((gpio_num_t)CONFIG_BLUETOOTH_LINK_PIN);
    return (level == 1);
#else
    return false;
#endif
}

// ========== UART Mode Implementation ==========
#ifdef CONFIG_BLUETOOTH_MODE_UART

bool BtEmitter::SendCommand(const char* cmd, std::string& response, int timeout_ms, const char* expected_prefix) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Module not initialized");
        return false;
    }
    
    // Clear RX buffer
    uart_flush_input(uart_port_);
    
    // Send command with CR/LF
    std::string full_cmd = std::string(cmd) + "\r\n";
    int written = uart_write_bytes(uart_port_, full_cmd.c_str(), full_cmd.length());
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write command");
        return false;
    }
    
    ESP_LOGI(TAG, "TX: %s", cmd);
    
    // Read response with timeout
    char buffer[256];
    response.clear();
    
    int64_t start_time = esp_timer_get_time();
    while ((esp_timer_get_time() - start_time) / 1000 < timeout_ms) {
        int len = uart_read_bytes(uart_port_, buffer, sizeof(buffer) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            buffer[len] = '\0';
            response += buffer;
            
            // Check for expected response or error
            if (response.find(expected_prefix) != std::string::npos ||
                response.find(AT_ERR_PREFIX) != std::string::npos) {
                break;
            }
        }
    }
    
    // Trim whitespace
    while (!response.empty() && (response.back() == '\r' || response.back() == '\n')) {
        response.pop_back();
    }
    
    ESP_LOGI(TAG, "RX: %s", response.c_str());
    
    return response.find(expected_prefix) != std::string::npos;
}

bool BtEmitter::TestConnection() {
    std::string response;
    return SendCommand("AT+", response, 500);
}

bool BtEmitter::ResetModule() {
    std::string response;
    // V1.7 uses AT+RESET, not AT+REST
    bool success = SendCommand("AT+RESET", response, 2000);
    if (success) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for module to restart
        SetState(BluetoothState::Idle);
        ESP_LOGI(TAG, "Module reset complete");
    }
    return success;
}

std::string BtEmitter::GetVersion() {
    std::string response;
    if (SendCommand("AT+GMR?", response)) {
        // Response format: OK+GMR:Vx.x
        size_t pos = response.find("GMR:");
        if (pos != std::string::npos) {
            return response.substr(pos + 4);
        }
        return response;
    }
    return "";
}

bool BtEmitter::StartScan(int timeout_ms) {
    ESP_LOGI(TAG, "Starting device scan (timeout: %dms)", timeout_ms);
    SetState(BluetoothState::Scanning);
    scanned_devices_.clear();
    
    std::string response;
    // Send SCAN command
    // Note: V1.7 returns "SCAN" (or starts scanning), NOT "OK+SCAN"
    // So we assume success if we see "SCAN" or just don't get an error immediately
    // Or we just send the command and proceed to reading loop
    
    // Try to expect "SCAN" which is what reference lib sees
    if (!SendCommand("AT+SCAN", response, 500, "SCAN")) {
        // Even if we don't see "SCAN" immediately, we might still receive devices
        // So we log warning but continue to read loop
        ESP_LOGW(TAG, "Did not receive 'SCAN' confirmation, but proceeding to read...");
    }

    // Accumulate responses for the duration of the scan
    // V1.7 sends devices as separate lines: "MacAdd:..."
    int64_t start_time = esp_timer_get_time();
    char buffer[128];
    std::string line_buffer;

    while ((esp_timer_get_time() - start_time) / 1000 < timeout_ms) {
        int len = uart_read_bytes(uart_port_, buffer, sizeof(buffer) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            buffer[len] = '\0';
            line_buffer += buffer;
            
            // Process complete lines
            size_t pos;
            while ((pos = line_buffer.find('\n')) != std::string::npos) {
                std::string line = line_buffer.substr(0, pos);
                // Trim CR
                if (!line.empty() && line.back() == '\r') line.pop_back();
                
                if (!line.empty()) {
                    ESP_LOGD(TAG, "SCAN RX: %s", line.c_str());
                    ParseScanResponse(line);
                }
                
                line_buffer.erase(0, pos + 1);
            }
        }
    }
    
    ESP_LOGI(TAG, "Scan complete: found %d device(s)", (int)scanned_devices_.size());
    SetState(BluetoothState::Idle);
    return !scanned_devices_.empty();
}

bool BtEmitter::StopScan() {
    // Send any command to interrupt scan
    std::string response;
    SendCommand("AT+", response, 500);
    SetState(BluetoothState::Idle);
    return true;
}

std::string BtEmitter::NormalizeMac(const std::string& mac) {
    // Remove colons and convert to uppercase
    std::string result;
    for (char c : mac) {
        if (c != ':' && c != '-') {
            result += toupper(c);
        }
    }
    
    // Ensure 12 characters (pad with leading zeros if needed)
    while (result.length() < 12) {
        result = "0" + result;
    }
    
    return result.substr(0, 12);
}

bool BtEmitter::ParseScanResponse(const std::string& response) {
    // Response format examples V1.7:
    // MacAdd:1A2B3C4D5E6F Name:Speaker1
    // Note: Reference lib checks for "MacAdd", my old code checked "MACADD:"
    
    // Check for "MacAdd" or "MACADD" (case insensitive match)
    std::string upper_resp = response;
    std::transform(upper_resp.begin(), upper_resp.end(), upper_resp.begin(), ::toupper);
    
    size_t mac_pos = upper_resp.find("MACADD");
    if (mac_pos == std::string::npos) {
        return false;
    }
    
    BluetoothDevice device;
    
    // Extract MAC (skip "MACADD" or "MACADD:" or "MacAdd")
    size_t mac_start = mac_pos + 6; 
    if (mac_start < response.length() && response[mac_start] == ':') mac_start++;
    
    if (mac_start + 12 <= response.length()) {
        device.mac_address = response.substr(mac_start, 12);
        
        // Validate MAC
        bool valid_mac = true;
        for (char c : device.mac_address) {
            if (!isxdigit(c)) { valid_mac = false; break; }
        }
        
        if (!valid_mac) return false;
        
        // Extract NAME
        size_t name_pos = upper_resp.find("NAME:", mac_start + 12);
        if (name_pos != std::string::npos) {
            size_t name_start = name_pos + 5;
            device.name = response.substr(name_start);
            
            // Trim whitespace
            while (!device.name.empty() && isspace(device.name.back())) device.name.pop_back();
            while (!device.name.empty() && isspace(device.name.front())) device.name.erase(0, 1);
        }
        
        // Duplicate check
        bool exists = false;
        for (const auto& d : scanned_devices_) {
            if (d.mac_address == device.mac_address) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            ESP_LOGI(TAG, "Found device: '%s' [%s]", 
                     device.name.empty() ? "Unknown" : device.name.c_str(),
                     device.mac_address.c_str());
            scanned_devices_.push_back(device);
            return true;
        }
    }
    
    return false;
}

bool BtEmitter::ConnectToDevice(const std::string& mac_address) {
    std::string mac = NormalizeMac(mac_address);
    
    ESP_LOGI(TAG, "Connecting to device: %s", mac.c_str());
    SetState(BluetoothState::Connecting);
    
    std::string cmd = "AT+CONADD=" + mac;
    std::string response;
    
    bool success = SendCommand(cmd.c_str(), response, 5000);
    
    // Wait for connection to establish
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    if (IsConnected()) {
        SetState(BluetoothState::Connected);
        connected_device_.mac_address = mac;
        
        // Try to find name from scan results
        for (const auto& dev : scanned_devices_) {
            if (dev.mac_address == mac) {
                connected_device_.name = dev.name;
                break;
            }
        }
        connected_device_.is_connected = true;
        
        ESP_LOGI(TAG, "Connected successfully to %s", mac.c_str());
        return true;
    }
    
    SetState(BluetoothState::Idle);
    ESP_LOGW(TAG, "Connection failed");
    return false;
}

bool BtEmitter::ConnectToDeviceByName(const std::string& name) {
    // Search in scanned devices
    for (const auto& dev : scanned_devices_) {
        // Case-insensitive partial match
        std::string dev_name_lower = dev.name;
        std::string search_lower = name;
        std::transform(dev_name_lower.begin(), dev_name_lower.end(), dev_name_lower.begin(), ::tolower);
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
        
        if (dev_name_lower.find(search_lower) != std::string::npos) {
            ESP_LOGI(TAG, "Found matching device: '%s' [%s]", dev.name.c_str(), dev.mac_address.c_str());
            return ConnectToDevice(dev.mac_address);
        }
    }
    
    ESP_LOGW(TAG, "Device '%s' not found in scan results", name.c_str());
    return false;
}

bool BtEmitter::Disconnect() {
    ESP_LOGI(TAG, "Disconnecting from current device");
    SetState(BluetoothState::Disconnecting);
    
    std::string response;
    // V1.7: Use AT+DELVMLINK to disconnect and clear memory
    // This prevents auto-reconnection to the device
    // Expect "Delete_Vmlink"
    bool success = SendCommand("AT+DELVMLINK", response, 2000, "Delete_Vmlink");
    ESP_LOGI(TAG, "Disconnect command response: %s", response.c_str());
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Also reset module to ensure clean state
    SendCommand("AT+RESET", response, 2000, "OK+RESET");
    
    connected_device_ = BluetoothDevice();
    SetState(BluetoothState::Idle);
    
    return success;
}

bool BtEmitter::AddToAutoConnect(const std::string& mac_address) {
    std::string mac = NormalizeMac(mac_address);
    std::string cmd = "AT+ADDLINKADD=" + mac;
    std::string response;
    
    bool success = SendCommand(cmd.c_str(), response);
    if (success) {
        ESP_LOGI(TAG, "Added %s to auto-connect list", mac.c_str());
    }
    return success;
}

bool BtEmitter::AddToAutoConnectByName(const std::string& name) {
    std::string cmd = "AT+ADDLINKNAME=" + name;
    std::string response;
    
    bool success = SendCommand(cmd.c_str(), response);
    if (success) {
        ESP_LOGI(TAG, "Added '%s' to auto-connect list by name", name.c_str());
    }
    return success;
}

std::vector<std::string> BtEmitter::GetAutoConnectList() {
    std::vector<std::string> list;
    std::string response;
    
    if (SendCommand("AT+VMLINK?", response)) {
        // Parse response to extract stored devices
        // Format varies by firmware version
        // Try to extract MAC addresses
        size_t pos = 0;
        while ((pos = response.find("VMLINK", pos)) != std::string::npos) {
            // Skip to potential MAC address
            pos += 6;
            // Look for 12 hex chars
            size_t start = pos;
            while (pos < response.length() && !isxdigit(response[pos])) pos++;
            
            if (pos + 12 <= response.length()) {
                std::string mac = response.substr(pos, 12);
                bool valid = true;
                for (char c : mac) {
                    if (!isxdigit(c)) { valid = false; break; }
                }
                if (valid) {
                    list.push_back(mac);
                    pos += 12;
                }
            }
        }
    }
    
    return list;
}

bool BtEmitter::ClearAutoConnectList() {
    std::string response;
    // V1.7 returns "Delete_Vmlink"
    bool success = SendCommand("AT+CLEARLINK", response, 1000, "Delete_Vmlink");
    if (success) {
        ESP_LOGI(TAG, "Auto-connect list cleared");
    }
    return success;
}

int BtEmitter::GetConnectionStatus() {
    std::string response;
    if (SendCommand("AT+STATUS", response)) {
        // Response: OK+STATUS:0 or OK+STATUS:1
        size_t pos = response.find("STATUS:");
        if (pos != std::string::npos && pos + 8 <= response.length()) {
            char status = response[pos + 7];
            return (status == '1') ? 1 : 0;
        }
    }
    return -1;
}

int BtEmitter::GetVolume() {
    std::string response;
    if (SendCommand("AT+VOL?", response)) {
        // Response: OK+VOL:XX
        size_t pos = response.find("VOL:");
        if (pos != std::string::npos) {
            return atoi(response.c_str() + pos + 4);
        }
    }
    return -1;
}

bool BtEmitter::SetVolume(int level) {
    if (level < 0) level = 0;
    if (level > 31) level = 31;
    
    std::string cmd = "AT+VOL=" + std::to_string(level);
    std::string response;
    return SendCommand(cmd.c_str(), response);
}

#endif // CONFIG_BLUETOOTH_MODE_UART
