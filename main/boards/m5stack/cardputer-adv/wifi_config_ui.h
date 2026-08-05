#ifndef WIFI_CONFIG_UI_H
#define WIFI_CONFIG_UI_H

#include "tca8418_keyboard.h"
#include "display/lcd_display.h"
#include <string>
#include <vector>
#include <functional>

// WiFi scan result structure
struct WifiScanResult {
    std::string ssid;
    int8_t rssi;
    bool is_encrypted;
};

// WiFi configuration UI state machine
enum class WifiConfigState {
    Scanning,       // Scanning for WiFi networks
    SelectWifi,     // Selecting from WiFi list
    InputPassword,  // Entering password for selected WiFi
    InputSsid,      // Manual SSID input
    InputManualPwd, // Manual password input (after SSID)
    SavedList,      // Viewing saved WiFi list
    Connecting,     // Connecting to WiFi
    Success,        // Connection successful
    Failed          // Connection failed
};

// Result of WiFi configuration
enum class WifiConfigResult {
    None,           // Still in progress
    Connected,      // Successfully connected
    Cancelled       // User cancelled
};

class WifiConfigUI {
public:
    using ConnectCallback = std::function<void(const std::string& ssid, const std::string& password)>;

    WifiConfigUI(LcdDisplay* display);
    ~WifiConfigUI();

    // Start the WiFi configuration UI
    void Start();

    // Start directly with saved WiFi list
    void StartWithSavedList();

    // Handle keyboard events, returns result
    WifiConfigResult HandleKeyEvent(const KeyEvent& event);

    // Set callback for when connection should be attempted
    void SetConnectCallback(ConnectCallback callback) { connect_callback_ = callback; }

    // Notify connection result
    void OnConnectResult(bool success);

    // Check if UI is active
    bool IsActive() const { return is_active_; }

    // Update cursor blink state (call periodically from main loop)
    void UpdateCursor();

private:
    LcdDisplay* display_;
    WifiConfigState state_;
    bool is_active_;
    ConnectCallback connect_callback_;

    // WiFi scan results
    std::vector<WifiScanResult> scan_results_;
    int selected_index_;
    int scroll_offset_;

    // Saved WiFi list
    std::vector<std::pair<std::string, std::string>> saved_wifi_list_;
    int saved_selected_index_;
    int saved_scroll_offset_;

    // Input buffers
    std::string input_ssid_;
    std::string input_password_;
    std::string selected_ssid_;
    bool input_focus_on_password_;  // For manual input: true = password field, false = ssid field

    // Cursor blinking
    bool cursor_visible_;
    uint32_t last_cursor_toggle_;
    static constexpr uint32_t CURSOR_BLINK_MS = 500;

    // Display constants
    static constexpr int MAX_VISIBLE_ITEMS = 4;
    static constexpr int MAX_INPUT_LENGTH = 64;

    // State handlers
    void StartScanning();
    void ShowScanResults();
    void ShowPasswordInput();
    void ShowManualInput();
    void ShowSavedList();
    void ShowConnecting();
    void ShowSuccess();
    void ShowFailed();

    // Redraw functions (don't reset state/input)
    void RedrawPasswordInput();
    void RedrawManualInput();

    // Input handlers
    void HandleScanningKey(const KeyEvent& event);
    void HandleSelectWifiKey(const KeyEvent& event);
    void HandlePasswordInputKey(const KeyEvent& event);
    void HandleManualInputKey(const KeyEvent& event);
    void HandleSavedListKey(const KeyEvent& event);
    void HandleConnectingKey(const KeyEvent& event);
    void HandleResultKey(const KeyEvent& event);

    // Helper functions
    void DrawHeader(const char* title);
    void DrawFooter(const char* hint);
    void DrawInputField(const char* label, const std::string& value, bool is_password, bool is_active);
    void DrawWifiList(const std::vector<WifiScanResult>& list, int selected, int scroll_offset);
    void DrawSavedWifiList();
    std::string GetSignalBars(int8_t rssi);
    void LoadSavedWifiList();
    void SaveWifiCredentials(const std::string& ssid, const std::string& password);
    void DeleteSavedWifi(int index);
    void DoWifiScan();
    void AttemptConnection();
};

#endif // WIFI_CONFIG_UI_H
