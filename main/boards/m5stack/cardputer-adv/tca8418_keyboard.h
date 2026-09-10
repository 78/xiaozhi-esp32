#ifndef TCA8418_KEYBOARD_H
#define TCA8418_KEYBOARD_H

#include "i2c_device.h"
#include <functional>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// TCA8418 Register definitions
#define TCA8418_REG_CFG             0x01
#define TCA8418_REG_INT_STAT        0x02
#define TCA8418_REG_KEY_LCK_EC      0x03
#define TCA8418_REG_KEY_EVENT_A     0x04
#define TCA8418_REG_KP_GPIO_1       0x1D
#define TCA8418_REG_KP_GPIO_2       0x1E
#define TCA8418_REG_KP_GPIO_3       0x1F

// Config register bits
#define TCA8418_CFG_KE_IEN          0x01  // Key events interrupt enable

// Modifier key masks
enum KeyModifier {
    KEY_MOD_NONE   = 0x00,
    KEY_MOD_SHIFT  = 0x01,
    KEY_MOD_CTRL   = 0x02,
    KEY_MOD_ALT    = 0x04,
    KEY_MOD_OPT    = 0x08,
};

// HID-compatible key codes
enum KeyCode {
    KC_NONE = 0x00,
    KC_A = 0x04,
    KC_B = 0x05,
    KC_C = 0x06,
    KC_D = 0x07,
    KC_E = 0x08,
    KC_F = 0x09,
    KC_G = 0x0A,
    KC_H = 0x0B,
    KC_I = 0x0C,
    KC_J = 0x0D,
    KC_K = 0x0E,
    KC_L = 0x0F,
    KC_M = 0x10,
    KC_N = 0x11,
    KC_O = 0x12,
    KC_P = 0x13,
    KC_Q = 0x14,
    KC_R = 0x15,
    KC_S = 0x16,
    KC_T = 0x17,
    KC_U = 0x18,
    KC_V = 0x19,
    KC_W = 0x1A,
    KC_X = 0x1B,
    KC_Y = 0x1C,
    KC_Z = 0x1D,
    KC_1 = 0x1E,
    KC_2 = 0x1F,
    KC_3 = 0x20,
    KC_4 = 0x21,
    KC_5 = 0x22,
    KC_6 = 0x23,
    KC_7 = 0x24,
    KC_8 = 0x25,
    KC_9 = 0x26,
    KC_0 = 0x27,
    KC_ENTER = 0x28,
    KC_ESC = 0x29,
    KC_BACKSPACE = 0x2A,
    KC_TAB = 0x2B,
    KC_SPACE = 0x2C,
    KC_MINUS = 0x2D,
    KC_EQUAL = 0x2E,
    KC_LBRACKET = 0x2F,
    KC_RBRACKET = 0x30,
    KC_BACKSLASH = 0x31,
    KC_SEMICOLON = 0x33,
    KC_APOSTROPHE = 0x34,
    KC_GRAVE = 0x35,
    KC_COMMA = 0x36,
    KC_DOT = 0x37,
    KC_SLASH = 0x38,
    KC_CAPSLOCK = 0x39,
    KC_RIGHT = 0x4F,
    KC_LEFT = 0x50,
    KC_DOWN = 0x51,
    KC_UP = 0x52,
    KC_LSHIFT = 0xE1,
    KC_LCTRL = 0xE0,
    KC_LALT = 0xE2,
    KC_LOPT = 0xE3,
};

// Key event structure with full information
struct KeyEvent {
    bool pressed;           // true = pressed, false = released
    bool is_modifier;       // true if this is a modifier key
    uint8_t key_code;       // HID key code (KeyCode enum)
    const char* key_char;   // Character representation (e.g., "a", "A", "1", "!")
};

// Legacy key codes for backward compatibility
enum LegacyKeyCode {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ENTER,
    KEY_OTHER
};

class Tca8418Keyboard : public I2cDevice {
public:
    using KeyCallback = std::function<void(LegacyKeyCode key)>;
    using KeyEventCallback = std::function<void(const KeyEvent& event)>;

    Tca8418Keyboard(i2c_master_bus_handle_t i2c_bus, uint8_t addr, gpio_num_t int_pin);
    ~Tca8418Keyboard();

    void Initialize();
    void SetKeyCallback(KeyCallback callback) { key_callback_ = callback; }
    void SetKeyEventCallback(KeyEventCallback callback) { key_event_callback_ = callback; }

    // Get current modifier state
    uint8_t GetModifierMask() const { return modifier_mask_; }
    bool IsShiftPressed() const { return (modifier_mask_ & KEY_MOD_SHIFT) != 0; }
    bool IsCapsLockOn() const { return caps_lock_on_; }

private:
    gpio_num_t int_pin_;
    KeyCallback key_callback_;
    KeyEventCallback key_event_callback_;
    TaskHandle_t task_handle_ = nullptr;
    volatile bool isr_flag_ = false;
    uint8_t modifier_mask_ = 0;
    bool caps_lock_on_ = false;
    uint64_t key_state_mask_ = 0;  // 4x14 logical keys, bit=1 means pressed

    void ConfigureMatrix();
    void EnableInterrupts();
    void FlushEvents();
    uint8_t GetEvent();
    LegacyKeyCode MapLegacyKeyCode(uint8_t row, uint8_t col);
    KeyEvent MapKeyEvent(uint8_t row, uint8_t col, bool pressed);
    void UpdateModifierState(uint8_t row, uint8_t col, bool pressed);

    static void IRAM_ATTR GpioIsrHandler(void* arg);
    static void KeyboardTask(void* arg);
};

#endif // TCA8418_KEYBOARD_H
