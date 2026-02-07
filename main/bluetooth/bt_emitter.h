#ifndef BT_EMITTER_H
#define BT_EMITTER_H

#include <string>
#include <vector>
#include <functional>
#include <driver/gpio.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUETOOTH_MODE_UART
#include <driver/uart.h>
#endif

/**
 * @brief Represents a discovered Bluetooth device
 */
struct BluetoothDevice {
    std::string name;           // Device name
    std::string mac_address;    // MAC address (12 hex chars, no colons)
    int rssi = 0;               // Signal strength if available
    bool is_connected = false;  // Currently connected
};

/**
 * @brief Bluetooth module state machine states
 */
enum class BluetoothState {
    Uninitialized,   // Not initialized yet
    Idle,            // Ready, not scanning or connecting
    Scanning,        // Actively scanning for devices
    Connecting,      // Attempting to connect
    Connected,       // Successfully connected to a device
    Disconnecting    // Disconnecting from device
};

/**
 * @brief KCX_BT_EMITTER Bluetooth Module Controller
 * 
 * Supports two modes:
 * - GPIO Mode: Basic connect/disconnect via GPIO pulses
 * - UART Mode: Full control via AT commands (scan, connect by MAC, etc.)
 */
class BtEmitter {
public:
    /**
     * @brief Get singleton instance
     */
    static BtEmitter& GetInstance();
    
    /**
     * @brief Initialize the Bluetooth module
     * @return true if initialization successful
     */
    bool Initialize();
    
    /**
     * @brief Check if module is initialized
     */
    bool IsInitialized() const { return initialized_; }
    
    // ========== GPIO-based control (Basic mode) ==========
    
    /**
     * @brief Enter pairing mode (short pulse 100ms)
     * Module will search for and connect to nearby devices
     */
    void EnterPairingMode();
    
    /**
     * @brief Disconnect and clear pairing memory (long pulse 3s)
     */
    void DisconnectAndClear();
    
    /**
     * @brief Check connection status via LINK pin
     * @return true if connected to a device
     */
    bool IsConnected();
    
    /**
     * @brief Get current state
     */
    BluetoothState GetState() const { return state_; }
    
    /**
     * @brief Get state as string
     */
    const char* GetStateString() const;
    
#ifdef CONFIG_BLUETOOTH_MODE_UART
    // ========== UART AT Command control (Advanced mode) ==========
    
    /**
     * @brief Send AT command and wait for response
     * @param cmd Command string (without \r\n)
     * @param response String to store the response
     * @param timeout_ms Timeout in milliseconds
     * @param expected_prefix Expected prefix for success (default "OK+")
     * @return true if command was successful (found expected_prefix)
     */
    bool SendCommand(const char* cmd, std::string& response, int timeout_ms = 1000, const char* expected_prefix = "OK+");
    
    /**
     * @brief Test communication with module
     * @return true if module responds
     */
    bool TestConnection();
    
    /**
     * @brief Reset the module
     */
    bool ResetModule();
    
    /**
     * @brief Get module firmware version
     */
    std::string GetVersion();
    
    // ----- Device Discovery -----
    
    /**
     * @brief Start scanning for Bluetooth devices
     * @param timeout_ms Scan timeout (default 8 seconds)
     * @return true if scan started successfully
     */
    bool StartScan(int timeout_ms = 8000);
    
    /**
     * @brief Stop ongoing scan
     */
    bool StopScan();
    
    /**
     * @brief Get list of scanned devices
     */
    std::vector<BluetoothDevice> GetScannedDevices() const { return scanned_devices_; }
    
    /**
     * @brief Clear scanned devices list
     */
    void ClearScannedDevices() { scanned_devices_.clear(); }
    
    // ----- Connection Management -----
    
    /**
     * @brief Connect to a specific device by MAC address
     * @param mac_address 12-character hex MAC address
     * @return true if connection initiated
     */
    bool ConnectToDevice(const std::string& mac_address);
    
    /**
     * @brief Connect to a device by name (from scan results)
     * @param name Device name to search and connect
     * @return true if device found and connection initiated
     */
    bool ConnectToDeviceByName(const std::string& name);
    
    /**
     * @brief Disconnect from current device
     */
    bool Disconnect();
    
    /**
     * @brief Get currently connected device info
     */
    BluetoothDevice GetConnectedDevice() const { return connected_device_; }
    
    // ----- Auto-Connect List Management -----
    
    /**
     * @brief Add device MAC to auto-connect list
     * @param mac_address 12-character hex MAC address
     * @return true if added successfully
     */
    bool AddToAutoConnect(const std::string& mac_address);
    
    /**
     * @brief Add device by name to auto-connect list
     * @param name Device name
     * @return true if added successfully
     */
    bool AddToAutoConnectByName(const std::string& name);
    
    /**
     * @brief Get all devices in auto-connect list
     */
    std::vector<std::string> GetAutoConnectList();
    
    /**
     * @brief Clear all auto-connect records
     */
    bool ClearAutoConnectList();
    
    /**
     * @brief Check connection status via AT command
     * @return 1=connected, 0=not connected, -1=error
     */
    int GetConnectionStatus();
    
    // ----- Volume Control -----
    
    /**
     * @brief Get current volume level
     * @return Volume 0-31, or -1 on error
     */
    int GetVolume();
    
    /**
     * @brief Set volume level
     * @param level Volume 0-31
     */
    bool SetVolume(int level);
    
#endif // CONFIG_BLUETOOTH_MODE_UART

    // ========== Callbacks ==========
    using StateCallback = std::function<void(BluetoothState old_state, BluetoothState new_state)>;
    void SetStateCallback(StateCallback cb) { state_callback_ = cb; }

private:
    BtEmitter();
    ~BtEmitter();
    
    // Prevent copying
    BtEmitter(const BtEmitter&) = delete;
    BtEmitter& operator=(const BtEmitter&) = delete;
    
    void SetState(BluetoothState new_state);
    
    bool initialized_ = false;
    BluetoothState state_ = BluetoothState::Uninitialized;
    StateCallback state_callback_ = nullptr;
    
#ifdef CONFIG_BLUETOOTH_MODE_UART
    uart_port_t uart_port_ = UART_NUM_2;
    std::vector<BluetoothDevice> scanned_devices_;
    BluetoothDevice connected_device_;
    
    bool ParseScanResponse(const std::string& response);
    std::string NormalizeMac(const std::string& mac);
#endif
};

#endif // BT_EMITTER_H
